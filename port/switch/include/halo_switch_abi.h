/*
HALO_SWITCH_ABI.H

The contract between the Switch host and the ARM64 ILP32 guest (halo_guest.elf).
This matches the ABI contract used in the Android port so that halo_guest.elf
is 100% binary-compatible between platforms.
*/

#ifndef __HALO_SWITCH_ABI_H
#define __HALO_SWITCH_ABI_H

#include <stdint.h>

/* The guest image is linked to run at 0x88000000 (2.125 GB) */
#define HALO_GUEST_IMAGE_BASE 0x88000000u

/* Guest code and data layout */
#define HALO_GUEST_CODE_BASE  0x88000000u
#define HALO_GUEST_CODE_SIZE  0x00710000u /* .guest_header + .text + .rodata (~7.1 MB) */
#define HALO_GUEST_DATA_BASE  0x88710000u
#define HALO_GUEST_DATA_SIZE  0x00AF0000u /* .data + .guest_init + .bss (~11 MB) */

/* The Xbox contiguous memory window (128 MB) at 0x80000000 (2.0 GB) */
#define HALO_GUEST_WINDOW_BASE 0x80000000u
#define HALO_GUEST_WINDOW_SIZE 0x08000000u

#define HALO_GUEST_MAGIC 0x4f4c4148u /* 'HALO' */
#define HALO_GUEST_ABI_VERSION 1

/* Header at HALO_GUEST_IMAGE_BASE */
struct halo_guest_header
{
	uint32_t magic;
	uint32_t abi_version;
	uint32_t image_end;          /* end of .bss */
	uint32_t import_table;       /* uint64_t[import_count], filled by the host */
	uint32_t import_names;       /* import_count NUL-terminated names */
	uint32_t import_count;       /* address of a uint32_t holding the count */
	uint32_t start;              /* void __guest_start(struct halo_guest_boot *) */
	uint32_t thread_start;       /* void __guest_thread_start(uint32_t thread) */
	uint32_t thread_attach;      /* uint32_t __guest_thread_attach(void) */
	uint32_t init_array_start;   /* void (*)(void) entries, 4 bytes each */
	uint32_t init_array_end;
};

/* Description of the process handed to __guest_start */
struct halo_guest_boot
{
	uint32_t argc;
	uint32_t argv;               /* char ** in guest memory */
	uint32_t environment;        /* char ** in guest memory, NULL-terminated */
	uint32_t page_size;
};

#endif
