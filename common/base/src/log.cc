#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>

#include "log.h"

#define LOGSIZE 4096

int __log_level__ = 6;

static int logfd = -1;
static int logday = 0;
static char log_dir[128] = "../log";
static char appname[32] = "";
#if CLIENTAPI
static const char *threadname = NULL;
//static void init_namekey(void) { }
void _set_log_thread_name_(const char *n) { }
#else
#if HAS_TLS
static __thread const char *threadname;
void _set_log_thread_name_(const char *n) { threadname = n; }
#else
static pthread_key_t namekey;
static pthread_once_t nameonce = PTHREAD_ONCE_INIT;
static void _init_namekey_(void) { pthread_key_create(&namekey, NULL); }
void _set_log_thread_name_(const char *n) { pthread_setspecific(namekey, n); }
#endif
#endif

#if HAS_STAT
#include "StatTTC.h"
static int statEnable;
CStatItemU32 statLogCount[8];
static unsigned int localStatLogCnt[8];
#endif

void _init_log_ (const char *app, const char *dir)
{
#if !CLIENTAPI
#if !HAS_TLS
	pthread_once(&nameonce, _init_namekey_);
#endif
#endif
#if HAS_STAT
	statEnable = 0; // 暂时只在本地统计
	memset(localStatLogCnt, 0, sizeof(localStatLogCnt));
#endif

	strncpy(appname, app, sizeof(appname)-1);
	
	if(dir) {
		strncpy (log_dir, dir, sizeof (log_dir) - 1);
	}
	mkdir(log_dir, 0777);
	if(access(log_dir, W_OK|X_OK) < 0)
	{
		log_error("logdir(%s): Not writable", log_dir);
	}

	logfd = open("/dev/null", O_WRONLY);
	if(logfd < 0)
		logfd = dup(2);
	fcntl(logfd, F_SETFD, FD_CLOEXEC);
}

void _init_log_stat_(void)
{
#if HAS_STAT
	for(unsigned i=0; i<8; i++)
	{
		statLogCount[i] = statmgr.GetItemU32(LOG_COUNT_0+i);
		statLogCount[i].set(localStatLogCnt[i]);
	}
	statEnable = 1;
#endif
}

void _set_log_level_(int l)
{
	if(l>=0)
		__log_level__ = l > 4 ? l : 4;
}

void _write_log_(
	int level, 
	const char *filename, 
	const char *funcname,
	int lineno,
	const char *format, ...)
{
	// save errno
	int savedErrNo = errno;
	int off = 0;
	char buf[LOGSIZE];
	char logfile[256];
#if !CLIENTAPI
#if !HAS_TLS
	const char *threadname;
#endif
#endif

	if(appname[0] == 0)
		return;

#if HAS_STAT
	if(level < 0) level = 0;
	else if(level > 7) level = 7;
	if(!statEnable)
		++localStatLogCnt[level];
	else
		++statLogCount[level];
#endif
	
	// construct prefix
	struct tm tm;
	time_t now = time (NULL);
	localtime_r(&now, &tm);
	filename = basename(filename);
#if !CLIENTAPI
#if HAS_TLS
#else
	pthread_once(&nameonce, _init_namekey_);
	threadname = (const char *)pthread_getspecific(namekey);
#endif
#endif
	if(threadname)
	off = snprintf (buf, LOGSIZE,
			"<%d>[%02d:%02d:%02d] %s: %s(%d)[%s]: ",
			level,
			tm.tm_hour, tm.tm_min, tm.tm_sec,
			threadname,
			filename, lineno, funcname
		       );
	else
	off = snprintf (buf, LOGSIZE,
			"<%d>[%02d:%02d:%02d] pid[%d]: %s(%d)[%s]: ",
			level,
			tm.tm_hour, tm.tm_min, tm.tm_sec,
			_gettid_(),
			filename, lineno, funcname
		       );
	if(off >= LOGSIZE)
		off = LOGSIZE - 1;
		
	{
	int today = tm.tm_year*1000 + tm.tm_yday;
	if(logfd >= 0 && today != logday)
	{
		int fd;

		logday = today;
		snprintf (logfile, sizeof(logfile),
				"%s/%s.error%04d%02d%02d.log", log_dir, appname,
				tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
		fd = open (logfile, O_CREAT | O_LARGEFILE | O_APPEND |O_WRONLY, 0644);
		if(fd >= 0)
		{
			dup2(fd, logfd);
			close(fd);
			fcntl(logfd, F_SETFD, FD_CLOEXEC);
		}
	}
	}

	{
		// formatted message
		va_list ap;
		va_start(ap, format);
		// restore errno
		errno = savedErrNo;
		off += vsnprintf(buf+off, LOGSIZE-off, format, ap);
		va_end(ap);
	}

	if(off >= LOGSIZE)
		off = LOGSIZE - 1;
	if(buf[off-1]!='\n'){
		if(off < LOGSIZE - 1)
			buf[off++] = '\n';
		else
			buf[LOGSIZE-1] = '\n';
	}

	int unused;
	unused = fwrite(buf+3, 1, off-3, stderr);

	if(logfd >= 0)
		unused = write(logfd, buf, off);
}

