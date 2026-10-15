/*
SWITCH_POSIX.C

POSIX file and networking abstractions for Nintendo Switch.
Implements all 45 posix_* functions required by halo_guest.elf.
*/

#include "host.h"
#if __has_include("posix.h")
#include "posix.h"
#else
#include "../../linux/src/posix.h"
#endif

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

static void split64(unsigned long long value, posix_ulong *low, posix_ulong *high)
{
	*low = (posix_ulong)(value & 0xffffffffULL);
	*high = (posix_ulong)(value >> 32);
}

static void fill_information(const struct stat *st, struct posix_file_information *information)
{
	memset(information, 0, sizeof(*information));
	if (S_ISDIR(st->st_mode))
		information->flags |= _posix_file_is_directory;
	if (!(st->st_mode & S_IWUSR))
		information->flags |= _posix_file_is_read_only;
	split64((unsigned long long)st->st_size, &information->size_low, &information->size_high);
	information->modification_seconds = (posix_ulong)st->st_mtime;
	information->access_seconds = (posix_ulong)st->st_atime;
	information->creation_seconds = (posix_ulong)st->st_ctime;
}

int posix_stat(const char *path, struct posix_file_information *information)
{
	char host_path[1024];
	translate_guest_path(path, host_path, sizeof(host_path));
	struct stat st;
	if (stat(host_path, &st) != 0)
		return -1;
	fill_information(&st, information);
	return 0;
}

int posix_fstat(int descriptor, struct posix_file_information *information)
{
	struct stat st;
	if (fstat(descriptor, &st) != 0)
		return -1;
	fill_information(&st, information);
	return 0;
}

int posix_set_file_times(const char *path,
	posix_ulong access_seconds, posix_ulong access_nanoseconds,
	posix_ulong modification_seconds, posix_ulong modification_nanoseconds)
{
	(void)path;
	(void)access_seconds;
	(void)access_nanoseconds;
	(void)modification_seconds;
	(void)modification_nanoseconds;
	return 0;
}

int posix_seek(int descriptor, posix_long offset_low, posix_long offset_high, int whence,
	posix_ulong *position_low, posix_ulong *position_high)
{
	off_t offset = (off_t)(((unsigned long long)(posix_ulong)offset_high << 32) | (posix_ulong)offset_low);
	off_t result = lseek(descriptor, offset, whence);
	if (result == (off_t)-1)
		return -1;
	split64((unsigned long long)result, position_low, position_high);
	return 0;
}

int posix_truncate(int descriptor, posix_ulong size_low, posix_ulong size_high)
{
	return ftruncate(descriptor, (off_t)(((unsigned long long)size_high << 32) | size_low));
}

int posix_disk_space(const char *path,
	posix_ulong *free_low, posix_ulong *free_high,
	posix_ulong *total_low, posix_ulong *total_high)
{
	(void)path;
	/* Return safe static values (16 GB free, 32 GB total) to prevent statvfs driver crashes */
	split64(16ULL * 1024 * 1024 * 1024, free_low, free_high);
	split64(32ULL * 1024 * 1024 * 1024, total_low, total_high);
	return 0;
}

int posix_set_read_only(const char *path, int read_only)
{
	(void)path;
	(void)read_only;
	return 0;
}

int posix_make_directory(const char *path)
{
	char host_path[1024];
	translate_guest_path(path, host_path, sizeof(host_path));
	return mkdir(host_path, 0777);
}

#define DIRECTORY_HANDLE_COUNT 64
static DIR *directory_handles[DIRECTORY_HANDLE_COUNT];
static pthread_mutex_t directory_handle_lock = PTHREAD_MUTEX_INITIALIZER;

static void *directory_handle_new(DIR *directory)
{
	unsigned long index;
	if (!directory)
		return NULL;
	pthread_mutex_lock(&directory_handle_lock);
	for (index = 0; index < DIRECTORY_HANDLE_COUNT; index++)
	{
		if (!directory_handles[index])
		{
			directory_handles[index] = directory;
			pthread_mutex_unlock(&directory_handle_lock);
			return (void *)(index + 1);
		}
	}
	pthread_mutex_unlock(&directory_handle_lock);
	closedir(directory);
	return NULL;
}

