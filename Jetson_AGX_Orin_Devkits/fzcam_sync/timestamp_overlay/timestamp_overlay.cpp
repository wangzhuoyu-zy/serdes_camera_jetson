/*
 * timestamp_overlay - 实时采集 N 个相机画面并叠加 fzcam_sync 触发时间戳
 *
 * 用法: sudo ./timestamp_overlay [-N 4] [-d /dev/video] [-w 1920] [-h 1080]
 *
 * 按键:
 *   q / ESC - 退出
 *   s / p   - 拍图保存（合成大图）
 *
 * 布局: N 相机按 2xceil(N/2) 网格拼成大图，每格自带时间戳叠加。
 */

#include <opencv2/opencv.hpp>

#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <vector>

/* ---- fzcam_sync ioctl 定义 ---- */
#define FZCAM_SYNC_MAX_TS_QUEUE 32

#define FZCAM_SYNC_START        _IOW('f', 1, fzcam_sync_config_t)
#define FZCAM_SYNC_STOP         _IO('f', 2)
#define FZCAM_SYNC_GET_TS_BATCH _IOR('f', 4, fzcam_sync_ts_batch_t)

typedef struct {
    uint64_t tsc_counter_ns;
    uint64_t capture_timestamp_ns;
    uint32_t generator_id;
    uint32_t edge_id;
    uint32_t sequence;
    uint32_t flags;
} fzcam_sync_ts_t;

typedef struct {
    fzcam_sync_ts_t timestamps[FZCAM_SYNC_MAX_TS_QUEUE];
    uint32_t count;
    uint32_t flags;
} fzcam_sync_ts_batch_t;

typedef struct {
    uint16_t fps;
    uint16_t duty_cycle;
    uint32_t enable_timestamp;
} fzcam_sync_config_t;

/* ---- 全局 ---- */
static volatile bool g_running = true;
static int           g_fzcam_fd = -1;

/* CLOCK_MONOTONIC 相对 CLOCK_REALTIME 的偏移（纳秒） */
static uint64_t g_mono_to_wall_offset_ns = 0;

static uint64_t mono_ns_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static uint64_t wall_ns_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static void init_mono_to_wall_offset(void) {
    uint64_t mono_sum = 0, wall_sum = 0;
    for (int i = 0; i < 5; i++) {
        mono_sum += mono_ns_now();
        wall_sum += wall_ns_now();
    }
    g_mono_to_wall_offset_ns = wall_sum / 5 - mono_sum / 5;
}

/* ---- 参数 ---- */
#define MAX_CAMS 16

struct Args {
    int         cam_count   = 1;                       // 相机数量
    int         start_index = 0;                       // 起始 /dev/videoN
    int         width       = 1920;
    int         height      = 1080;
    int         grid_cols   = 0;                       // 0 = 自动 2 列
    bool        show_help   = false;
};

static void print_usage(const char *prog) {
    fprintf(stderr,
        "用法: sudo %s [选项]\n"
        "\n"
        "选项:\n"
        "  -N, --num      N     相机数量 (1..%d, 默认 1)\n"
        "  -i, --index    I     起始 /dev/videoI (默认 0)\n"
        "  -w, --width    W     单相机宽度 (默认 1920)\n"
        "  -h, --height   H     单相机高度 (默认 1080)\n"
        "  -c, --cols     C     网格列数 (默认自动: 1->1, 2-3->2, 4+->sqrt)\n"
        "  -?, --help-only      显示帮助\n"
        "\n"
        "按键:\n"
        "  q / ESC  退出\n"
        "  s / p    拍图保存\n",
        prog, MAX_CAMS);
}

