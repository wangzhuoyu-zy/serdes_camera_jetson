/*
 * isx031_intrinsics_tool.c — ISX031 内参读写 CLI
 *
 * 子命令:
 *   read   从一个或多个 Sensor 读取内参, 输出 JSON / OpenCV YAML
 *   write  写入单字段或整段 (--field fx --value 1234.5 或 --from-yaml)
 *   dump   导出 Flash raw 字节到 bin 文件
 *   erase  擦除一个或多个 Sector (4096B/个)
 *   scan   扫描 I2C 总线, 列出 candidate Sensor
 *   verify 回读校验写入的字段是否一致 (--expected-yaml)
 *
 * 示例:
 *   ./isx031_intrinsics_tool /dev/i2c-9 read --addr 0x3A --format json
 *   ./isx031_intrinsics_tool /dev/i2c-9 read --addr 0x3A,0x3B,0x3C,0x3D -o cam.yaml
 *   ./isx031_intrinsics_tool /dev/i2c-9 write --addr 0x3A --field fx --value 1234.5 --verify
 *   ./isx031_intrinsics_tool /dev/i2c-9 write --addr 0x3A,0x3B --from-yaml batch.yaml --verify
 *   ./isx031_intrinsics_tool /dev/i2c-9 dump  --addr 0x3A --start 0xC200 --length 104 -o int.bin
 *   ./isx031_intrinsics_tool /dev/i2c-9 erase --addr 0x3A --start 0xC000 --count 4
 *   ./isx031_intrinsics_tool /dev/i2c-9 scan
 *
 * 目标平台: Jetson Orin (ARM64 Linux, Ubuntu 22.04)
 */

#include "isx031_flash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <libgen.h>
#include <ctype.h>

/* ------------------------------------------------------------------ */
/* 简易 JSON / YAML 输出 (无外部依赖)                                  */
/* ------------------------------------------------------------------ */

/* (intentionally no json_escape: all output keys/values are numeric, no escaping needed) */

/* 把 isx031_intrinsics_t 内字段按 enum 取地址 (用于模型无关遍历) */
static const double *cli_field_ptr(const isx031_intrinsics_t *v, isx031_field_t f)
{
    switch (f) {
        case ISX031_FIELD_FX:    return &v->fx;
        case ISX031_FIELD_FY:    return &v->fy;
        case ISX031_FIELD_CX:    return &v->cx;
        case ISX031_FIELD_CY:    return &v->cy;
        case ISX031_FIELD_K1:    return &v->k1;
        case ISX031_FIELD_K2:    return &v->k2;
        case ISX031_FIELD_K3:    return &v->k3;
        case ISX031_FIELD_K4:    return &v->k4;
        case ISX031_FIELD_K5:    return &v->k5;
        case ISX031_FIELD_K6:    return &v->k6;
        case ISX031_FIELD_P1:    return &v->p1;
        case ISX031_FIELD_P2:    return &v->p2;
        case ISX031_FIELD_REPRO: return &v->repro;
        default:                 return NULL;
    }
}

/* JSON 输出: 按模型字段顺序输出 */
static void json_print_intrinsics(const isx031_intrinsics_t *v,
                                  isx031_model_t m, FILE *out)
{
    const isx031_model_layout_t *L = isx031_get_layout(m);
    if (!L || L->n_fields == 0) {
        fputs("{}", out);
        return;
    }
    fputc('{', out);
    for (int i = 0; i < L->n_fields; i++) {
        const double *p = cli_field_ptr(v, L->fields[i]);
        if (!p) continue;
        fprintf(out, "%s\"%s\": %.17g",
                i == 0 ? "" : ", ",
                L->field_name[i] ? L->field_name[i] : ISX031_FIELD_NAMES[L->fields[i]],
                *p);
    }
    fputc('}', out);
}

static void yaml_print_intrinsics(const isx031_intrinsics_t *v,
                                  isx031_model_t m, FILE *out)
{
    const isx031_model_layout_t *L = isx031_get_layout(m);
    if (!L || L->n_fields == 0) return;
    for (int i = 0; i < L->n_fields; i++) {
        const double *p = cli_field_ptr(v, L->fields[i]);
        if (!p) continue;
        fprintf(out, "    %s: %.17g\n",
                L->field_name[i] ? L->field_name[i] : ISX031_FIELD_NAMES[L->fields[i]],
                *p);
    }
}

/* OpenCV FileStorage 兼容 YAML: 输出 K (3x3) + D (1xN)
 * - Pinhole: D = [k1, k2, p1, p2, k3] (5 项, OpenCV 标准顺序)
 * - Fisheye: D = [k1, k2, k3, k4]    (4 项, OpenCV cv::fisheye 标准顺序)
 */