static DIR *directory_from_handle(void *handle, int release)
{
	unsigned long index = (unsigned long)handle - 1;
	DIR *directory = NULL;
	if (index >= DIRECTORY_HANDLE_COUNT)
		return NULL;
	pthread_mutex_lock(&directory_handle_lock);
	directory = directory_handles[index];
	if (release)
		directory_handles[index] = NULL;
	pthread_mutex_unlock(&directory_handle_lock);
	return directory;
}

void *posix_directory_open(const char *path)
{
	char host_path[1024];
	translate_guest_path(path, host_path, sizeof(host_path));
	return directory_handle_new(opendir(host_path));
}

int posix_directory_next(void *directory, char *name, posix_ulong name_size)
{
	DIR *stream = directory_from_handle(directory, 0);
	struct dirent *entry;
	if (!stream)
		return 0;
	while ((entry = readdir(stream)) != NULL)
	{
		if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
			continue;
		if (strlen(entry->d_name) + 1 > name_size)
			continue;
		strcpy(name, entry->d_name);
		return 1;
	}
	return 0;
}

void posix_directory_close(void *directory)
{
	DIR *stream = directory_from_handle(directory, 1);
	if (stream)
		closedir(stream);
}

int posix_find_entry_case_insensitive(const char *directory, const char *name,
	char *result, posix_ulong result_size)
{
	char host_dir[1024];
	translate_guest_path(directory, host_dir, sizeof(host_dir));
	DIR *handle = opendir(*host_dir ? host_dir : ".");
	struct dirent *entry;
	int found = 0;

	if (!handle)
		return 0;
	while ((entry = readdir(handle)) != NULL)
	{
		if (!strcasecmp(entry->d_name, name) && strlen(entry->d_name) + 1 <= result_size)
		{
			strcpy(result, entry->d_name);
			found = 1;
			break;
		}
	}
	closedir(handle);
	return found;
}

/* ---------- Networking ---------- */

static int last_net_error = 0;

int posix_socket_last_error(void)
{
	return last_net_error;
}

int posix_socket(int family, int type, int protocol)
{
	int s = socket(family, type, protocol);
	if (s < 0)
		last_net_error = errno;
	return s;
}

int posix_socket_close(int s)
{
	return close(s);
}

int posix_socket_bind(int s, const void *address, int address_length)
{
	return bind(s, address, (socklen_t)address_length);
}

int posix_socket_connect(int s, const void *address, int address_length)
{
	return connect(s, address, (socklen_t)address_length);
}

int posix_socket_listen(int s, int backlog)
{
	return listen(s, backlog);
}

int posix_socket_accept(int s, void *address, int *address_length)
{
	socklen_t len = address_length ? (socklen_t)*address_length : 0;
	int res = accept(s, address, address_length ? &len : NULL);
	if (address_length)
		*address_length = (int)len;
	return res;
}

int posix_socket_send(int s, const void *buffer, int length, int flags)
{
	return (int)send(s, buffer, (size_t)length, flags);
}

int posix_socket_sendto(int s, const void *buffer, int length, int flags,
	const void *address, int address_length)
{
	return (int)sendto(s, buffer, (size_t)length, flags, address, (socklen_t)address_length);
}

int posix_socket_recv(int s, void *buffer, int length, int flags)
{
	return (int)recv(s, buffer, (size_t)length, flags);
}

int posix_socket_recvfrom(int s, void *buffer, int length, int flags,
	void *address, int *address_length)
{
	socklen_t len = address_length ? (socklen_t)*address_length : 0;
	int res = (int)recvfrom(s, buffer, (size_t)length, flags, address, address_length ? &len : NULL);
	if (address_length)
		*address_length = (int)len;
	return res;
}

int posix_socket_shutdown(int s, int how)
{
	return shutdown(s, how);
}

int posix_socket_set_nonblocking(int s, int nonblocking)
{
	int flags = fcntl(s, F_GETFL);
	if (flags < 0)
		return -1;
	flags = nonblocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
	return fcntl(s, F_SETFL, flags);
}