static bool parse_args(int argc, char *argv[], Args &args) {
    static struct option long_opts[] = {
        {"num",       required_argument, nullptr, 'N'},
        {"index",     required_argument, nullptr, 'i'},
        {"width",     required_argument, nullptr, 'w'},
        {"height",    required_argument, nullptr, 'h'},
        {"cols",      required_argument, nullptr, 'c'},
        {"help-only", no_argument,       nullptr, '?'},
        {nullptr,     0,                 nullptr, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "N:i:w:h:c:?", long_opts, nullptr)) != -1) {
        switch (opt) {
        case 'N': args.cam_count   = atoi(optarg); break;
        case 'i': args.start_index = atoi(optarg); break;
        case 'w': args.width       = atoi(optarg); break;
        case 'h': args.height      = atoi(optarg); break;
        case 'c': args.grid_cols   = atoi(optarg); break;
        case '?': args.show_help   = true;          break;
        default:  return false;
        }
    }
    if (args.show_help) { print_usage(argv[0]); return false; }
    if (args.cam_count < 1 || args.cam_count > MAX_CAMS) {
        fprintf(stderr, "[ERROR] -N 必须在 1..%d\n", MAX_CAMS);
        return false;
    }
    if (args.grid_cols == 0) {
        args.grid_cols = (args.cam_count <= 1) ? 1 :
                         (args.cam_count <= 3) ? 2 :
                         (int)std::ceil(std::sqrt((double)args.cam_count));
    }
    return true;
}

/* ---- fzcam_sync 设备封装 ---- */
static bool fzcam_sync_init(int &fd) {
    fd = open("/dev/fzcam_sync", O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "[ERROR] open /dev/fzcam_sync: %s (errno=%d)\n",
                strerror(errno), errno);
        return false;
    }
    fzcam_sync_config_t cfg = {};
    cfg.fps              = 2000;
    cfg.duty_cycle       = 10;
    cfg.enable_timestamp = 1;
    if (ioctl(fd, FZCAM_SYNC_START, &cfg) < 0) {
        fprintf(stderr, "[ERROR] ioctl FZCAM_SYNC_START: %s (errno=%d)\n",
                strerror(errno), errno);
        close(fd);
        fd = -1;
        return false;
    }
    printf("[INFO] fzcam_sync timestamp capture started (20 Hz)\n");
    return true;
}

static void fzcam_sync_stop(int fd) {
    if (fd >= 0) { ioctl(fd, FZCAM_SYNC_STOP); close(fd); }
}

static bool fzcam_sync_get_ts(int fd, fzcam_sync_ts_t &ts) {
    fzcam_sync_ts_batch_t batch = {};
    if (ioctl(fd, FZCAM_SYNC_GET_TS_BATCH, &batch) < 0) return false;
    if (batch.count == 0) return false;
    ts = batch.timestamps[batch.count - 1];
    return true;
}

