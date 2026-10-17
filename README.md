# Halo: Combat Evolved — Nintendo Switch Port

THIS IS NOT COMPLETE AND NEEDS support!!! Any help is appreciated in getting this running!!!

"I may abandon this or work on it as a personal project someone beat me too this by a few days and their port is much further but if you still wish to help with this i'm here for it so reach out"

So far this can get to main execution and currently I just need some help taking this further and getting free of 
the start up crash loop this project been in its the furthest its been yet. I provided my halo.log generated before the crash in case it helps debug but I am sure more is needed then that log.

"EXTRA notes!!! I ran most of my devkitpro work inside a podman isolated instance so the files were kinda a mess lots of workarounds to do what I needed in a docker like setup. I progressed from simple green screen graphic's to getting the main elf execution to occur. Then as time went on I let gemini llm take over the podman instance" 

The build successfully passed segment loading, resolved all 192 imports, and transitioned the code memory to RX before a black screen and software closed cause of a error prompt.

A native Nintendo Switch port of **Halo: Combat Evolved**, built on the [halo-ce-universal](https://github.com/cybersecurity/halo-ce-universal) engine decompilation. 

This repository contains the native AArch64 host execution wrapper, the AArch64 ILP32 translation layer, virtual memory subsystem, static OpenGL ES 3.2 dispatch table, and POSIX/musl syscall emulation designed to run Halo: Combat Evolved natively on Horizon OS via libnx.

---

## Table of Contents
1. [Architecture Overview](#architecture-overview)
   - [AArch64 ILP32 Translation Layer](#1-aarch64-ilp32-translation-layer)
   - [Memory Architecture & Windows](#2-memory-architecture--virtual-windows)
   - [Static OpenGL ES 3.2 Dispatch Table](#3-static-opengl-es-32-dispatch-table)
   - [System Call Emulation Layer](#4-system-call-emulation-layer)
   - [Diagnostics & Exception Handling](#5-diagnostics--exception-handling)
2. [Repository Structure](#repository-structure)
3. [Building the Port](#building-the-port)
   - [Option A: Native devkitPro Build](#option-a-native-devkitpro-build)
   - [Option B: Docker Build (Recommended)](#option-b-docker-build-recommended)
   - [Prerequisites & Toolchain Setup](#prerequisites--toolchain-setup)
4. [Nintendo Switch Setup & SD Card Layout](#nintendo-switch-setup--sd-card-layout)
   - [Required Map Files](#required-map-files)
   - [Critical: Launching with Full RAM (Title Redirection)](#critical-launching-with-full-ram-title-redirection)
5. [Controls & Audio](#controls--audio)
6. [Troubleshooting & Logs](#troubleshooting--logs)

---

## Architecture Overview

Halo: Combat Evolved was originally authored for the 32-bit x86 Xbox architecture. The `halo-ce-universal` project compiles the reverse-engineered engine into an AArch64 ILP32 (`arm64_32`) ELF binary (`halo_guest.elf`) utilizing musl libc.

The Nintendo Switch port couples this 32-bit guest engine with a native AArch64 LP64 host wrapper (`halo.nro`) running on Horizon OS.

```
┌─────────────────────────────────────────────────────────────────┐
│                     Nintendo Switch (Horizon OS)                │
├─────────────────────────────────────────────────────────────────┤
│  Host Wrapper (halo.nro - AArch64 LP64)                         │
│  ├── libnx (Kernel, CodeMemory, Applet, Exception Handler)     │
│  ├── Switch Mesa (deko3d / nouveau / EGL / GLES 3.2)            │
│  ├── Static OpenGL Dispatch Table (98+ hostgl_* entry points)   │
│  ├── SDL2 (Handheld/Docked 720p/1080p, Joy-Con / Pro Input)    │
│  └── musl Syscall Emulation Layer (openat, futex, mmap, etc.)  │
├─────────────────────────────────────────────────────────────────┤
│  Translation Bridge (call_on_stack.s)                           │
│  └── AAPCS64 register preservation & 32-bit low stack pivot     │
├─────────────────────────────────────────────────────────────────┤
│  Guest Executable (halo_guest.elf - AArch64 ILP32 / musl libc)  │
│  ├── Xbox Direct3D 8 Contiguous Memory Window (0x80000000)      │
│  ├── Guest Code Execution via CodeMemory (0x88000000)           │
│  └── Dynamic Memory Pool (0x89400000 - zero-initialized)        │
└─────────────────────────────────────────────────────────────────┘
```

### 1. AArch64 ILP32 Translation Layer

* **32-Bit Pointer Constraints:** Although executed on an ARMv8-A 64-bit CPU (Cortex-A57), the guest engine pointers are strictly 32-bit (`uint32_t`). When launched from standard 64-bit userland, stack pointers reside above 4 GB (e.g., `sp = 0x306dc089a0`). Any 32-bit pointer casting or local variable calculation truncates the upper 32 bits, generating immediate unmapped Data Abort faults.
* **Stack Switching Trampoline (`call_on_stack.s`):** A custom assembly trampoline pivots the active stack pointer into pre-allocated 32-bit low virtual memory (< 4 GB) before invoking guest entry points or callbacks:
  ```assembly
  call_on_stack(new_sp, target_fn, arg0, arg1, arg2, arg3)
  ```
  The trampoline preserves all AAPCS64 callee-saved registers (`x19`–`x30`, `d8`–`d15`), transitions `sp` to the 32-bit low stack, branches to the function, and restores the original host environment upon return.
* **Thread Context Transitions (`host_thread.c`):** Both the main thread (16 MB stack) and worker threads (such as `cache_file_windows_thread_proc`, 1 MB stack) run on low stacks managed by `host_low_map`. Dynamic stack checking in `host_call_guest` transparently switches non-guest threads (such as SDL audio and input callbacks) onto a low stack when calling back into guest code.

### 2. Memory Architecture & Virtual Windows

Halo CE on Switch manages approximately 2 GB of virtual address space:

| Virtual Address Range | Size | Allocation / Role |
|---|---|---|
| `0x80000000` – `0x88000000` | 128 MB | **Xbox Contiguous Memory Window** (Direct3D 8 resources) |
| `0x88000000` – `0x891c0000` | ~18 MB | **Guest Code Memory** (mapped via `svcControlCodeMemory`) |
| `0x891c0000` – `0x89400000` | ~2.2 MB | Boot arguments, environment list, and host control structs |
| `0x89400000` – `0x99400000` | 256 MB | **Dynamic Memory Pool** (musl libc `malloc-ng` heap) |
| `< 4 GB` (Low Virtual) | 16 MB + 1 MB | **Low-Memory Stacks** for main and worker threads |

* **Xbox Contiguous Physical Memory Window (`0x80000000`):** Direct3D 8 on the Xbox relies on contiguous physical allocations (`platform_contiguous_alloc`) for textures, 256-entry palettes, vertex buffers, and index buffers. During engine boot, `contiguous_arena_reserve` calls:
  ```c
  mmap((void *)0x80000000, 0x8000000, PROT_NONE,
       MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
  ```
  The host translation layer (`host_memory.c`) intercepts `MAP_FIXED` (`0x10`) and `MAP_FIXED_NOREPLACE` (`0x100000`), ensuring the exact `0x80000000` address is reserved, setting `arena_reserved = TRUE` and enabling all subsequent graphics allocations without out-of-memory panics.
* **Guest Code Memory Permissions:** The guest ELF segments are extracted into memory and registered with Horizon OS using `svcControlCodeMemory` (RX for code segments, RW for data/bss).
* **musl `malloc-ng` Zero-Page Guarantees:** musl libc's modern memory allocator performs runtime integrity checks in `enframe()` (`assert(!p[-4])`). The host zero-initializes the dynamic pool buffer and all mapped low pages with `memset`, ensuring compliance with POSIX anonymous mapping expectations.

### 3. Static OpenGL ES 3.2 Dispatch Table

* **The Problem on Switch Mesa:** Nintendo Switch Mesa (`libGLESv2` / `switch-mesa` / `deko3d` / `libdrm_nouveau`) follows the specification where `eglGetProcAddress()` is strictly designated for extension functions. Querying core OpenGL ES symbols (such as `glClear`, `glViewport`, `glBindTexture`, `glGenBuffers`) via `eglGetProcAddress` prior to creating an active EGL context returns `NULL`.
* **Loader Resolution Fault:** Because the guest loader processes all 192 guest imports upfront before window and context initialization, dynamic lookup caused all 98 `hostgl_*` graphics routines to resolve to `NULL`, triggering branch-to-NULL CPU exceptions upon engine initialization.
* **The Solution (`host_gl.c`):** A static dispatch table connects all 98+ `hostgl_*` symbols directly to compiled `libGLESv2` entry points at link time. Dynamic resolution is retained only as a fallback for vendor extensions. All 192 guest imports are guaranteed to resolve to valid executable function pointers.

### 4. System Call Emulation Layer

The host directly intercepts musl libc Linux system calls in `host_syscall.c`:
* **Path Translation (`translate_guest_path`):** Converts Xbox drive letters and Windows-style paths into Switch SD card paths:
  * `d:\maps\ui.map` $\rightarrow$ `sdmc:/switch/halo/maps/ui.map`
  * `d:\debug.txt` $\rightarrow$ `sdmc:/switch/halo/debug.txt`
  * `c:\...` / `e:\...` / `z:\...` $\rightarrow$ `sdmc:/switch/halo/save/<drive>/...`
* **File Open Flag Mapping:** Translates Linux open flags (`O_CREAT` 0x40, `O_TRUNC` 0x200, `O_APPEND` 0x400) to Switch newlib flags (`O_CREAT` 0x200, `O_TRUNC` 0x400, `O_APPEND` 0x08).
* **Kernel Stat Translation:** Emulates ILP32 `struct kstat` layout for `SYS_newfstatat` and `SYS_fstat`, providing `st_size`, `st_mode`, and block sizing.
* **Standard Descriptor Isolation:** Descriptors 0, 1, and 2 are pre-occupied at startup to prevent guest file opens from overlapping with stdout/stderr.
* **Thread-Safe I/O:** `SYS_pread64` and `SYS_pwrite64` maintain atomic file offsets across concurrent threads via mutex locking.
* **Futex Emulation:** Emulates `SYS_futex` (`FUTEX_WAIT`, `FUTEX_WAKE`) with thread yielding to avoid CPU starvation and CRT lock priority inversion.

### 5. Diagnostics & Exception Handling

* **User Exception Handler (`__libnx_exception_handler`):** Registered with libnx with an independent 16 KB stack (`__nx_exception_stack`). Safely captures hardware faults (Data Aborts, Instruction Aborts) and dumps the Program Counter (`PC`), Link Register (`LR`), Stack Pointer (`SP`), Fault Address (`FAR`), Exception Syndrome (`ESR`), and stack memory backtraces.
* **Live On-Screen Console:** Uses `consoleInit(NULL)` to mirror host log output and guest `SYS_write` stderr/stdout directly onto the Switch display in real-time.
* **Heartbeat Monitor:** A background thread prints a ticking counter `[HB:N]` in the upper right corner of the screen, providing immediate visual verification between software terminations and CPU hard-locks.

---

## Repository Structure

```
├── .gitignore                      # Ignores build artifacts, object files, logs
├── Makefile                        # DevkitPro / libnx build configuration
├── README.md                       # Architecture & setup documentation
├── include/                        # Header files
│   ├── halo_switch_abi.h           # ARM64 ILP32 guest structures and boot ABI
│   ├── host.h                      # Host functions, memory macros, declarations
│   └── posix.h                     # POSIX definitions for Switch newlib
├── src/                            # Source files
│   ├── call_on_stack.s             # Assembly trampoline for 32-bit stack switching
│   ├── host_gl.c                   # Static OpenGL ES 3.2 dispatch table
│   ├── host_imports.c              # Guest-to-host symbol resolution table
│   ├── host_loader.c               # ELF loader & CodeMemory permission setup
│   ├── host_main.c                 # Entry point, logging, path translation, exception handler
│   ├── host_memory.c               # Virtual memory manager & Xbox contiguous window
│   ├── host_sdl.c                  # SDL2 display, EGL context, audio, and controller input
│   ├── host_syscall.c              # Musl libc system call emulation
│   ├── host_thread.c               # Guest thread creation & context dispatch
│   └── switch_posix.c              # Case-insensitive path search and directory helpers
├── romfs/                          # RomFS assets embedded into halo.nro
│   └── halo_guest.elf              # Precompiled AArch64 ILP32 guest executable
└── scripts/                        # Build & setup utility scripts
    ├── build.sh                    # Helper for native make build
    ├── docker-build.sh             # Helper for devkitpro Docker build
    └── install-devkitpro-pacman.sh # Environment setup script for Debian/Ubuntu
```

---

## Building the Port

### Option A: Native devkitPro Build

If you have devkitPro installed with the `switch-dev` packages:

```bash
# 1. Source devkitPro environment (if not already set)
source /etc/profile.d/devkit-env.sh
# or: export DEVKITPRO=/opt/devkitpro

# 2. Build using make
make -j$(nproc)

# Or run the build helper script:
./scripts/build.sh
```

To clean previous build artifacts:
```bash
make clean
```

The compiled homebrew application will be produced at:
`./halo.nro`

### Option B: Docker Build (Recommended)

You can build cleanly without installing toolchains on your host system using Docker and the official devkitA64 container:

```bash
./scripts/docker-build.sh
```

Or run Docker directly:
```bash
docker run --rm \
  -u $(id -u):$(id -g) \
  -v "$(pwd)":/work \
  -w /work \
  devkitpro/devkita64 \
  make -j$(nproc)
```

### Prerequisites & Toolchain Setup

If setting up a fresh Linux machine or container, you can run:
```bash
sudo ./scripts/install-devkitpro-pacman.sh
```

The required devkitPro packages are:
* `switch-dev`
* `switch-sdl2`
* `switch-mesa`
* `switch-libdrm_nouveau`

---

## Nintendo Switch Setup & SD Card Layout

Insert your Nintendo Switch MicroSD card into your computer, or access it via DBI / NX-Shell / FTP.

Create the following folder structure on your SD card root (`sdmc:/`):

```
sdmc:/
└── switch/
    └── halo/
        ├── halo.nro              # Place the compiled halo.nro here
        ├── halo.log              # Generated automatically at startup
        ├── save/                 # Created automatically for profile & save data
        │   ├── c/
        │   ├── e/
        │   ├── x/
        │   ├── y/
        │   └── z/
        └── maps/                 # Place your Halo PC map files here!
            ├── ui.map            # REQUIRED: Main menu and UI scenario
            ├── bitmaps.map       # REQUIRED: User interface and texture assets
            ├── sounds.map        # REQUIRED: Audio and voice assets
            ├── a10.map           # Campaign: The Pillar of Autumn
            ├── a30.map           # Campaign: Halo
            ├── a50.map           # Campaign: The Truth and Reconciliation
            ├── b30.map           # Campaign: The Silent Cartographer
            ├── b40.map           # Campaign: Assault on the Control Room
            ├── c10.map           # Campaign: 343 Guilty Spark
            ├── c20.map           # Campaign: The Library
            ├── c40.map           # Campaign: Two Betrayals
            ├── d20.map           # Campaign: Keyes
            ├── d40.map           # Campaign: The Maw
            └── bloodgulch.map    # Multiplayer maps (optional)
```

> **Note:** Alternatively, `halo.nro` can be placed at `sdmc:/switch/halo.nro`.

### Required Map Files

Assets must be acquired from a legal installation of **Halo: Combat Evolved (PC)** or **Halo Custom Edition**.
1. Navigate to your Halo PC installation folder (e.g. `C:\Program Files (x86)\Microsoft Games\Halo\MAPS\`).
2. Copy `ui.map`, `bitmaps.map`, `sounds.map`, and all `.map` files into `sdmc:/switch/halo/maps/`.
3. Ensure file names are lowercase or match standard naming.

> **CRITICAL:** `ui.map` is required to display the main menu. The engine will halt if `ui.map` is missing.

### CRITICAL: Launching with Full RAM (Title Redirection)

Halo: Combat Evolved reserves approximately **2 GB of virtual memory**.

⚠️ **DO NOT RUN IN APPLET MODE!**  
Opening the Homebrew Menu via the Album applet limits available memory to ~450 MB. Running in Applet Mode will result in an immediate out-of-memory crash.

**How to Launch with Full RAM (Title Redirection):**
1. On the Nintendo Switch main home menu, **press and hold the [R] button** on your controller.
2. While continuing to hold **[R]**, select and launch **any installed commercial game** (or free game demo).
3. Keep holding **[R]** until the Homebrew Menu appears.
4. Select and launch **Halo: Combat Evolved**.
5. The game will now run with full access to the console's RAM.

---

## Controls & Audio

* **Controllers:** Nintendo Switch Joy-Cons (Handheld mode or Joy-Con grip) and Nintendo Switch Pro Controllers are supported natively via libnx and SDL2 GameController mappings.
* **Audio:** Stereo sound playback is delivered via SDL Audio callbacks, dynamically bridged into the guest thread environment.
* **Display:** Automatically supports 720p 60fps in Handheld mode and 1080p 60fps in Docked mode via Switch Mesa and EGL.

---

## Troubleshooting & Logs

* **Inspecting Logs:**
  Check `sdmc:/switch/halo/halo.log` on your SD card. The host logs image loading, segment permissions, import resolution, memory mappings, and runtime messages.
* **Testing Guest Updates without Recompiling:**
  The loader checks for `sdmc:/switch/halo/halo_guest.elf` before falling back to the RomFS copy. You can place updated guest binaries directly on your SD card for rapid iteration.
* **Crash Diagnostics:**
  If a hardware exception occurs, the custom exception handler will output crash register information (`PC`, `LR`, `SP`, `FAR`, `ESR`) directly to the screen and log files.

---

## Credits & License

* Based on [halo-ce-universal](https://github.com/cybersecurity/halo-ce-universal)
* Original game created by **Bungie** and **Microsoft Game Studios**
* Nintendo Switch toolchains and libraries by [devkitPro](https://devkitpro.org/) and the [libnx](https://github.com/switchbrew/libnx) team
