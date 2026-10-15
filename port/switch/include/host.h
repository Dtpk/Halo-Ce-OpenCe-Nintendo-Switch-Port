/*
HOST.H

Nintendo Switch host definitions for Halo: Combat Evolved.
*/

#ifndef __HALO_SWITCH_HOST_H
#define __HALO_SWITCH_HOST_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <switch.h>
#include <switch/kernel/jit.h>

#include "halo_switch_abi.h"

#define PAGE_SIZE_4K       0x1000UL
#define DYNAMIC_POOL_START 0x89400000UL
#define DYNAMIC_POOL_SIZE  0x10000000UL /* 256 MB */
#define DYNAMIC_POOL_END   (DYNAMIC_POOL_START + DYNAMIC_POOL_SIZE)

/* Guest memory is 1:1 mapped in 32-bit virtual address space */
extern void *g_guest_memory_base;
extern void *g_guest_data_host_ptr;
extern void *g_guest_code_rw_addr;
extern void *g_guest_dyn_host_ptr;
extern Jit g_jit;
#define GUEST_BASE_OFFSET 0x80000000UL
#define GUEST_TO_HOST(addr) ((void*)(uintptr_t)(addr))

static inline void* GUEST_PTR_TO_HOST(uintptr_t addr) {
    void *ptr;
    __asm__("" : "=r"(ptr) : "0"((void*)addr));
    return ptr;
}

/* ---------- logging */

extern FILE *log_file;
extern pthread_mutex_t log_lock;
void host_logf(int priority, const char *format, ...) __attribute__((format(printf, 2, 3)));
void host_log(int priority, const char *text);
#define HOST_LOG_INFO 4
#define HOST_LOG_WARN 5
#define HOST_LOG_ERROR 6

void host_exit(int code) __attribute__((noreturn));
int host_errno(void);
void host_fatal(const char *format, ...) __attribute__((format(printf, 1, 2), noreturn));
void host_abort(const char *reason) __attribute__((noreturn));

/* ---------- guest memory */

int host_memory_preinit(void);
int host_memory_initialize(uint32_t image_base, uint32_t image_size);
int host_memory_transition_executable(uint32_t text_base, uint32_t text_size);
void *host_low_map(size_t size, int protection);
void host_low_unmap(void *address, size_t size);
int host_low_owns(uintptr_t address, size_t size);
long host_guest_mmap(uint64_t address, uint64_t size, int protection, int flags, int fd, int64_t offset);
long host_guest_munmap(uint64_t address, uint64_t size);
long host_guest_mprotect(uint64_t address, uint64_t size, int protection);

/* ---------- the guest image */

struct host_guest_image
{
	const struct halo_guest_header *header;
	uint32_t base, end;
};

extern struct host_guest_image host_image;

int host_load_image(const void *elf, size_t size);

/* ---------- entering guest code */

uint64_t call_on_stack(void *new_sp, void *fn, uint64_t a, uint64_t b, uint64_t c, uint64_t d);
uint32_t host_call_guest(uint32_t function, uint32_t a, uint32_t b, uint32_t c, uint32_t d);
int host_native_thread_create(void *(*function)(void *), void *argument, size_t stack_size);
void host_run_guest_main(uint32_t boot) __attribute__((noreturn));

uint32_t host_get_tp(void);
void host_set_tp(uint32_t thread);
int host_thread_create(uint32_t guest_thread, uint32_t stack_size);

/* ---------- memory watch */

void host_memory_watch_initialize(void);
void host_memory_watch_protect(uint32_t address, uint32_t size);
uint32_t host_memory_watch_generation(uint32_t address, uint32_t size);
uint32_t host_memory_watch_serial(void);
void host_memory_watch_prepare_write(uint32_t address, uint32_t size);
void host_memory_watch_forget(uint32_t address, uint32_t size);

/* ---------- import table & resolution */

void *host_resolve_import(const char *name);
void *host_gl_resolve(const char *name);

/* path callback for guest */
void host_android_path(int which, char *buffer, uint32_t size);
void translate_guest_path(const char *guest_path, char *host_path, size_t size);

/* system calls */
long long host_syscall(long long number, long long a, long long b, long long c,
	long long d, long long e, long long f);

#endif