/* ---- 单格叠加文字（UTC / Local / TSC） ---- */
static void overlay_cell(cv::Mat &tile, int cam_index, const fzcam_sync_ts_t *ts,
                         uint64_t wall_now_ns) {
    const int line_h = 24;
    const int bar_h  = line_h * 3 + 14;
    const int img_w  = tile.cols;
    const int img_h  = tile.rows;

    cv::Mat overlay = tile.clone();
    cv::rectangle(overlay, cv::Point(0, img_h - bar_h),
                  cv::Point(img_w, img_h), cv::Scalar(0, 0, 0), cv::FILLED);
    cv::addWeighted(overlay(cv::Rect(0, img_h - bar_h, img_w, bar_h)), 0.3,
                    tile(cv::Rect(0, img_h - bar_h, img_w, bar_h)), 0.7, 0,
                    tile(cv::Rect(0, img_h - bar_h, img_w, bar_h)));

    const double font = 0.45;
    const cv::Scalar white(255, 255, 255);
    const cv::Scalar yellow(0, 255, 255);
    int y = img_h - bar_h + 16;

    /* 第 1 行：CAM 标签 + TSC + 触发序号（黄） */
    char buf[256];
    if (ts) {
        snprintf(buf, sizeof(buf),
            "CAM%d  Trigger #%u  TSC=%lluns",
            cam_index, ts->sequence,
            (unsigned long long)ts->tsc_counter_ns);
    } else {
        snprintf(buf, sizeof(buf), "CAM%d  (no trigger)", cam_index);
    }
    cv::putText(tile, buf, cv::Point(8, y),
                cv::FONT_HERSHEY_SIMPLEX, font, yellow, 1, cv::LINE_AA);
    y += line_h;

    /* 第 2 行：UTC */
    if (ts) {
        uint64_t utc_ns = ts->capture_timestamp_ns + g_mono_to_wall_offset_ns;
        time_t sec = (time_t)(utc_ns / 1000000000ULL);
        long nsec  = (long)  (utc_ns % 1000000000ULL);
        struct tm tm_buf; gmtime_r(&sec, &tm_buf);
        snprintf(buf, sizeof(buf),
            "UTC:   %04d-%02d-%02d %02d:%02d:%02d.%09ld",
            tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
            tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec, nsec);
    } else {
        snprintf(buf, sizeof(buf), "UTC:   -");
    }
    cv::putText(tile, buf, cv::Point(8, y),
                cv::FONT_HERSHEY_SIMPLEX, font, white, 1, cv::LINE_AA);
    y += line_h;

    /* 第 3 行：Local（CLOCK_REALTIME） */
    time_t sec = (time_t)(wall_now_ns / 1000000000ULL);
    long nsec  = (long)  (wall_now_ns % 1000000000ULL);
    struct tm tm_buf; localtime_r(&sec, &tm_buf);
    snprintf(buf, sizeof(buf),
        "Local: %04d-%02d-%02d %02d:%02d:%02d.%09ld",
        tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
        tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec, nsec);
    cv::putText(tile, buf, cv::Point(8, y),
                cv::FONT_HERSHEY_SIMPLEX, font, white, 1, cv::LINE_AA);
}

static void sig_handler(int) { g_running = false; }

