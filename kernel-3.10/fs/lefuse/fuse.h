#ifndef ___FUSE_H___
#define ___FUSE_H___
extern void lefuse_request_send_background_ex(struct fuse_conn *fc,
    struct fuse_req *req, __u32 size);
extern void lefuse_request_send_ex(struct fuse_conn *fc, struct fuse_req *req,
    __u32 size);

#ifndef USER_BUILD_KERNEL  /* IO log is only enabled in eng load */
#define FUSEIO_TRACE
#endif

#ifdef MET_FUSEIO_TRACE
#define MET_FUSE_IOLOG_INIT()   struct timespec lemet_fuse_start_time, lemet_fuse_end_time
#define MET_FUSE_IOLOG_START()  get_monotonic_boottime(&lemet_fuse_start_time)
#define MET_FUSE_IOLOG_END()    get_monotonic_boottime(&lemet_fuse_end_time)
#else
#define MET_FUSE_IOLOG_INIT(...)
#define MET_FUSE_IOLOG_START(...)
#define MET_FUSE_IOLOG_END(...)
#endif


#ifdef FUSEIO_TRACE
#include <linux/sched.h>
#include <linux/xlog.h>
#include <linux/kthread.h>

extern void lefuse_time_diff(struct timespec *start,
    struct timespec *end,
    struct timespec *diff);

extern void lefuse_iolog_add(__u32 io_bytes, int type,
    struct timespec *start,
    struct timespec *end);

extern __u32 lefuse_iolog_timeus_diff(struct timespec *start,
    struct timespec *end);

extern void lefuse_iolog_exit(void);
extern void lefuse_iolog_init(void);

struct fuse_rw_info
{
    __u32  count;
    __u32  bytes;
    __u32  us;
};

struct fuse_proc_info
{
    pid_t pid;
    __u32 valid;
    int misc_type;
    struct fuse_rw_info read;
    struct fuse_rw_info write;
    struct fuse_rw_info misc;
};

#define FUSE_IOLOG_MAX     12
#define FUSE_IOLOG_BUFLEN  512
#define FUSE_IOLOG_LATENCY 1

#define FUSE_IOLOG_INIT()   struct timespec _tstart, _tend
#define FUSE_IOLOG_START()  get_monotonic_boottime(&_tstart)
#define FUSE_IOLOG_END()    get_monotonic_boottime(&_tend)
#define FUSE_IOLOG_US()     lefuse_iolog_timeus_diff(&_tstart, &_tend)
#define FUSE_IOLOG_PRINT(iobytes, type)  lefuse_iolog_add(iobytes, type, &_tstart, &_tend)

#else

#define FUSE_IOLOG_INIT(...)
#define FUSE_IOLOG_START(...)
#define FUSE_IOLOG_END(...)
#define FUSE_IOLOG_PRINT(...)
#define lefuse_iolog_init(...)
#define lefuse_iolog_exit(...)

#endif

#endif