static void opencv_yaml_print_intrinsics(const isx031_intrinsics_t *v,
                                         isx031_model_t m, FILE *out)
{
    fprintf(out, "  K: !!opencv-matrix\n"
                 "    rows: 3\n"
                 "    cols: 3\n"
                 "    dt: d\n"
                 "    data: [%.17g, 0., %.17g, "
                 "0., %.17g, %.17g, "
                 "0., 0., 1.]\n",
            v->fx, v->cx, v->fy, v->cy);

    if (m == ISX031_MODEL_H200) {
        /* Fisheye 4 参数 (OpenCV fisheye::distortCoeffs) */
        fprintf(out, "  D: !!opencv-matrix\n"
                     "    rows: 1\n"
                     "    cols: 4\n"
                     "    dt: d\n"
                     "    data: [%.17g, %.17g, %.17g, %.17g]\n",
                v->k1, v->k2, v->k3, v->k4);
    } else {
        /* Pinhole 5 参数 (OpenCV 标准径向+切向畸变) */
        fprintf(out, "  D: !!opencv-matrix\n"
                     "    rows: 1\n"
                     "    cols: 5\n"
                     "    dt: d\n"
                     "    data: [%.17g, %.17g, %.17g, %.17g, %.17g]\n",
                v->k1, v->k2, v->p1, v->p2, v->k3);
    }
}

static void hex_dump_bytes(const unsigned char *b, unsigned int len, FILE *out)
{
    for (unsigned int i = 0; i < len; i++) {
        if (i && (i % 32) == 0) fputc('\n', out);
        fprintf(out, "%02x", b[i]);
    }
    fputc('\n', out);
}

/* ------------------------------------------------------------------ */
/* 命令行参数解析                                                      */
/* ------------------------------------------------------------------ */

typedef struct {
    unsigned char addrs[16];
    int           n_addrs;
    bool          all_default;
    int           bus;        /* I2C 总线号 (informational) */
    const char   *dev_path;   /* /dev/i2c-N 路径 */
    bool          auto_detect; /* --auto: 从 Sensor 读 Model ID 自动选择 */
    isx031_model_t model;     /* --model 指定 */
} common_opts_t;

/* 字符串 → Model ID */
static int parse_model_token(const char *s, isx031_model_t *out)
{
    if (!s || !*s) return -1;
    /* 数字形式 (1..6) */
    char *end = NULL;
    long v = strtol(s, &end, 0);
    if (end != s && *end == '\0' && v >= 1 && v <= 6) {
        *out = (isx031_model_t)v;
        return 0;
    }
    /* 名称形式 */
    if (!strcmp(s, "h200") || !strcmp(s, "H200")) { *out = ISX031_MODEL_H200;    return 0; }
    if (!strcmp(s, "h69")  || !strcmp(s, "H69"))  { *out = ISX031_MODEL_H69;     return 0; }
    if (!strcmp(s, "h120") || !strcmp(s, "H120")) { *out = ISX031_MODEL_H120;    return 0; }
    if (!strcmp(s, "h64")  || !strcmp(s, "H64"))  { *out = ISX031_MODEL_H64;     return 0; }
    if (!strcmp(s, "h100") || !strcmp(s, "H100")) { *out = ISX031_MODEL_H100;    return 0; }
    if (!strcmp(s, "h30")  || !strcmp(s, "H30"))  { *out = ISX031_MODEL_H30;     return 0; }
    return -1;
}

typedef enum {
    FMT_JSON,
    FMT_YAML,
    FMT_OPENCV_YAML,
} out_fmt_t;

static int parse_addr_token(const char *tok, unsigned char *out)
{
    if (!tok || !*tok) return -1;
    char *end = NULL;
    unsigned long v = strtoul(tok, &end, 0);
    if (end == tok) return -1;
    if (v > 0x7F) return -1;
    *out = (unsigned char)v;
    return 0;
}

static int parse_addr_list(const char *s, common_opts_t *o)
{
    o->n_addrs = 0;
    if (!s) return -1;
    char *dup = strdup(s);
    if (!dup) return -1;
    char *save = NULL;
    for (char *tok = strtok_r(dup, ",", &save); tok;
         tok = strtok_r(NULL, ",", &save)) {
        while (*tok && isspace((unsigned char)*tok)) tok++;
        size_t L = strlen(tok);
        while (L && isspace((unsigned char)tok[L-1])) tok[--L] = 0;
        if (o->n_addrs >= (int)(sizeof(o->addrs)/sizeof(o->addrs[0]))) {
            free(dup);
            return -1;
        }
        if (parse_addr_token(tok, &o->addrs[o->n_addrs]) < 0) {
            ISX031_ERR("invalid I2C address: '%s'", tok);
            free(dup);
            return -1;
        }
        o->n_addrs++;
    }
    free(dup);
    return (o->n_addrs > 0) ? 0 : -1;
}

