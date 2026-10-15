/*
HOST_LOADER.C

Loads the ARM64 ILP32 guest executable (halo_guest.elf) into memory,
resolves its import table, and prepares it for execution.
*/

#include "host.h"

#include <elf.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>

struct host_guest_image host_image;

static void missing_import(void)
{
	host_fatal("guest called a missing host import");
}

int host_load_image(const void *elf, size_t size)
{
	const Elf64_Ehdr *header = (const Elf64_Ehdr *)elf;
	const Elf64_Phdr *program_headers;
	uint64_t low = ~0ull, high = 0;
	int index;
	const struct halo_guest_header *guest_header;

	if (size < sizeof(Elf64_Ehdr) || memcmp(header->e_ident, ELFMAG, SELFMAG) != 0)
	{
		host_logf(HOST_LOG_ERROR, "invalid guest ELF image");
		return -1;
	}

	if (header->e_machine != EM_AARCH64)
	{
		host_logf(HOST_LOG_ERROR, "invalid guest ELF architecture (expected AArch64)");
		return -1;
	}
    host_logf(HOST_LOG_INFO, "host_load_image: headers validated");

	program_headers = (const Elf64_Phdr *)((const char *)elf + header->e_phoff);
	for (index = 0; index < header->e_phnum; index++)
	{
		const Elf64_Phdr *segment = &program_headers[index];

		if (segment->p_type != PT_LOAD || segment->p_memsz == 0)
			continue;
		if (segment->p_vaddr < low)
			low = segment->p_vaddr;
		if (segment->p_vaddr + segment->p_memsz > high)
			high = segment->p_vaddr + segment->p_memsz;
	}

	low &= ~0xfffull;
	high = (high + 0xfffull) & ~0xfffull;

	if (low != HALO_GUEST_IMAGE_BASE || high > 0x100000000ULL)
	{
		host_logf(HOST_LOG_ERROR, "unexpected guest address range: 0x%" PRIx64 "..0x%" PRIx64, low, high);
		return -1;
	}

    host_logf(HOST_LOG_INFO, "host_load_image: initializing memory at 0x%lx..0x%lx", (unsigned long)low, (unsigned long)high);
	if (host_memory_initialize((uint32_t)low, (uint32_t)(high - low)) != 0)
	{
		host_logf(HOST_LOG_ERROR, "failed to initialize guest memory");
		return -1;
	}

	/* Copy PT_LOAD segments to memory */
    host_logf(HOST_LOG_INFO, "host_load_image: copying %d segments...", header->e_phnum);
    fflush(stdout);
	for (index = 0; index < header->e_phnum; index++)
	{
		const Elf64_Phdr *segment = &program_headers[index];
		void *target;

		if (segment->p_type != PT_LOAD) continue;
        if (segment->p_memsz == 0) continue;
        
		target = GUEST_PTR_TO_HOST((uintptr_t)segment->p_vaddr);
		if (segment->p_filesz > 0)
		{
			if (segment->p_offset + segment->p_filesz > size)
			{
				host_logf(HOST_LOG_ERROR, "truncated PT_LOAD segment %d", index);
                fflush(stdout);
				return -1;
			}
			memcpy(target, (const char *)elf + segment->p_offset, segment->p_filesz);
		}
		if (segment->p_memsz > segment->p_filesz)
		{
            size_t zero_size = (size_t)(segment->p_memsz - segment->p_filesz);
            void* zero_target = (char *)target + segment->p_filesz;
			memset(zero_target, 0, zero_size);
		}
        host_logf(HOST_LOG_INFO, "  Segment %d loaded (0x%lx, size 0x%lx)", index, (unsigned long)segment->p_vaddr, (unsigned long)segment->p_memsz);
        fflush(stdout);
        consoleUpdate(NULL);
	}

	guest_header = (const struct halo_guest_header *)GUEST_PTR_TO_HOST(HALO_GUEST_IMAGE_BASE);
	host_image.header = guest_header;
	host_image.base = (uint32_t)low;
	host_image.end = (uint32_t)high;

	if (guest_header->magic != HALO_GUEST_MAGIC || guest_header->abi_version != HALO_GUEST_ABI_VERSION)
	{
		host_logf(HOST_LOG_ERROR, "invalid guest header: magic 0x%08x, abi %u",
			guest_header->magic, guest_header->abi_version);
		return -1;
	}

	/* Resolve imports */
	if (guest_header->import_table != 0)
	{
		uint64_t *import_table = (uint64_t *)GUEST_PTR_TO_HOST(guest_header->import_table);
		const char *import_name = (const char *)GUEST_PTR_TO_HOST(guest_header->import_names);
		uint32_t import_count = *(uint32_t *)GUEST_PTR_TO_HOST(guest_header->import_count);

		host_logf(HOST_LOG_INFO, "resolving %u guest imports...", import_count);
        fflush(stdout);
        consoleUpdate(NULL);
		for (index = 0; index < (int)import_count; index++)
		{
			void *resolved = host_resolve_import(import_name);
			if (!resolved)
			{
				host_logf(HOST_LOG_WARN, "    Unresolved import: %s", import_name);
                fflush(stdout);
                consoleUpdate(NULL);
				resolved = (void *)missing_import;
			}

			import_table[index] = (uint64_t)(uintptr_t)resolved;
			import_name += strlen(import_name) + 1;
		}
	}

	/* Flush data cache across data segment containing imports and tables */
    armDCacheFlush((void*)HALO_GUEST_DATA_BASE, (size_t)HALO_GUEST_DATA_SIZE);

    /* Stub out Bink cinematic playback directly in guest memory */
    uint32_t *bink_start = (uint32_t *)GUEST_PTR_TO_HOST(0x88063fc0);
    uint32_t *bink_init = (uint32_t *)GUEST_PTR_TO_HOST(0x88062fc0);
    *bink_start = 0xd65f03c0; /* AArch64 RET */
    *bink_init = 0xd65f03c0;  /* AArch64 RET */
    host_logf(HOST_LOG_INFO, "stubbed out Bink cinematic player");
    consoleUpdate(NULL);

    /* Transition guest code segment from RW (MapOwner) to RX (MapSlave) */
    host_logf(HOST_LOG_INFO, "transitioning code memory to executable...");
    fflush(stdout);
    consoleUpdate(NULL);
    if (host_memory_transition_executable(HALO_GUEST_CODE_BASE, HALO_GUEST_CODE_SIZE) != 0)
    {
        host_logf(HOST_LOG_ERROR, "failed to transition guest code to executable");
        return -1;
    }

	return 0;
}
