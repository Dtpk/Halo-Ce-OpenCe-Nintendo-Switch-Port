/*
HOST_SYSCALL.C

Emulation of musl Linux system calls for Nintendo Switch.
Maps guest system call numbers to Switch Newlib / libnx POSIX functions.
*/

#include "host.h"

#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

struct guest_timespec
{
	int32_t seconds;
	int32_t nanoseconds;
};

#define GUEST(type, value) ((type)(uintptr_t)(uint32_t)(value))

struct kstat {
	unsigned long long st_dev;
	unsigned long long st_ino;
	unsigned int st_mode;
	unsigned int st_nlink;
	unsigned int st_uid;
	unsigned int st_gid;
	unsigned long long st_rdev;
	unsigned long long __pad;
	long long st_size;
	int st_blksize;
	int __pad2;
	long long st_blocks;
	long long st_atime_sec;
	long long st_atime_nsec;
	long long st_mtime_sec;
	long long st_mtime_nsec;
	long long st_ctime_sec;
	long long st_ctime_nsec;
	unsigned int unused_pad[2];
};

static void kstat_out(uint64_t guest_addr, const struct stat *st)
{
	struct kstat *k = GUEST(struct kstat *, guest_addr);
	if (!k) return;
	memset(k, 0, sizeof(*k));
	k->st_dev = st->st_dev;
	k->st_ino = st->st_ino;
	k->st_mode = st->st_mode;
	k->st_nlink = st->st_nlink;
	k->st_uid = st->st_uid;
	k->st_gid = st->st_gid;
	k->st_rdev = st->st_rdev;
	k->st_size = st->st_size;
	k->st_blksize = st->st_blksize ? st->st_blksize : 4096;
	k->st_blocks = st->st_blocks;
	k->st_atime_sec = st->st_atime;
	k->st_mtime_sec = st->st_mtime;
	k->st_ctime_sec = st->st_ctime;
}

static int translate_open_flags(int linux_flags)
{
	int host_flags = 0;
	int acc = linux_flags & 3;
	if (acc == 0) host_flags = O_RDONLY;
	else if (acc == 1) host_flags = O_WRONLY;
	else if (acc == 2) host_flags = O_RDWR;

	if (linux_flags & 0x40)   host_flags |= O_CREAT;
	if (linux_flags & 0x80)   host_flags |= O_EXCL;
	if (linux_flags & 0x200)  host_flags |= O_TRUNC;
	if (linux_flags & 0x400)  host_flags |= O_APPEND;
	if (linux_flags & 0x800)  host_flags |= O_NONBLOCK;

	return host_flags;
}

static int timespec_in(uint64_t address, struct timespec *result)
{
	const struct guest_timespec *value = GUEST(const struct guest_timespec *, address);

	if (!address)
		return 0;
	result->tv_sec = value->seconds;
	result->tv_nsec = value->nanoseconds;
	return 1;
}

static void timespec_out(uint64_t address, const struct timespec *value)
{
	struct guest_timespec *result = GUEST(struct guest_timespec *, address);

	if (!address)
		return;
	result->seconds = (int32_t)value->tv_sec;
	result->nanoseconds = (int32_t)value->tv_nsec;
}

static long result_of(long value)
{
	return value == -1 ? -errno : value;
}

static pthread_mutex_t pread_lock = PTHREAD_MUTEX_INITIALIZER;

static ssize_t host_pread(int fd, void *buf, size_t count, off_t offset)
{
	pthread_mutex_lock(&pread_lock);
	off_t orig = lseek(fd, 0, SEEK_CUR);
	if (orig == (off_t)-1) { pthread_mutex_unlock(&pread_lock); return -1; }
	if (lseek(fd, offset, SEEK_SET) == (off_t)-1) { pthread_mutex_unlock(&pread_lock); return -1; }
	ssize_t ret = read(fd, buf, count);
	lseek(fd, orig, SEEK_SET);
	pthread_mutex_unlock(&pread_lock);
	return ret;
}

static ssize_t host_pwrite(int fd, const void *buf, size_t count, off_t offset)
{
	pthread_mutex_lock(&pread_lock);
	off_t orig = lseek(fd, 0, SEEK_CUR);
	if (orig == (off_t)-1) { pthread_mutex_unlock(&pread_lock); return -1; }
	if (lseek(fd, offset, SEEK_SET) == (off_t)-1) { pthread_mutex_unlock(&pread_lock); return -1; }
	ssize_t ret = write(fd, buf, count);
	lseek(fd, orig, SEEK_SET);
	pthread_mutex_unlock(&pread_lock);
	return ret;
}