static void resolve_addrs(common_opts_t *o)
{
    if (o->all_default) {
        o->n_addrs = ISX031_DEFAULT_DESER_ADDR_COUNT;
        for (int i = 0; i < o->n_addrs; i++)
            o->addrs[i] = ISX031_DEFAULT_DESER_ADDRS[i];
    } else if (o->n_addrs == 0) {
        /* 默认解串器地址 */
        o->n_addrs = ISX031_DEFAULT_DESER_ADDR_COUNT;
        for (int i = 0; i < o->n_addrs; i++)
            o->addrs[i] = ISX031_DEFAULT_DESER_ADDRS[i];
        ISX031_WARN("--addr 未指定, 使用默认解串器地址 0x3A..0x3D");
    }
}

/* ------------------------------------------------------------------ */
/* sub-command: read                                                   */
/* ------------------------------------------------------------------ */

static int do_read_addr(int fd, unsigned char addr,
                        const common_opts_t *o,
                        uint32_t base, unsigned int length,
                        out_fmt_t fmt, FILE *out,
                        bool first)
{
    isx031_model_t m;
    isx031_meta_t  meta;
    isx031_intrinsics_t v;
    unsigned char *raw = NULL;

    /* 1) 确定 model */
    if (o->auto_detect) {
        m = isx031_detect_model(fd, addr);
        if (m == ISX031_MODEL_UNKNOWN) {
            ISX031_WARN("0x%02X: 自动探测失败, 默认按 H120 处理", addr);
            m = ISX031_MODEL_H120;
        }
    } else {
        m = o->model;
    }

    const isx031_model_layout_t *L = isx031_get_layout(m);
    if (!L) {
        ISX031_ERR("0x%02X: 无效 model=%d", addr, m);
        return -1;
    }

    /* 2) 读取元数据 (sw_version / model_id / SN) — 失败不致命 */
    memset(&meta, 0, sizeof(meta));
    if (isx031_read_meta(fd, addr, &meta) < 0) {
        ISX031_WARN("0x%02X: meta read failed (继续)", addr);
    }
    if (o->auto_detect && meta.model != ISX031_MODEL_UNKNOWN)
        m = meta.model;

    /* 3) 读取内参 */
    unsigned int need = (unsigned int)L->n_fields * ISX031_FIELD_SIZE;
    if (length != 0 && length < need) {
        /* 用户显式传了 --length 且小于该模型所需字节 */
        need = length;
    }
    raw = (unsigned char *)malloc(need);
    if (!raw) return -1;
    if (isx031_flash_read(fd, addr, base, need, raw) < 0) {
        free(raw);
        return -1;
    }

    isx031_parse_intrinsics(raw, need, m, &v);

    /* 4) 输出 */
    if (fmt == FMT_JSON) {
        if (!first) fprintf(out, ",\n");
        fprintf(out, "  \"0x%02X\": {\n", addr);
        fprintf(out, "    \"model\": \"%s\",\n", isx031_model_name(m));
        fprintf(out, "    \"model_id\": \"0x%02X\",\n", meta.model_id);
        fprintf(out, "    \"sw_version\": \"%s\",\n", meta.sw_version);
        fprintf(out, "    \"sn\": \"%s\",\n", meta.sn);
        fprintf(out, "    \"base\": \"0x%04X\",\n", base);
        fprintf(out, "    \"raw_hex\": \"");
        hex_dump_bytes(raw, need, out);
        fprintf(out, "    \"crc16\": \"0x%04X\",\n",
                isx031_crc16_ccitt(raw, need, 0xFFFF));
        fprintf(out, "    \"intrinsics\": ");
        json_print_intrinsics(&v, m, out);
        fprintf(out, "\n  }");
    } else if (fmt == FMT_YAML || fmt == FMT_OPENCV_YAML) {
        fprintf(out, "0x%02X:\n", addr);
        if (fmt == FMT_OPENCV_YAML) {
            fprintf(out, "  model: %s\n", isx031_model_name(m));
            fprintf(out, "  sw_version: %s\n", meta.sw_version);
            fprintf(out, "  sn: %s\n", meta.sn);
            opencv_yaml_print_intrinsics(&v, m, out);
        } else {
            fprintf(out, "  model: %s\n", isx031_model_name(m));
            fprintf(out, "  sw_version: %s\n", meta.sw_version);
            fprintf(out, "  sn: %s\n", meta.sn);
            fprintf(out, "  base: 0x%04X\n", base);
            fprintf(out, "  intrinsics:\n");
            yaml_print_intrinsics(&v, m, out);
        }
    }
    free(raw);
    return 0;
}

