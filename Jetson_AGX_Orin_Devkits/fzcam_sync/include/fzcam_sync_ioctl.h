/*
 * fzcam_sync_ioctl.h - Userspace ioctl definitions for fzcam_sync.ko
 *
 * Compatible with C and C++.
 */
#ifndef _FZCAM_SYNC_IOCTL_H_
#define _FZCAM_SYNC_IOCTL_H_

#include <stdint.h>
#include <sys/ioctl.h>

/* ---- Constants ---- */
#define FZCAM_SYNC_MAX_TS_QUEUE  32

#define FZCAM_SYNC_FLAG_VALID    (1 << 0)
#define FZCAM_SYNC_FLAG_OVERFLOW (1 << 1)

#define FZCAM_SYNC_FPS_SCALE     1   /* 0.01 fps resolution */

/* ---- ioctl Commands ---- */
#define FZCAM_SYNC_START          _IOW('f', 1, fzcam_sync_config_t)
#define FZCAM_SYNC_STOP           _IO('f', 2)
#define FZCAM_SYNC_GET_TS         _IOR('f', 3, fzcam_sync_ts_t)
#define FZCAM_SYNC_GET_TS_BATCH   _IOR('f', 4, fzcam_sync_ts_batch_t)
#define FZCAM_SYNC_CLEAR_TS       _IO('f', 5)
#define FZCAM_SYNC_GET_STATUS     _IOR('f', 6, fzcam_sync_status_t)

/* ---- Types ---- */

/* Single timestamp entry */
typedef struct {
    uint64_t tsc_counter_ns;       /* TSC counter value converted to nanoseconds */
    uint64_t capture_timestamp_ns; /* ktime_get_ns() at capture moment (CLOCK_MONOTONIC) */
    uint32_t generator_id;         /* Generator ID */
    uint32_t edge_id;              /* Edge ID: 0=rising, 1=falling */
    uint32_t sequence;             /* Monotonic sequence number */
    uint32_t flags;                /* FZCAM_SYNC_FLAG_* */
} fzcam_sync_ts_t;

/* Batch timestamp retrieval */
typedef struct {
    fzcam_sync_ts_t timestamps[FZCAM_SYNC_MAX_TS_QUEUE];
    uint32_t count;
    uint32_t flags;
} fzcam_sync_ts_batch_t;

/* Configuration for start ioctl */
typedef struct {
    uint16_t fps;                  /* FPS * FZCAM_SYNC_FPS_SCALE (e.g. 3000 = 30.00 Hz) */
    uint16_t duty_cycle;           /* Duty cycle in percent (1-99) */
    uint32_t enable_timestamp;     /* non-zero = enable timestamp capture */
} fzcam_sync_config_t;

/* Device status */
typedef struct {
    uint32_t running;              /* 1 = generators active */
    uint32_t ts_queue_count;       /* Number of timestamps in kernel queue */
    uint32_t ts_sequence;          /* Total timestamps captured since start */
    uint32_t generator_count;      /* Number of configured generators */
} fzcam_sync_status_t;

#endif /* _FZCAM_SYNC_IOCTL_H_ */
