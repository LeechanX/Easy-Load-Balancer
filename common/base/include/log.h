#ifndef __TTCLOG_H__
#define __TTCLOG_H__

#include <sys/cdefs.h>
__BEGIN_DECLS

#include <stdio.h>

//#if CLIENTAPI
//#define __log_level__ 6
//#else
extern int __log_level__;
//#endif
#define log_generic(lvl, fmt, args...)	 _write_log_(lvl, __FILE__, __FUNCTION__, __LINE__ , fmt, ##args)
#define log_emerg(fmt, args...)	log_generic(0, fmt, ##args)
#define log_alert(fmt, args...)		log_generic(1, fmt, ##args)
#define log_crit(fmt, args...)		log_generic(2, fmt, ##args)
#define log_error(fmt, args...)		log_generic(3, fmt, ##args)
#define log_warning(fmt, args...)	do{ if(__log_level__>=4)log_generic(4, fmt, ##args); } while(0)
#define log_notice(fmt, args...)	do{ if(__log_level__>=5)log_generic(5, fmt, ##args); } while(0)
#define log_info(fmt, args...)		do{ if(__log_level__>=6)log_generic(6, fmt, ##args); } while(0)
#define log_debug(fmt, args...)		do{ if(__log_level__>=7)log_generic(7, fmt, ##args); } while(0)

#define error_log(fmt, args...)	log_error(fmt, ##args)

#if __cplusplus
extern void _init_log_ (const char *app, const char *dir = NULL);
#else
extern void _init_log_ (const char *app, const char *dir);
#endif
extern void _init_log_stat_(void);
extern void _set_log_level_(int);
extern void _set_log_thread_name_(const char *n);
extern void _write_log_ (int, const char*, const char *, int, const char *, ...) __attribute__((format(printf,5,6)));

#include <asm/unistd.h>
#include <unistd.h>
#ifndef __NR_gettid
#endif
static inline int _gettid_(void) { return syscall(__NR_gettid); }

#include <sys/time.h>
static inline unsigned int GET_MSEC(void) { struct timeval tv; gettimeofday(&tv, NULL); return tv.tv_sec * 1000 + tv.tv_usec/1000; }
#define INIT_MSEC(v)	v = GET_MSEC()
#define CALC_MSEC(v)	v =  GET_MSEC() - (v)
static inline unsigned int GET_USEC(void) { struct timeval tv; gettimeofday(&tv, NULL); return tv.tv_sec * 1000000 + tv.tv_usec; }
#define INIT_USEC(v)	v = GET_USEC()
#define CALC_USEC(v)	v =  GET_USEC() - (v)

__END_DECLS
#endif