static int cmd_read(int fd, int argc, char **argv)
{
    common_opts_t o = {0};
    o.model = ISX031_MODEL_H120;   /* 默认 H120 (兼容旧行为) */
    o.auto_detect = false;
    out_fmt_t fmt = FMT_JSON;
    uint32_t base = ISX031_INTRINSIC_BASE_ADDR;
    unsigned int length = 0;   /* 0 = 按 model 自动 */
    const char *out_path = NULL;
    bool crc = false;
    FILE *out = stdout;

    static struct option opts[] = {
        {"addr",       required_argument, 0, 'a'},
        {"all-default",no_argument,       0, 'A'},
        {"auto",       no_argument,       0, 'X'},
        {"model",      required_argument, 0, 'M'},
        {"base",       required_argument, 0, 'b'},
        {"length",     required_argument, 0, 'l'},
        {"format",     required_argument, 0, 'f'},
        {"out",        required_argument, 0, 'o'},
        {"crc",        no_argument,       0, 'c'},
        {0,0,0,0},
    };

    int c;
    optind = 1;
    while ((c = getopt_long(argc, argv, "a:AXM:b:l:f:o:c", opts, NULL)) != -1) {
        switch (c) {
            case 'a': if (parse_addr_list(optarg, &o) < 0) return 2; break;
            case 'A': o.all_default = true; break;
            case 'X': o.auto_detect = true; break;
            case 'M':
                if (parse_model_token(optarg, &o.model) < 0) {
                    ISX031_ERR("invalid --model '%s' (h200/h69/h120/h64/h100/h30 或 1..6)", optarg);
                    return 2;
                }
                break;
            case 'b': base   = (uint32_t)strtoul(optarg, NULL, 0); break;
            case 'l': length = (unsigned int)strtoul(optarg, NULL, 0); break;
            case 'f':
                if      (!strcmp(optarg, "json"))        fmt = FMT_JSON;
                else if (!strcmp(optarg, "yaml"))        fmt = FMT_YAML;
                else if (!strcmp(optarg, "opencv-yaml")) fmt = FMT_OPENCV_YAML;
                else { ISX031_ERR("unknown --format '%s'", optarg); return 2; }
                break;
            case 'o': out_path = optarg; break;
            case 'c': crc = true; break;
            default:  return 2;
        }
    }
    resolve_addrs(&o);
    if (out_path) {
        out = fopen(out_path, "w");
        if (!out) { ISX031_ERR("open %s: %s", out_path, strerror(errno)); return 1; }
    }

    int rc = 0;
    if (fmt == FMT_JSON) fprintf(out, "{\n");
    if (fmt == FMT_OPENCV_YAML) fprintf(out, "%%YAML:1.0\n---\n");

    for (int i = 0; i < o.n_addrs; i++) {
        if (do_read_addr(fd, o.addrs[i], &o, base, length, fmt, out, i == 0) < 0) {
            ISX031_ERR("read 0x%02X failed", o.addrs[i]);
            rc = 2;
        }
    }

    if (fmt == FMT_JSON) {
        fprintf(out, (crc ? ",\n  \"crc_global\": \"%04X\"\n" : ""), 0);
        fprintf(out, "}\n");
    }

    if (out != stdout) {
        fclose(out);
        /* 若读取完全失败, 删除残留的空文件 */
        if (rc != 0 && out_path) {
            unlink(out_path);
        }
    }
    return rc;
}

/* ------------------------------------------------------------------ */
/* sub-command: write                                                  */
/* ------------------------------------------------------------------ */

static int field_name_to_enum(const char *name)
{
    for (int i = 0; i < ISX031_TOTAL_FIELDS; i++) {
        if (strcmp(name, ISX031_FIELD_NAMES[i]) == 0)
            return i;
    }
    return -1;
}

/* 极简 YAML 解析: 仅支持形如
 *   fx: 1234.5
 *   k1: -0.01
 * 的扁平 key: value, 跳过空行与 '#' 注释
 */
static int parse_simple_yaml(const char *path, isx031_intrinsics_t *out)
{
    FILE *fp = fopen(path, "r");
    if (!fp) { ISX031_ERR("open %s: %s", path, strerror(errno)); return -1; }

    char line[512];
    int n = 0;
    while (fgets(line, sizeof(line), fp)) {
        char *p = line;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p == '#' || *p == '\n' || *p == '\0') continue;
        char *colon = strchr(p, ':');
        if (!colon) continue;
        *colon = 0;
        char *key = p;
        char *val = colon + 1;
        while (*val && isspace((unsigned char)*val)) val++;
        size_t L = strlen(val);
        while (L && (val[L-1] == '\n' || val[L-1] == '\r' || isspace((unsigned char)val[L-1]))) {
            val[--L] = 0;
        }
        int idx = field_name_to_enum(key);
        if (idx < 0) continue;
        double *fields[ISX031_TOTAL_FIELDS] = {
            &out->fx, &out->fy, &out->cx, &out->cy,
            &out->k1, &out->k2, &out->k3, &out->k4, &out->k5, &out->k6,
            &out->p1, &out->p2, &out->repro,
        };
        *fields[idx] = strtod(val, NULL);
        n++;
    }
    fclose(fp);
    return (n > 0) ? 0 : -1;
}

