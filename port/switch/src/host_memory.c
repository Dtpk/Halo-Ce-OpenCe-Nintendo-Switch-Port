/*
HOST_MEMORY.C

Memory management for Nintendo Switch (Horizon OS / Atmosphere).
All guest addresses must lie below 4 GB (< 0x100000000).
*/

#include "host.h"

#include <errno.h>
#include <inttypes.h>
#include <malloc.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>

#ifndef MAP_FIXED
#define MAP_FIXED 0x10
#endif

/* Allocate 2 GB heap via libnx */
u64 __nx_heap_size = 0x80000000ULL;

#define PAGE_SIZE_4K       0x1000UL
#define DYNAMIC_POOL_START 0x89400000UL
#define DYNAMIC_POOL_SIZE  0x10000000UL /* 256 MB */
#define DYNAMIC_POOL_END   (DYNAMIC_POOL_START + DYNAMIC_POOL_SIZE)
#define TOTAL_PAGES        (DYNAMIC_POOL_SIZE / PAGE_SIZE_4K)

static uint8_t page_bitmap[TOTAL_PAGES / 8];
static pthread_mutex_t memory_lock = PTHREAD_MUTEX_INITIALIZER;
static int memory_preinitialized = 0;

static int is_page_used(size_t page)
{
	return (page_bitmap[page / 8] >> (page % 8)) & 1;
}

static void mark_page(size_t page, int used)
{
	if (used)
		page_bitmap[page / 8] |= (1 << (page % 8));
	else
		page_bitmap[page / 8] &= ~(1 << (page % 8));
}

/* CodeMemory handles */
static Handle g_guest_window_handle = INVALID_HANDLE;
static Handle g_guest_code_handle = INVALID_HANDLE;
static Handle g_guest_data_handle = INVALID_HANDLE;
static Handle g_guest_dyn_handle = INVALID_HANDLE;
void* g_guest_code_rw_addr = NULL;
Jit g_jit;
void *g_guest_memory_base = NULL;
void *g_guest_data_host_ptr = NULL;
void *g_guest_dyn_host_ptr = NULL;

