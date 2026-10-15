/*
HOST_THREAD.C

Thread creation and execution for guest code on Nintendo Switch.
Every thread running guest ILP32 code must have its stack below 4 GB.
*/

#include "host.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAIN_STACK_SIZE (16 * 1024 * 1024)

static __thread uint32_t guest_tp;
static __thread void *t_guest_stack = NULL;
static __thread size_t t_guest_stack_size = 0;

uint32_t host_get_tp(void)
{
	return guest_tp;
}

void host_set_tp(uint32_t thread)
{
	host_logf(HOST_LOG_INFO, ">>> host_set_tp called with thread=0x%08x", thread);
	consoleUpdate(NULL);
	guest_tp = thread;
}

typedef uint32_t (*guest_function)(uint32_t, uint32_t, uint32_t, uint32_t);

static inline int on_guest_stack(void)
{
	uintptr_t sp;
	__asm__ volatile("mov %0, sp" : "=r"(sp));
	return sp < 0x100000000ULL;
}

struct guest_call_args
{
	uint32_t function;
	uint32_t a, b, c, d;
	uint32_t result;
};

static uint32_t guest_call_worker(struct guest_call_args *args)
{
	host_logf(HOST_LOG_INFO, ">>> guest_call_worker function=0x%08x", args->function);
	
	if (args->function != (host_image.header ? host_image.header->thread_start : 0) &&
	    !guest_tp && host_image.header && host_image.header->thread_attach)
	{
		((guest_function)(uintptr_t)host_image.header->thread_attach)(0, 0, 0, 0);
	}

	args->result = ((guest_function)(uintptr_t)args->function)(args->a, args->b, args->c, args->d);
	return args->result;
}

uint32_t host_call_guest(uint32_t function, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
	if (on_guest_stack())
	{
		struct guest_call_args args = { function, a, b, c, d, 0 };
		return guest_call_worker(&args);
	}

	if (!t_guest_stack)
	{
		t_guest_stack_size = 1024 * 1024;
		t_guest_stack = host_low_map(t_guest_stack_size, 0);
		if (!t_guest_stack)
		{
			host_fatal("failed to allocate thread guest stack for host_call_guest");
		}
	}

	void *call_sp = (char *)t_guest_stack + t_guest_stack_size - 64;
	call_sp = (void *)((uintptr_t)call_sp & ~15ULL);

	struct guest_call_args args = { function, a, b, c, d, 0 };
	call_on_stack(call_sp, (void *)guest_call_worker, (uint64_t)(uintptr_t)&args, 0, 0, 0);
	return args.result;
}