long long host_syscall(long long number, long long a, long long b, long long c,
	long long d, long long e, long long f)
{
	if (number != 113 && number != 114 && number != 169 && number != 124 && number != 98 && number != 64)
	{
		host_logf(HOST_LOG_INFO, "[HOST SYSCALL] number=%lld, args=%llx %llx %llx", number, a, b, c);
		consoleUpdate(NULL);
	}
	switch (number)
	{
	case 64: /* SYS_write */
		if (a == 1 || a == 2)
		{
			/* Output to stdout/console and halo.log */
			const void *data = GUEST(const void *, b);
			size_t size = (size_t)(uint32_t)c;
			pthread_mutex_lock(&log_lock);
			if (size > 0 && size < 65536)
			{
				if (log_file)
				{
					fwrite(data, 1, size, log_file);
					fflush(log_file);
				}
				fwrite(data, 1, size, stdout);
				fflush(stdout);
				consoleUpdate(NULL);
			}
			pthread_mutex_unlock(&log_lock);
			return (uint32_t)c;
		}
		{
			ssize_t ret = write((int)a, GUEST(const void *, b), (size_t)(uint32_t)c);
			return result_of(ret);
		}

	case 63: /* SYS_read */
		return result_of(read((int)a, GUEST(void *, b), (size_t)(uint32_t)c));

	case 67: /* SYS_pread64 */
		return result_of(host_pread((int)a, GUEST(void *, b), (size_t)(uint32_t)c, (off_t)d));

	case 68: /* SYS_pwrite64 */
		return result_of(host_pwrite((int)a, GUEST(const void *, b), (size_t)(uint32_t)c, (off_t)d));

	case 56: /* SYS_openat */
	{
		const char *raw_path = GUEST(const char *, b);
		char path[1024];
		translate_guest_path(raw_path, path, sizeof(path));
		int flags = translate_open_flags((int)c);
		int fd = open(path, flags, (mode_t)d);
		if (fd >= 0)
			host_logf(HOST_LOG_INFO, "[HOST FS] openat '%s' (raw '%s') -> fd %d", path, raw_path, fd);
		else
			host_logf(HOST_LOG_WARN, "[HOST FS] openat '%s' (raw '%s') failed: errno %d (%s)", path, raw_path, errno, strerror(errno));
		return result_of(fd);
	}

	case 57: /* SYS_close */
	{
		int res = close((int)a);
		return result_of(res);
	}

	case 62: /* SYS_lseek */
		return result_of(lseek((int)a, (off_t)b, (int)c));

	case 113: /* SYS_clock_gettime */
	{
		struct timespec value;
		long result = clock_gettime((clockid_t)a, &value);
		if (result == 0)
			timespec_out((uint64_t)b, &value);
		return result_of(result);
	}

	case 114: /* SYS_clock_getres */
	{
		struct timespec value;
		long result = clock_getres((clockid_t)a, &value);
		if (result == 0)
			timespec_out((uint64_t)b, &value);
		return result_of(result);
	}

	case 169: /* SYS_gettimeofday */
	{
		struct timeval value;
		struct guest_timespec *result = GUEST(struct guest_timespec *, a);
		gettimeofday(&value, NULL);
		if (result)
		{
			result->seconds = (int32_t)value.tv_sec;
			result->nanoseconds = (int32_t)(value.tv_usec * 1000);
		}
		return 0;
	}

	case 101: /* SYS_nanosleep */
	{
		struct timespec request, remaining;
		long result;
		if (!timespec_in((uint64_t)a, &request))
			return -EFAULT;
		result = result_of(nanosleep(&request, &remaining));
		if (result == -EINTR)
			timespec_out((uint64_t)b, &remaining);
		return result;
	}

	case 124: /* SYS_sched_yield */
		return sched_yield();

	case 98: /* SYS_futex */
		sched_yield();
		return 0;

	case 222: /* SYS_mmap */
		return host_guest_mmap((uint64_t)a, (uint64_t)b, (int)c, (int)d, (int)e, f);

	case 215: /* SYS_munmap */
		return host_guest_munmap((uint64_t)a, (uint64_t)b);

	case 226: /* SYS_mprotect */
		return host_guest_mprotect((uint64_t)a, (uint64_t)b, (int)c);

	case 216: /* SYS_mremap */
	case 214: /* SYS_brk */
		return -ENOMEM;

	case 233: /* SYS_madvise */
		return 0;

	case 134: /* SYS_rt_sigaction */
	case 132: /* SYS_sigaltstack */
	case 135: /* SYS_rt_sigprocmask */
		return 0;

	case 93: /* SYS_exit */
	case 94: /* SYS_exit_group */
		host_exit((int)a);

	case 96: /* SYS_set_tid_address */
		return 1;

	case 17: /* SYS_getcwd */
	{
		char *buf = GUEST(char *, a);
		if (getcwd(buf, (size_t)b))
			return (long)strlen(buf) + 1;
		return -errno;
	}

	case 49: /* SYS_chdir */
	{
		const char *raw_path = GUEST(const char *, a);
		char path[1024];
		translate_guest_path(raw_path, path, sizeof(path));
		return result_of(chdir(path));
	}

	case 34: /* SYS_mkdirat */
	{
		const char *raw_path = GUEST(const char *, b);
		char path[1024];
		translate_guest_path(raw_path, path, sizeof(path));
		return result_of(mkdir(path, (mode_t)c));
	}

	case 35: /* SYS_unlinkat */
	{
		const char *raw_path = GUEST(const char *, b);
		char path[1024];
		translate_guest_path(raw_path, path, sizeof(path));
		return result_of(unlink(path));
	}

	case 38: /* SYS_renameat */
	{
		const char *raw_old = GUEST(const char *, b);
		const char *raw_new = GUEST(const char *, d);
		char old_path[1024], new_path[1024];
		translate_guest_path(raw_old, old_path, sizeof(old_path));
		translate_guest_path(raw_new, new_path, sizeof(new_path));
		return result_of(rename(old_path, new_path));
	}

	case 80: /* SYS_fstat */
	{
		struct stat st;
		long res = fstat((int)a, &st);
		if (res == 0)
			kstat_out((uint64_t)b, &st);
		return result_of(res);
	}

	case 79: /* SYS_newfstatat */
	{
		struct stat st;
		const char *raw_path = GUEST(const char *, b);
		char path[1024];
		translate_guest_path(raw_path, path, sizeof(path));
		long res = stat(path, &st);
		if (res == 0)
			kstat_out((uint64_t)c, &st);
		return result_of(res);
	}

	case 172: /* SYS_getpid */
		return 1;

	case 178: /* SYS_gettid */
		return 1;

	case 278: /* SYS_getrandom */
	{
		randomGet(GUEST(void *, a), (size_t)b);
		return (long)b;
	}

	default:
		return -ENOSYS;
	}
}