int posix_socket_set_nodelay(int s)
{
	int val = 1;
	return setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val));
}

int posix_socket_bytes_available(int s, posix_ulong *count)
{
	int avail = 0;
	int res = ioctl(s, FIONREAD, &avail);
	if (res >= 0)
		*count = (posix_ulong)avail;
	return res;
}

int posix_socket_setsockopt(int s, int level, int name, const void *value, int length)
{
	return setsockopt(s, level, name, value, (socklen_t)length);
}

int posix_socket_getsockopt(int s, int level, int name, void *value, int *length)
{
	socklen_t len = (socklen_t)*length;
	int res = getsockopt(s, level, name, value, &len);
	*length = (int)len;
	return res;
}

int posix_socket_getsockname(int s, void *address, int *address_length)
{
	socklen_t len = (socklen_t)*address_length;
	int res = getsockname(s, address, &len);
	*address_length = (int)len;
	return res;
}

int posix_socket_getpeername(int s, void *address, int *address_length)
{
	socklen_t len = (socklen_t)*address_length;
	int res = getpeername(s, address, &len);
	*address_length = (int)len;
	return res;
}

int posix_socket_select(int *read, int *read_count, int *write, int *write_count,
	int *error, int *error_count, posix_long timeout_seconds, posix_long timeout_microseconds, int infinite)
{
	struct pollfd fds[64];
	int total = 0;
	int i;

	if (read && read_count)
	{
		for (i = 0; i < *read_count && total < 64; i++)
		{
			fds[total].fd = read[i];
			fds[total].events = POLLIN;
			fds[total].revents = 0;
			total++;
		}
	}
	if (write && write_count)
	{
		for (i = 0; i < *write_count && total < 64; i++)
		{
			fds[total].fd = write[i];
			fds[total].events = POLLOUT;
			fds[total].revents = 0;
			total++;
		}
	}

	int timeout_ms = infinite ? -1 : (int)(timeout_seconds * 1000 + timeout_microseconds / 1000);
	return poll(fds, (nfds_t)total, timeout_ms);
}

posix_ulong posix_local_ipv4_address(void)
{
	/* 127.0.0.1 in network byte order */
	return htonl(0x7F000001);
}

void posix_random_bytes(void *buffer, posix_ulong size)
{
	randomGet(buffer, (size_t)size);
}

struct _reent;
int _getentropy_r(struct _reent *r, void *buf, size_t buflen)
{
	(void)r;
	randomGet(buf, buflen);
	return 0;
}

int getentropy(void *buf, size_t buflen)
{
	randomGet(buf, buflen);
	return 0;
}

posix_ulong posix_resolve_ipv4(const char *host)
{
	struct in_addr addr;
	if (inet_aton(host, &addr))
		return addr.s_addr;
	return 0;
}

int posix_upnp_forward_udp(unsigned short port, unsigned short preferred_port, posix_ulong *external_address,
	unsigned short *external_port, char *error, int error_size)
{
	(void)port;
	(void)preferred_port;
	(void)external_address;
	(void)external_port;
	if (error && error_size > 0)
		snprintf(error, error_size, "UPnP unsupported on Switch");
	return 0;
}

void posix_upnp_stop_forwarding_udp(unsigned short external_port)
{
	(void)external_port;
}

int posix_command_line_argument(int index, char *buffer, posix_ulong size)
{
	(void)index;
	(void)buffer;
	(void)size;
	return 0;
}

posix_ulong posix_process_id(void)
{
	return 1;
}

int posix_register_url_scheme(const char *scheme, const char *description)
{
	(void)scheme;
	(void)description;
	return 0;
}

int posix_user_secret(unsigned char *secret, int size)
{
	(void)secret;
	(void)size;
	return 0;
}

int posix_discord_connect(void)
{
	return -1;
}

int posix_discord_write(int handle, const void *buffer, int length)
{
	(void)handle;
	(void)buffer;
	(void)length;
	return -1;
}

int posix_discord_read(int handle, void *buffer, int length)
{
	(void)handle;
	(void)buffer;
	(void)length;
	return -1;
}

void posix_discord_close(int handle)
{
	(void)handle;
}