int host_memory_preinit(void)
{
    Result rc;
    #define ALIGN_64K(x) (((x) + 0xFFFF) & ~0xFFFF)
    
    size_t window_size = ALIGN_64K(HALO_GUEST_WINDOW_SIZE);
    size_t code_size   = ALIGN_64K(HALO_GUEST_CODE_SIZE);
    size_t data_size   = ALIGN_64K(HALO_GUEST_DATA_SIZE);
    size_t dyn_size    = ALIGN_64K(DYNAMIC_POOL_SIZE);

    host_logf(HOST_LOG_INFO, "initializing guest memory with 64KB alignment (window: 0x%zx, code: 0x%zx, data: 0x%zx, dyn: 0x%zx)...",
              window_size, code_size, data_size, dyn_size);

    /* 0. Xbox Contiguous Memory Window (128 MB) at 0x80000000 */
    void* window_buffer = memalign(0x10000, window_size);
    if (!window_buffer) host_fatal("failed to allocate xbox window buffer");
    printf("[HALO] window_buffer allocated at %p, size 0x%zx\n", window_buffer, window_size);
    fflush(stdout);

    rc = svcCreateCodeMemory(&g_guest_window_handle, window_buffer, window_size);
    if (R_FAILED(rc)) {
        printf("[HALO] svcCreateCodeMemory(window) failed: 0x%x\n", rc);
        fflush(stdout);
        host_fatal("svcCreateCodeMemory(window) failed: 0x%x", rc);
    }

    host_logf(HOST_LOG_INFO, "mapping xbox window (RW): handle=0x%x, dst=%p, size=0x%zx",
              g_guest_window_handle, (void*)(uintptr_t)HALO_GUEST_WINDOW_BASE, window_size);
    rc = svcControlCodeMemory(g_guest_window_handle, CodeMapOperation_MapOwner, (void *)(uintptr_t)HALO_GUEST_WINDOW_BASE, window_size, Perm_Rw);
    if (R_FAILED(rc)) host_fatal("svcControlCodeMemory(window MapOwner) failed: 0x%x", rc);

    memset(GUEST_PTR_TO_HOST(HALO_GUEST_WINDOW_BASE), 0, window_size);
    host_logf(HOST_LOG_INFO, "Xbox contiguous memory window zeroed at 0x%08x", HALO_GUEST_WINDOW_BASE);

    /* 1. Guest Code Segment (CodeMemory) */
    void* code_buffer = memalign(0x10000, code_size);
    if (!code_buffer) host_fatal("failed to allocate code buffer");
    printf("[HALO] code_buffer allocated at %p, size 0x%zx\n", code_buffer, code_size);
    fflush(stdout);

    rc = svcCreateCodeMemory(&g_guest_code_handle, code_buffer, code_size);
    if (R_FAILED(rc)) {
        printf("[HALO] svcCreateCodeMemory(code) failed: 0x%x\n", rc);
        fflush(stdout);
        host_fatal("svcCreateCodeMemory(code) failed: 0x%x", rc);
    }

    host_logf(HOST_LOG_INFO, "mapping code (RW): handle=0x%x, dst=%p, size=0x%zx", g_guest_code_handle, (void*)HALO_GUEST_CODE_BASE, code_size);
    rc = svcControlCodeMemory(g_guest_code_handle, CodeMapOperation_MapOwner, (void *)(uintptr_t)HALO_GUEST_CODE_BASE, code_size, Perm_Rw);
    if (R_FAILED(rc)) host_fatal("svcControlCodeMemory(code MapOwner) failed: 0x%x", rc);
    
    g_guest_code_rw_addr = (void *)(uintptr_t)HALO_GUEST_CODE_BASE;
    g_jit.rw_addr = g_guest_code_rw_addr;

    /* 2. Guest Data Segment (CodeMemory) */
    void* data_buffer = memalign(0x10000, data_size);
    if (!data_buffer) host_fatal("failed to allocate data buffer");
    printf("[HALO] data_buffer allocated at %p, size 0x%zx\n", data_buffer, data_size);
    fflush(stdout);

    rc = svcCreateCodeMemory(&g_guest_data_handle, data_buffer, data_size);
    if (R_FAILED(rc)) {
        printf("[HALO] svcCreateCodeMemory(data) failed: 0x%x\n", rc);
        fflush(stdout);
        host_fatal("svcCreateCodeMemory(data) failed: 0x%x", rc);
    }

    host_logf(HOST_LOG_INFO, "mapping data (RW): handle=0x%x, dst=%p, size=0x%zx", g_guest_data_handle, (void*)HALO_GUEST_DATA_BASE, data_size);
    rc = svcControlCodeMemory(g_guest_data_handle, CodeMapOperation_MapOwner, (void *)(uintptr_t)HALO_GUEST_DATA_BASE, data_size, Perm_Rw);
    if (R_FAILED(rc)) host_fatal("svcControlCodeMemory(data MapOwner) failed: 0x%x", rc);

    g_guest_data_host_ptr = (void *)(uintptr_t)HALO_GUEST_DATA_BASE;

    /* 3. Dynamic Pool */
    void* dyn_buffer = memalign(0x10000, dyn_size);
    if (!dyn_buffer) host_fatal("failed to allocate dyn buffer");
    memset(dyn_buffer, 0, dyn_size);
    printf("[HALO] dyn_buffer allocated at %p, size 0x%zx\n", dyn_buffer, dyn_size);
    fflush(stdout);

    rc = svcCreateCodeMemory(&g_guest_dyn_handle, dyn_buffer, dyn_size);
    if (R_FAILED(rc)) {
        printf("[HALO] svcCreateCodeMemory(dyn) failed: 0x%x\n", rc);
        fflush(stdout);
        host_fatal("svcCreateCodeMemory(dyn) failed: 0x%x", rc);
    }

    host_logf(HOST_LOG_INFO, "mapping dynamic pool (RW): handle=0x%x, dst=%p, size=0x%zx",
              g_guest_dyn_handle, (void*)(uintptr_t)DYNAMIC_POOL_START, dyn_size);
    rc = svcControlCodeMemory(g_guest_dyn_handle, CodeMapOperation_MapOwner, (void *)(uintptr_t)DYNAMIC_POOL_START, dyn_size, Perm_Rw);
    if (R_FAILED(rc)) host_fatal("svcControlCodeMemory(dyn MapOwner) failed: 0x%x", rc);

    g_guest_dyn_host_ptr = (void *)(uintptr_t)DYNAMIC_POOL_START;
    host_logf(HOST_LOG_INFO, "dynamic pool mapped at 0x%08x", (uint32_t)DYNAMIC_POOL_START);

    memset(page_bitmap, 0, sizeof(page_bitmap));
    memory_preinitialized = 1;
    host_logf(HOST_LOG_INFO, "guest segments successfully reserved and allocated.");
    return 0;
}