static int do_write_field(int fd, unsigned char addr,
                          int field_enum, double value, bool verify)
{
    if (isx031_write_field(fd, addr, (isx031_field_t)field_enum, value) < 0) {
        ISX031_ERR("write field '%s' on 0x%02X failed",
                   ISX031_FIELD_NAMES[field_enum], addr);
        return -1;
    }
    if (verify) {
        isx031_intrinsics_t rb;
        /* verify 按 Pinhole 读 (兼容历史), 因为 verify 关心单字段, 不影响 */
        unsigned char raw[ISX031_TOTAL_BYTES];
        if (isx031_read_intrinsics(fd, addr, ISX031_MODEL_H120, &rb, raw,
                                   ISX031_TOTAL_BYTES) < 0) {
            ISX031_ERR("verify readback failed on 0x%02X", addr);
            return -1;
        }
        const double *fields[ISX031_TOTAL_FIELDS] = {
            &rb.fx, &rb.fy, &rb.cx, &rb.cy,
            &rb.k1, &rb.k2, &rb.k3, &rb.k4, &rb.k5, &rb.k6,
            &rb.p1, &rb.p2, &rb.repro,
        };
        double got = *fields[field_enum];
        if (got != value) {
            ISX031_ERR("verify MISMATCH 0x%02X %s: got %.17g expected %.17g",
                       addr, ISX031_FIELD_NAMES[field_enum], got, value);
            return -1;
        }
        ISX031_INFO("verify OK 0x%02X %s = %.17g", addr,
                    ISX031_FIELD_NAMES[field_enum], got);
    }
    return 0;
}

static int cmd_write(int fd, int argc, char **argv)
{
    common_opts_t o = {0};
    o.model = ISX031_MODEL_H120;
    o.auto_detect = false;
    const char *field = NULL;
    const char *value_str = NULL;
    const char *from_yaml = NULL;
    bool verify = false;

    static struct option opts[] = {
        {"addr",       required_argument, 0, 'a'},
        {"all-default",no_argument,       0, 'A'},
        {"auto",       no_argument,       0, 'X'},
        {"model",      required_argument, 0, 'M'},
        {"field",      required_argument, 0, 'f'},
        {"value",      required_argument, 0, 'v'},
        {"from-yaml",  required_argument, 0, 'y'},
        {"verify",     no_argument,       0, 'c'},
        {0,0,0,0},
    };

    int c;
    optind = 1;
    while ((c = getopt_long(argc, argv, "a:AXM:f:v:y:c", opts, NULL)) != -1) {
        switch (c) {
            case 'a': if (parse_addr_list(optarg, &o) < 0) return 2; break;
            case 'A': o.all_default = true; break;
            case 'X': o.auto_detect = true; break;
            case 'M':
                if (parse_model_token(optarg, &o.model) < 0) {
                    ISX031_ERR("invalid --model '%s'", optarg);
                    return 2;
                }
                break;
            case 'f': field = optarg; break;
            case 'v': value_str = optarg; break;
            case 'y': from_yaml = optarg; break;
            case 'c': verify = true; break;
            default:  return 2;
        }
    }
    resolve_addrs(&o);

    int rc = 0;

    if (field && value_str) {
        int fe = field_name_to_enum(field);
        if (fe < 0) {
            ISX031_ERR("unknown field '%s'", field);
            return 2;
        }
        if (o.n_addrs != 1) {
            ISX031_ERR("--field/--value requires a single --addr");
            return 2;
        }
        double v = strtod(value_str, NULL);
        if (do_write_field(fd, o.addrs[0], fe, v, verify) < 0)
            rc = 2;
    } else if (from_yaml) {
        isx031_intrinsics_t vals;
        memset(&vals, 0, sizeof(vals));
        if (parse_simple_yaml(from_yaml, &vals) < 0) {
            ISX031_ERR("failed to parse --from-yaml %s", from_yaml);
            return 2;
        }
        for (int i = 0; i < o.n_addrs; i++) {
            /* 写: 按 model (auto 或显式) 写入 */
            isx031_model_t wm = o.auto_detect ? isx031_detect_model(fd, o.addrs[i]) : o.model;
            if (wm == ISX031_MODEL_UNKNOWN) wm = ISX031_MODEL_H120;
            if (isx031_write_intrinsics(fd, o.addrs[i], wm, &vals) < 0) {
                ISX031_ERR("write intrinsics 0x%02X failed", o.addrs[i]);
                rc = 2;
                continue;
            }
            if (verify) {
                isx031_intrinsics_t rb;
                unsigned char raw[ISX031_TOTAL_BYTES];
                if (isx031_read_intrinsics(fd, o.addrs[i], wm, &rb, raw,
                                           ISX031_TOTAL_BYTES) < 0) {
                    ISX031_ERR("verify readback 0x%02X failed", o.addrs[i]);
                    rc = 2;
                    continue;
                }
                if (memcmp(&rb, &vals, sizeof(rb)) != 0) {
                    ISX031_ERR("verify MISMATCH 0x%02X", o.addrs[i]);
                    rc = 2;
                } else {
                    ISX031_INFO("verify OK 0x%02X", o.addrs[i]);
                }
            }
        }
    } else {
        ISX031_ERR("需要 --field/--value 或 --from-yaml");
        return 2;
    }
    return rc;
}