void host_run_guest_main(uint32_t boot)
{
	uint32_t guest_entry = (host_image.header && host_image.header->start) ? host_image.header->start : HALO_GUEST_IMAGE_BASE;
	void* host_entry = GUEST_PTR_TO_HOST(guest_entry);
	void* host_boot = GUEST_PTR_TO_HOST(boot);
	struct halo_guest_boot *boot_struct = (struct halo_guest_boot *)host_boot;

	host_logf(HOST_LOG_INFO, "Boot struct at %p: argc=%d, argv=0x%08x, env=0x%08x", 
		host_boot, boot_struct->argc, boot_struct->argv, boot_struct->environment);

	/* Allocate 16MB main guest stack in 32-bit low memory (< 4GB) */
	void *guest_stack = host_low_map(MAIN_STACK_SIZE, 0);
	if (!guest_stack)
	{
		host_fatal("failed to allocate main guest stack (16 MB)");
	}

	void *guest_sp = (char *)guest_stack + MAIN_STACK_SIZE - 64;
	guest_sp = (void *)((uintptr_t)guest_sp & ~15ULL);

	uintptr_t host_sp_before;
	__asm__ volatile("mov %0, sp" : "=r"(host_sp_before));
	host_logf(HOST_LOG_INFO, "Host SP before stack switch: %p", (void *)host_sp_before);
	host_logf(HOST_LOG_INFO, "Main guest stack mapped: %p..%p (Target SP: %p, strictly < 4GB: %s)",
		guest_stack, (char *)guest_stack + MAIN_STACK_SIZE, guest_sp,
		((uintptr_t)guest_sp < 0x100000000ULL) ? "YES" : "NO");

	host_logf(HOST_LOG_INFO, "switching stack to %p and jumping to guest entry point at 0x%08x...",
		guest_sp, guest_entry);

	unsigned char *p = (unsigned char *)host_entry;
	host_logf(HOST_LOG_INFO, "Entry point bytes: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
		p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
	host_logf(HOST_LOG_INFO, "jumping to guest entry point via call_on_stack now...");
	fflush(stdout);
	printf("[HALO] JUMPING NOW (Target: %p, Stack: %p)...\n", host_entry, guest_sp);
	consoleUpdate(NULL);

	/* Switch SP to guest stack < 4GB and execute entry point */
	call_on_stack(guest_sp, host_entry, (uint64_t)(uintptr_t)host_boot, 0, 0, 0);

	host_fatal("the guest returned from __guest_start");
}

struct thread_context
{
	void *(*function)(void *);
	void *argument;
	void *guest_stack;
	size_t guest_stack_size;
};

static void *thread_wrapper(void *context)
{
	struct thread_context ctx = *(struct thread_context *)context;
	free(context);

	host_logf(HOST_LOG_INFO, "secondary thread started on host pthread %p, switching to guest stack %p...", 
		(void *)pthread_self(), ctx.guest_stack);
	guest_tp = 0;

	void *thread_sp = (char *)ctx.guest_stack + ctx.guest_stack_size - 64;
	thread_sp = (void *)((uintptr_t)thread_sp & ~15ULL);

	call_on_stack(thread_sp, ctx.function, (uint64_t)(uintptr_t)ctx.argument, 0, 0, 0);

	host_logf(HOST_LOG_INFO, "secondary thread function returned");
	guest_tp = 0;

	if (ctx.guest_stack)
		host_low_unmap(ctx.guest_stack, ctx.guest_stack_size);
	return NULL;
}

int host_native_thread_create(void *(*function)(void *), void *argument, size_t stack_size)
{
	struct thread_context *ctx;
	pthread_t thread;
	int error;

	size_t real_stack_size = stack_size > 0x100000 ? stack_size : 0x100000;
	real_stack_size = (real_stack_size + 0xFFFF) & ~0xFFFF;

	void *guest_stack = host_low_map(real_stack_size, 0);
	if (!guest_stack)
	{
		host_logf(HOST_LOG_ERROR, "failed to allocate guest stack for thread (%zu bytes)", real_stack_size);
		return ENOMEM;
	}

	ctx = malloc(sizeof(*ctx));
	if (!ctx)
	{
		host_logf(HOST_LOG_ERROR, "malloc for thread_context failed");
		host_low_unmap(guest_stack, real_stack_size);
		return ENOMEM;
	}

	ctx->function = function;
	ctx->argument = argument;
	ctx->guest_stack = guest_stack;
	ctx->guest_stack_size = real_stack_size;

	error = pthread_create(&thread, NULL, thread_wrapper, ctx);
	if (error)
	{
		host_logf(HOST_LOG_ERROR, "pthread_create failed: %d", error);
		host_low_unmap(guest_stack, real_stack_size);
		free(ctx);
		return error;
	}

	pthread_detach(thread);
	return 0;
}

static void *guest_thread_main(void *guest_thread)
{
	host_call_guest(host_image.header->thread_start, (uint32_t)(uintptr_t)guest_thread, 0, 0, 0);
	return NULL;
}

int host_thread_create(uint32_t guest_thread, uint32_t stack_size)
{
	return host_native_thread_create(guest_thread_main, (void *)(uintptr_t)guest_thread, stack_size);
}