int host_memory_transition_executable(uint32_t text_base, uint32_t text_size)
{
    Result rc;
	text_size = (text_size + 0xFFFF) & ~0xFFFF;
	text_base = text_base & ~0xFFFF;

	host_logf(HOST_LOG_INFO, "transitioning code memory at 0x%08x (size 0x%x) to executable...",
		text_base, text_size);
    fflush(stdout);

    /* Flush data cache on RW mapping before transitioning */
    armDCacheFlush((void *)(uintptr_t)text_base, (size_t)text_size);

    /* Unmap Owner RW mapping */
    rc = svcControlCodeMemory(g_guest_code_handle, CodeMapOperation_UnmapOwner, (void *)(uintptr_t)text_base, text_size, 0);
    host_logf(HOST_LOG_INFO, "svcControlCodeMemory(UnmapOwner) -> 0x%x", rc);
    fflush(stdout);
    if (R_FAILED(rc)) host_fatal("svcControlCodeMemory(UnmapOwner) failed: 0x%x", rc);

    /* Map Slave RX mapping at the exact same address */
    rc = svcControlCodeMemory(g_guest_code_handle, CodeMapOperation_MapSlave, (void *)(uintptr_t)text_base, text_size, Perm_Rx);
    host_logf(HOST_LOG_INFO, "svcControlCodeMemory(MapSlave, RX) -> 0x%x", rc);
    fflush(stdout);
    if (R_FAILED(rc)) host_fatal("svcControlCodeMemory(MapSlave, RX) failed: 0x%x", rc);

    armICacheInvalidate((void*)(uintptr_t)text_base, (size_t)text_size);
    host_logf(HOST_LOG_INFO, "code at 0x%08x successfully transitioned to RX via CodeMemory", text_base);
    fflush(stdout);
	return 0;
}

int host_memory_initialize(uint32_t image_base, uint32_t image_size)
{
	(void)image_base;
	(void)image_size;

	if (memory_preinitialized)
	{
		host_logf(HOST_LOG_INFO, "memory already preinitialized, skipping re-mapping");
		return 0;
	}

	if (host_memory_preinit() != 0)
		return -1;

	return 0;
}

void *host_low_map(size_t size, int protection)
{
	size_t pages_needed = (size + PAGE_SIZE_4K - 1) / PAGE_SIZE_4K;
	size_t run = 0;
	size_t start_page = 0;
	size_t i;
	void *address = NULL;

	(void)protection;
	if (!memory_preinitialized)
		host_memory_preinit();

	pthread_mutex_lock(&memory_lock);
	for (i = 0; i < TOTAL_PAGES; i++)
	{
		if (!is_page_used(i))
		{
			if (run == 0)
				start_page = i;
			run++;
			if (run == pages_needed)
			{
				size_t j;
				for (j = start_page; j < start_page + pages_needed; j++)
					mark_page(j, 1);

				address = (void *)(uintptr_t)(DYNAMIC_POOL_START + start_page * PAGE_SIZE_4K);
				break;
			}
		}
		else
		{
			run = 0;
		}
	}
	pthread_mutex_unlock(&memory_lock);

	if (!address)
	{
		host_logf(HOST_LOG_ERROR, "out of 32-bit low memory: requested %zu bytes", size);
		return NULL;
	}

	/* Anonymous memory mapping contract: newly mapped pages MUST be zeroed */
	memset(address, 0, pages_needed * PAGE_SIZE_4K);

	return address;
}

void host_low_unmap(void *address, size_t size)
{
	uintptr_t addr = (uintptr_t)address;
	size_t pages, start_page, i;

	if (addr < DYNAMIC_POOL_START || addr >= DYNAMIC_POOL_END)
		return;

	start_page = (addr - DYNAMIC_POOL_START) / PAGE_SIZE_4K;
	pages = (size + PAGE_SIZE_4K - 1) / PAGE_SIZE_4K;

	/* Clear memory upon release so subsequent allocations don't contain stale data */
	memset(address, 0, pages * PAGE_SIZE_4K);

	pthread_mutex_lock(&memory_lock);
	for (i = start_page; i < start_page + pages && i < TOTAL_PAGES; i++)
		mark_page(i, 0);
	pthread_mutex_unlock(&memory_lock);
}

int host_low_owns(uintptr_t address, size_t size)
{
    /* Check if within Xbox contiguous window, guest image, or dynamic pool */
	if (address >= HALO_GUEST_WINDOW_BASE && (address + size) <= (HALO_GUEST_WINDOW_BASE + HALO_GUEST_WINDOW_SIZE))
		return 1;
	if (address >= HALO_GUEST_IMAGE_BASE && (address + size) <= (HALO_GUEST_DATA_BASE + HALO_GUEST_DATA_SIZE))
		return 1;
	if (address >= DYNAMIC_POOL_START && (address + size) <= DYNAMIC_POOL_END)
		return 1;
	return 0;
}

#ifndef MAP_FIXED
#define MAP_FIXED 0x10
#endif
#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif
#ifndef PROT_NONE
#define PROT_NONE 0
#endif