/* ------------------------------------------------------------------ */
/* sub-command: dump                                                    */
/* ------------------------------------------------------------------ */

static int cmd_dump(int fd, int argc, char **argv)
{
    common_opts_t o = {0};
    uint32_t start = 0;
    unsigned int length = 0;
    const char *out_path = NULL;

    static struct option opts[] = {
        {"addr",   required_argument, 0, 'a'},
        {"start",  required_argument, 0, 's'},
        {"len",    required_argument, 0, 'l'},
        {"out",    required_argument, 0, 'o'},
        {0,0,0,0},
    };

    int c;
    optind = 1;
    while ((c = getopt_long(argc, argv, "a:s:l:o:", opts, NULL)) != -1) {
        switch (c) {
            case 'a': if (parse_addr_list(optarg, &o) < 0) return 2; break;
            case 's': start  = (uint32_t)strtoul(optarg, NULL, 0); break;
            case 'l': length = (unsigned int)strtoul(optarg, NULL, 0); break;
            case 'o': out_path = optarg; break;
            default:  return 2;
        }
    }
    if (length == 0 || !out_path) {
        ISX031_ERR("dump 需要 --start --len --out");
        return 2;
    }
    if (o.n_addrs == 0) { o.addrs[0] = 0x3A; o.n_addrs = 1; }

    unsigned char *buf = (unsigned char *)malloc(length);
    if (!buf) { ISX031_ERR("malloc(%u) failed", length); return 1; }

    int n = isx031_flash_read(fd, o.addrs[0], start, length, buf);
    if (n < 0) {
        ISX031_ERR("dump read failed");
        free(buf);
        return 2;
    }
    FILE *fp = fopen(out_path, "wb");
    if (!fp) { ISX031_ERR("open %s: %s", out_path, strerror(errno)); free(buf); return 1; }
    fwrite(buf, 1, length, fp);
    fclose(fp);
    free(buf);

    fprintf(stdout, "{\"addr\":\"0x%02X\",\"start\":\"0x%04X\",\"length\":%u,\"out\":\"%s\"}\n",
            o.addrs[0], start, length, out_path);
    return 0;
}

/* ------------------------------------------------------------------ */
/* sub-command: erase                                                   */
/* ------------------------------------------------------------------ */

static int cmd_erase(int fd, int argc, char **argv)
{
    common_opts_t o = {0};
    uint32_t start = ISX031_INTRINSIC_BASE_ADDR;
    unsigned int count = 1;

    static struct option opts[] = {
        {"addr",  required_argument, 0, 'a'},
        {"start", required_argument, 0, 's'},
        {"count", required_argument, 0, 'c'},
        {0,0,0,0},
    };

    int c;
    optind = 1;
    while ((c = getopt_long(argc, argv, "a:s:c:", opts, NULL)) != -1) {
        switch (c) {
            case 'a': if (parse_addr_list(optarg, &o) < 0) return 2; break;
            case 's': start = (uint32_t)strtoul(optarg, NULL, 0); break;
            case 'c': count = (unsigned int)strtoul(optarg, NULL, 0); break;
            default:  return 2;
        }
    }
    if (o.n_addrs == 0) { o.addrs[0] = 0x3A; o.n_addrs = 1; }

    int rc = 0;
    for (int i = 0; i < o.n_addrs; i++) {
        for (unsigned int k = 0; k < count; k++) {
            uint32_t addr = start + (uint32_t)k * ISX031_SECTOR_SIZE;
            ISX031_INFO("erase 0x%02X sector 0x%05X ...", o.addrs[i], addr);
            if (isx031_flash_sector_erase(fd, o.addrs[i], addr) < 0) {
                ISX031_ERR("erase 0x%05X on 0x%02X failed", addr, o.addrs[i]);
                rc = 2;
            }
        }
        isx031_flash_wait_erase_done(fd, o.addrs[i], 0);
    }
    return rc;
}

/* ------------------------------------------------------------------ */
/* sub-command: scan                                                    */
/* ------------------------------------------------------------------ */