int main(int argc, char *argv[]) {
    Args args;
    if (!parse_args(argc, argv, args)) {
        return (args.show_help ? 0 : 1);
    }

    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);

    init_mono_to_wall_offset();
    printf("[INFO] mono->wall offset = %lluns\n",
           (unsigned long long)g_mono_to_wall_offset_ns);

    /* ---- 打开 fzcam_sync ---- */
    if (!fzcam_sync_init(g_fzcam_fd)) {
        fprintf(stderr, "[ERROR] 无法打开 /dev/fzcam_sync\n");
        return 1;
    }

    /* ---- 打开 N 个相机 ---- */
    std::vector<cv::VideoCapture> caps(args.cam_count);
    std::vector<char>             active(args.cam_count, 0);
    int active_cnt = 0;

    for (int i = 0; i < args.cam_count; i++) {
        char dev[64];
        snprintf(dev, sizeof(dev), "/dev/video%d", args.start_index + i);
        try {
            caps[i].open(std::string(dev), cv::CAP_V4L2);
        } catch (const cv::Exception &e) {
            fprintf(stderr, "[WARN] CAM%d OpenCV 异常 %s: %s\n", i, dev, e.what());
        }
        if (!caps[i].isOpened()) {
            fprintf(stderr, "[WARN] 无法打开 %s，跳过\n", dev);
            continue;
        }
        caps[i].set(cv::CAP_PROP_FRAME_WIDTH,  args.width);
        caps[i].set(cv::CAP_PROP_FRAME_HEIGHT, args.height);
        active[i]  = 1;
        active_cnt++;
        printf("[INFO] CAM%d  %s 已打开 (%dx%d)\n", i, dev, args.width, args.height);
    }
    if (active_cnt == 0) {
        fprintf(stderr, "[ERROR] 没有可用相机，退出\n");
        fzcam_sync_stop(g_fzcam_fd);
        return 1;
    }

    /* ---- 网格尺寸 ---- */
    int cols = args.grid_cols;
    int rows = (active_cnt + cols - 1) / cols;
    /* 单格尺寸 = 相机原尺寸（保证全分辨率） */
    int cell_w = args.width;
    int cell_h = args.height;
    int canvas_w = cols * cell_w;
    int canvas_h = rows * cell_h;
    printf("[INFO] 网格 %d x %d  画布 %d x %d\n", rows, cols, canvas_h, canvas_w);

    /* 限制画布显示尺寸，避免 4K×4 网格超出屏幕 */
    int show_w = canvas_w, show_h = canvas_h;
    const int MAX_W = 1920, MAX_H = 1080;
    double scale = 1.0;
    if (canvas_w > MAX_W || canvas_h > MAX_H) {
        scale = std::min((double)MAX_W / canvas_w, (double)MAX_H / canvas_h);
        show_w = (int)(canvas_w * scale);
        show_h = (int)(canvas_h * scale);
    }

    const char *win_name = "FZCam Timestamp Overlay (Multi)";
    cv::namedWindow(win_name, cv::WINDOW_NORMAL);
    cv::resizeWindow(win_name, show_w, show_h);

    /* ---- 主循环 ---- */
    fzcam_sync_ts_t last_ts = {};
    bool            has_ts  = false;
    std::vector<cv::Mat> frames(args.cam_count);
    cv::Mat canvas(canvas_h, canvas_w, CV_8UC3, cv::Scalar(20, 20, 20));

    while (g_running) {
        uint64_t wall_now = wall_ns_now();
        fzcam_sync_ts_t ts;
        if (fzcam_sync_get_ts(g_fzcam_fd, ts)) {
            last_ts = ts;
            has_ts  = true;
        }

        canvas.setTo(cv::Scalar(20, 20, 20));

        /* 拼接每路相机 */
        for (int i = 0; i < args.cam_count; i++) {
            if (!active[i]) continue;
            caps[i] >> frames[i];
            cv::Mat tile;
            if (!frames[i].empty()) {
                tile = frames[i];
            } else {
                tile = cv::Mat(cell_h, cell_w, CV_8UC3, cv::Scalar(40, 40, 40));
                cv::putText(tile, "NO SIGNAL", cv::Point(cell_w/2 - 80, cell_h/2),
                            cv::FONT_HERSHEY_SIMPLEX, 1.0,
                            cv::Scalar(0, 0, 255), 2, cv::LINE_AA);
            }

            /* 叠加 */
            overlay_cell(tile, i, has_ts ? &last_ts : nullptr, wall_now);

            /* 贴到画布 */
            int grid_idx = 0;
            for (int k = 0; k < i; k++) if (active[k]) grid_idx++;
            int r = grid_idx / cols;
            int c = grid_idx % cols;
            cv::Rect roi(c * cell_w, r * cell_h, cell_w, cell_h);
            tile.copyTo(canvas(roi));
        }

        /* 显示（按 scale 缩放） */
        cv::Mat show;
        if (scale < 1.0) {
            cv::resize(canvas, show, cv::Size(show_w, show_h));
        } else {
            show = canvas;
        }
        cv::imshow(win_name, show);

        /* 按键 */
        int key = cv::waitKey(1) & 0xFF;
        if (key == 'q' || key == 27) {
            g_running = false;
        } else if (key == 's' || key == 'p') {
            char fname[256];
            uint64_t ms = has_ts
                ? (last_ts.capture_timestamp_ns / 1000000ULL)
                : (wall_now / 1000000ULL);
            if (has_ts)
                snprintf(fname, sizeof(fname),
                         "snap_multi_trig%u_%llums.png",
                         last_ts.sequence, (unsigned long long)ms);
            else
                snprintf(fname, sizeof(fname),
                         "snap_multi_%llums.png",
                         (unsigned long long)ms);
            if (cv::imwrite(fname, canvas)) {
                printf("[INFO] 拍图已保存: %s (%dx%d)\n", fname, canvas_w, canvas_h);
            } else {
                fprintf(stderr, "[ERROR] 保存失败: %s\n", fname);
            }
        }
    }

    for (auto &c : caps) if (c.isOpened()) c.release();
    cv::destroyAllWindows();
    fzcam_sync_stop(g_fzcam_fd);
    printf("[INFO] 已退出\n");
    return 0;
}