long host_guest_mmap(uint64_t address, uint64_t size, int protection, int flags, int fd, int64_t offset)
{
	(void)fd;
	(void)offset;

	if (size == 0)
		return -EINVAL;

	if (flags & (MAP_FIXED | MAP_FIXED_NOREPLACE))
	{
		if (address + size > 0x100000000ULL)
			return -ENOMEM;
            
		if (!host_low_owns((uintptr_t)address, (size_t)size))
		{
			host_logf(HOST_LOG_ERROR, "host_guest_mmap: range 0x%llx (size 0x%llx) out of guest heap",
				(unsigned long long)address, (unsigned long long)size);
			return -ENOMEM;
		}

		/* Zero the mapped area for newly committed read/write allocations, but avoid re-zeroing PROT_NONE reservations or RX code */
		if (protection != PROT_NONE && !(address >= HALO_GUEST_CODE_BASE && address < (HALO_GUEST_CODE_BASE + HALO_GUEST_CODE_SIZE)))
		{
			memset(GUEST_PTR_TO_HOST(address), 0, (size_t)size);
		}
		host_logf(HOST_LOG_INFO, "host_guest_mmap: fixed map at 0x%llx (size 0x%llx, prot 0x%x)",
			(unsigned long long)address, (unsigned long long)size, protection);
		return (long)address;
	}

	void *mapped = host_low_map((size_t)size, protection);
	if (!mapped)
	{
		host_logf(HOST_LOG_ERROR, "host_guest_mmap: dynamic map failed for size 0x%llx", (unsigned long long)size);
		return -ENOMEM;
	}
	host_logf(HOST_LOG_INFO, "host_guest_mmap: dynamic map at %p (size 0x%llx, prot 0x%x)",
		mapped, (unsigned long long)size, protection);
	return (long)(uintptr_t)mapped;
}

long host_guest_munmap(uint64_t address, uint64_t size)
{
	if (address >= DYNAMIC_POOL_START && address < DYNAMIC_POOL_END)
		host_low_unmap((void *)(uintptr_t)address, (size_t)size);
	return 0;
}

long host_guest_mprotect(uint64_t address, uint64_t size, int protection)
{
	(void)address;
	(void)size;
	(void)protection;
	return 0;
}

/* ---------- Texture Memory Watch ---------- */

#define WATCH_PAGE_COUNT (HALO_GUEST_WINDOW_SIZE / PAGE_SIZE_4K)
static uint32_t page_generations[WATCH_PAGE_COUNT];
static volatile uint32_t current_serial = 1;

static uint32_t watch_page_index(uint32_t address)
{
	return (address - HALO_GUEST_WINDOW_BASE) / PAGE_SIZE_4K;
}

void host_memory_watch_initialize(void)
{
	memset(page_generations, 0, sizeof(page_generations));
	current_serial = 1;
}

void host_memory_watch_protect(uint32_t address, uint32_t size)
{
	(void)address;
	(void)size;
}

uint32_t host_memory_watch_generation(uint32_t address, uint32_t size)
{
	uint32_t first, last, page, newest = 0;

	if (!size || address < HALO_GUEST_WINDOW_BASE)
		return 0;
	first = watch_page_index(address);
	last = watch_page_index(address + size - 1);
	if (last >= WATCH_PAGE_COUNT)
		last = WATCH_PAGE_COUNT - 1;

	for (page = first; page <= last; page++)
	{
		if (page_generations[page] > newest)
			newest = page_generations[page];
	}
	return newest;
}

uint32_t host_memory_watch_serial(void)
{
	return current_serial;
}

void host_memory_watch_prepare_write(uint32_t address, uint32_t size)
{
	uint32_t first, last, page, gen;

	if (!size || address < HALO_GUEST_WINDOW_BASE)
		return;
	first = watch_page_index(address);
	last = watch_page_index(address + size - 1);
	if (last >= WATCH_PAGE_COUNT)
		last = WATCH_PAGE_COUNT - 1;

	gen = __sync_add_and_fetch(&current_serial, 1);
	for (page = first; page <= last; page++)
		page_generations[page] = gen;
}

void host_memory_watch_forget(uint32_t address, uint32_t size)
{
	uint32_t first, last, page, gen;

	if (!size || address < HALO_GUEST_WINDOW_BASE)
		return;
	first = watch_page_index(address);
	last = watch_page_index(address + size - 1);
	if (last >= WATCH_PAGE_COUNT)
		last = WATCH_PAGE_COUNT - 1;

	gen = __sync_add_and_fetch(&current_serial, 1);
	for (page = first; page <= last; page++)
		page_generations[page] = gen;
}