static int cmd_scan(int fd, int argc, char **argv)
{
    (void)argc; (void)argv;
    unsigned char addrs[128];
    int n = isx031_i2c_scan(fd, addrs, 128);
    if (n < 0) { ISX031_ERR("scan failed"); return 1; }

    fprintf(stdout, "{\n  \"found\": %d,\n  \"addrs\": [", n);
    for (int i = 0; i < n; i++) {
        fprintf(stdout, "%s\"0x%02X\"", (i == 0 ? "" : ", "), addrs[i]);
    }
    fprintf(stdout, "],\n  \"default_map\": [");
    for (int i = 0; i < ISX031_DEFAULT_DESER_ADDR_COUNT; i++) {
        fprintf(stdout, "%s\"0x%02X\"", (i == 0 ? "" : ", "),
                ISX031_DEFAULT_DESER_ADDRS[i]);
    }
    fprintf(stdout, "]\n}\n");
    return 0;
}

/* ------------------------------------------------------------------ */
/* sub-command: verify                                                  */
/* ------------------------------------------------------------------ */

static int cmd_verify(int fd, int argc, char **argv)
{
    common_opts_t o = {0};
    const char *expected_yaml = NULL;
    bool first = true;

    static struct option opts[] = {
        {"addr",          required_argument, 0, 'a'},
        {"all-default",   no_argument,       0, 'A'},
        {"expected-yaml", required_argument, 0, 'e'},
        {0,0,0,0},
    };

    int c;
    optind = 1;
    while ((c = getopt_long(argc, argv, "a:Ae:", opts, NULL)) != -1) {
        switch (c) {
            case 'a': if (parse_addr_list(optarg, &o) < 0) return 2; break;
            case 'A': o.all_default = true; break;
            case 'e': expected_yaml = optarg; break;
            default:  return 2;
        }
    }
    resolve_addrs(&o);
    if (!expected_yaml) {
        ISX031_ERR("--expected-yaml 必填");
        return 2;
    }
    isx031_intrinsics_t expected;
    memset(&expected, 0, sizeof(expected));
    if (parse_simple_yaml(expected_yaml, &expected) < 0) return 2;

    int rc = 0;
    fprintf(stdout, "{\n");
    for (int i = 0; i < o.n_addrs; i++) {
        isx031_intrinsics_t got;
        unsigned char raw[ISX031_TOTAL_BYTES];
        /* 按 model 读, 优先 auto, 否则用 H120 (verify 默认布局) */
        isx031_model_t vm = isx031_detect_model(fd, o.addrs[i]);
        if (vm == ISX031_MODEL_UNKNOWN) vm = ISX031_MODEL_H120;
        if (isx031_read_intrinsics(fd, o.addrs[i], vm, &got, raw,
                                   ISX031_TOTAL_BYTES) < 0) {
            rc = 2;
            continue;
        }
        bool ok = (memcmp(&got, &expected, sizeof(got)) == 0);
        if (!ok) rc = 2;
        if (!first) fprintf(stdout, ",\n");
        first = false;
        fprintf(stdout, "  \"0x%02X\": {\"ok\": %s, \"model\": \"%s\", \"intrinsics\": ",
                o.addrs[i], ok ? "true" : "false", isx031_model_name(vm));
        json_print_intrinsics(&got, vm, stdout);
        fprintf(stdout, "}");
    }
    fprintf(stdout, "\n}\n");
    return rc;
}

/* ------------------------------------------------------------------ */
/* 帮助                                                                  */
/* ------------------------------------------------------------------ */

static void disp_help(const char *prog)
{
    fprintf(stdout,
"isx031_intrinsics_tool — ISX031 内部 Flash 内参读写 CLI\n"
"\n"
"用法:\n"
"  %s <i2c_dev> <command> [options]\n"
"\n"
"参数:\n"
"  <i2c_dev>    I2C 总线设备, 如 /dev/i2c-9\n"
"\n"
"命令:\n"
"  read         读取内参, 输出 JSON / YAML / OpenCV YAML\n"
"  write        写入单字段或整段 (--field fx --value 1234.5 或 --from-yaml)\n"
"  dump         导出 Flash raw 字节到文件\n"
"  erase        擦除一个或多个 Sector (4096B/个)\n"
"  scan         扫描 I2C 总线 (0x03..0x77)\n"
"  verify       回读校验 (--expected-yaml)\n"
"  -h, --help   显示帮助\n"
"\n"
"通用选项:\n"
"  --addr ADDR[,ADDR...]   7-bit I2C 地址, 如 0x3A,0x3B\n"
"  --all-default           使用默认解串器地址 0x3A..0x3D\n"
"  --auto                  从 Sensor 的 Model ID (0xC106) 自动检测型号\n"
"  --model MODEL           显式指定型号: h200 | h69 | h120 | h64 | h100 | h30 (或 1..6)\n"
"                         (H200=Fisheye 模型; 其他=Pinhole 模型, 默认 H120)\n"
"  --verbose               打印所有 I2C 报文字节级细节 (调试用)\n"
"\n"
"read 选项:\n"
"  --base ADDR     起始 Flash 地址 (默认 0x%04X)\n"
"  --length N      读取字节数 (默认按 model 自动)\n"
"  --format FMT    json | yaml | opencv-yaml (默认 json)\n"
"  --out PATH      写入文件 (默认 stdout)\n"
"  --crc           附加 CRC16-CCITT\n"
"  --base 注: H200 起始 0xC200 长度 %d 字节 (Fisheye)\n"
"          其他型号 起始 0xC200 长度 %d 字节 (Pinhole)\n"
"\n"
"write 选项:\n"
"  --field NAME --value V         单字段写入\n"
"  --from-yaml PATH               从 YAML 文件整段写入\n"
"  --verify                       写入后回读校验\n"
"\n"
"dump 选项:\n"
"  --start ADDR  --len N  --out PATH\n"
"\n"
"erase 选项:\n"
"  --start ADDR  --count N    从 ADDR 起擦 N 个 Sector\n"
"\n"
"示例:\n"
"  %s /dev/i2c-9 read --addr 0x3A --format opencv-yaml -o cam.yaml\n"
"  %s /dev/i2c-9 read --auto --all-default --format json\n"
"  %s /dev/i2c-9 read --model h200 --addr 0x3A --format opencv-yaml\n"
"  %s /dev/i2c-9 write --addr 0x3A --field fx --value 1234.5 --verify\n"
"  %s /dev/i2c-9 write --all-default --from-yaml batch.yaml --verify\n"
"  %s /dev/i2c-9 dump  --addr 0x3A --start 0xC200 --len 104 -o int.bin\n"
"  %s /dev/i2c-9 erase --addr 0x3A --start 0xC000 --count 4\n"
"  %s /dev/i2c-9 scan\n"
"\n",
        prog,
        ISX031_INTRINSIC_BASE_ADDR, ISX031_H200_BYTES, ISX031_TOTAL_BYTES,
        prog, prog, prog, prog, prog, prog, prog, prog);
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv_orig)
{
    if (argc < 2 ||
        (strcmp(argv_orig[1], "-h") == 0 || strcmp(argv_orig[1], "--help") == 0)) {
        disp_help(argc > 0 ? argv_orig[0] : "isx031_intrinsics_tool");
        return (argc < 2) ? 1 : 0;
    }

    /* 构造过滤后的 argv (去掉 --verbose), 供子命令 getopt 使用 */
    char **argv = (char **)malloc((argc + 1) * sizeof(char *));
    if (!argv) { perror("malloc"); return 1; }
    int new_argc = 0;
    argv[new_argc++] = argv_orig[0];
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv_orig[i], "--verbose") == 0) {
            isx031_debug_i2c = 1;
            continue;  /* 不传给子命令 getopt */
        }
        argv[new_argc++] = argv_orig[i];
    }
    argv[new_argc] = NULL;

    if (new_argc < 3) {
        ISX031_ERR("缺少子命令");
        disp_help(argv_orig[0]);
        free(argv);
        return 2;
    }
    /* dev_path 是过滤后 argv[1], cmd 是 argv[2] */
    const char *dev_path = argv[1];
    const char *cmd      = argv[2];

    if (strcmp(cmd, "-h") == 0 || strcmp(cmd, "--help") == 0 ||
        strcmp(cmd, "help") == 0) {
        disp_help(argv_orig[0]);
        free(argv);
        return 0;
    }

    int fd = isx031_i2c_open(dev_path);
    if (fd < 0) { free(argv); return 1; }

    /* 把 i2c_dev 路径透传给子命令 (前两个位置参数已是 dev/cmd) */
    int sub_argc = new_argc - 2;
    char **sub_argv = argv + 2;

    int rc = 1;
    if      (!strcmp(cmd, "read"))   rc = cmd_read  (fd, sub_argc, sub_argv);
    else if (!strcmp(cmd, "write"))  rc = cmd_write (fd, sub_argc, sub_argv);
    else if (!strcmp(cmd, "dump"))   rc = cmd_dump  (fd, sub_argc, sub_argv);
    else if (!strcmp(cmd, "erase"))  rc = cmd_erase (fd, sub_argc, sub_argv);
    else if (!strcmp(cmd, "scan"))   rc = cmd_scan  (fd, sub_argc, sub_argv);
    else if (!strcmp(cmd, "verify")) rc = cmd_verify(fd, sub_argc, sub_argv);
    else {
        ISX031_ERR("unknown command: '%s'", cmd);
        disp_help(argv_orig[0]);
        rc = 2;
    }

    close(fd);
    free(argv);
    return rc;
}