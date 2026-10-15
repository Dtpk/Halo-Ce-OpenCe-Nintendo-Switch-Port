/*
HOST_MAIN.C

Main entry point for Halo: Combat Evolved on Nintendo Switch.
*/

#include "host.h"

#include <SDL2/SDL.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <switch.h>
#include <signal.h>

static char data_root[512] = "sdmc:/switch/halo";
static char save_root[512] = "sdmc:/switch/halo/save";
FILE *log_file = NULL;
pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

/* ---------- logging & errors ---------- */

alignas(16) u8 __nx_exception_stack[0x4000];
u64 __nx_exception_stack_size = sizeof(__nx_exception_stack);
u32 __nx_exception_ignoredebug = 1; /* Set to 1 to allow Atmosphere blue screen */
extern ThreadExceptionDump __nx_exceptiondump;

void __libnx_exception_handler(ThreadExceptionDump *unused_ctx)
{
    ThreadExceptionDump *ctx = &__nx_exceptiondump;
    consoleInit(NULL);
    printf("\x1b[2J\x1b[1;1H");
    printf("!!! GUEST CRASH DETECTED !!!\n\n");
    printf("PC: 0x%016llx  LR: 0x%016llx\n", (unsigned long long)ctx->pc.x, (unsigned long long)ctx->lr.x);
    printf("FAR: 0x%016llx  ESR: 0x%08x\n", (unsigned long long)ctx->far.x, ctx->esr);
    consoleUpdate(NULL);
    while (1) {
        svcSleepThread(1000000000ULL);
    }
}

static void crash_signal_handler(int sig)
{
    const char *signame = "UNKNOWN";
    if (sig == SIGSEGV) signame = "SIGSEGV";
    else if (sig == SIGABRT) signame = "SIGABRT";
    else if (sig == SIGILL) signame = "SIGILL";

    consoleInit(NULL);
    printf("\n\n*** SIGNAL %d (%s) ***\n\n", sig, signame);
    consoleUpdate(NULL);
    svcSleepThread(5000000000ULL);
    _exit(1);
}

static void init_signal_handlers(void)
{
    signal(SIGSEGV, crash_signal_handler);
    signal(SIGABRT, crash_signal_handler);
    signal(SIGILL, crash_signal_handler);
}

static void *heartbeat_thread(void *arg)
{
    (void)arg;
    int count = 0;
    while (1)
    {
        printf("\x1b[s\x1b[1;70H[HB:%d]\x1b[u", count++);
        consoleUpdate(NULL);
        svcSleepThread(1000000000ULL);
    }
    return NULL;
}

static void init_logging(void)
{
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&log_lock, &attr);
	pthread_mutexattr_destroy(&attr);

	mkdir("sdmc:/switch/halo", 0777);
	mkdir("sdmc:/switch/halo/save", 0777);

	log_file = fopen("sdmc:/switch/halo/halo.log", "w");
	if (log_file)
	{
		setvbuf(log_file, NULL, _IONBF, 0);
	}
    init_signal_handlers();

    pthread_t hb_thread;
    pthread_create(&hb_thread, NULL, heartbeat_thread, NULL);
}

void translate_guest_path(const char *guest_path, char *host_path, size_t size)
{
	if (!guest_path || !*guest_path)
	{
		snprintf(host_path, size, "sdmc:/switch/halo");
		return;
	}

	if (((guest_path[0] >= 'a' && guest_path[0] <= 'z') ||
	     (guest_path[0] >= 'A' && guest_path[0] <= 'Z')) && guest_path[1] == ':')
	{
		char drive = guest_path[0] | 0x20;
		const char *sub = guest_path + 2;
		while (*sub == '\\' || *sub == '/') sub++;

		if (drive == 'd')
			snprintf(host_path, size, "sdmc:/switch/halo/%s", sub);
		else
			snprintf(host_path, size, "sdmc:/switch/halo/save/%c/%s", drive, sub);
	}
	else if (strncmp(guest_path, "sdmc:/", 6) == 0 || strncmp(guest_path, "romfs:/", 7) == 0)
		snprintf(host_path, size, "%s", guest_path);
	else
		snprintf(host_path, size, "sdmc:/switch/halo/%s", guest_path);

	for (char *p = host_path; *p; p++)
		if (*p == '\\') *p = '/';
}

void host_logf(int priority, const char *format, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    pthread_mutex_lock(&log_lock);
    if (log_file)
    {
            fprintf(log_file, "[HALO] %s\n", buffer);
            fflush(log_file);
    }
    printf("%s\n", buffer);
    consoleUpdate(NULL);
    pthread_mutex_unlock(&log_lock);
}

void host_log(int priority, const char *text) { host_logf(priority, "%s", text); }

void host_fatal(const char *format, ...)
{
	char message[1024];
	va_list args;
	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);

	host_logf(HOST_LOG_ERROR, "FATAL ERROR: %s", message);
	_exit(1);
}

void host_abort(const char *reason) { host_fatal("guest aborted: %s", reason); }
void host_exit(int code) { _exit(code); }
int host_errno(void) { return errno; }
void host_android_path(int which, char *buffer, uint32_t size) { snprintf(buffer, size, "%s", which ? save_root : data_root); }

static int directory_has_maps(const char *root)
{
	char path[600];
	struct stat info;
	snprintf(path, sizeof(path), "%s/maps/ui.map", root);
	return (stat(path, &info) == 0);
}

/* ---------- environment ---------- */
#define ENVIRONMENT_MAX 64
struct environment { char *entries[ENVIRONMENT_MAX]; int count; };
static void env_set(struct environment *env, const char *name, const char *value)
{
	size_t len = strlen(name);
	char *entry = malloc(len + strlen(value) + 2);
	sprintf(entry, "%s=%s", name, value);
	for (int i = 0; i < env->count; i++)
		if (!strncmp(env->entries[i], name, len) && env->entries[i][len] == '=')
			{ free(env->entries[i]); env->entries[i] = entry; return; }
	if (env->count < ENVIRONMENT_MAX) env->entries[env->count++] = entry;
}

static uint32_t make_boot(const struct environment *env)
{
	uint32_t mem_guest_addr = 0x891c0000;
	char *mem = (char *)GUEST_PTR_TO_HOST(mem_guest_addr);
	memset(mem, 0, 0x10000);
	struct halo_guest_boot *boot = (struct halo_guest_boot *)mem;
	uint32_t *argv = (uint32_t *)(mem + sizeof(*boot));
	uint32_t *environ_list = argv + 2;
	char *strings = (char *)(environ_list + ENVIRONMENT_MAX + 1);
	strcpy(strings, "halo");
	argv[0] = (uint32_t)(mem_guest_addr + (uintptr_t)((uint8_t*)strings - (uint8_t*)mem));
	strings += strlen(strings) + 1;
	int i;
	for (i = 0; i < env->count; i++) {
		size_t len = strlen(env->entries[i]) + 1;
		memcpy(strings, env->entries[i], len);
		environ_list[i] = (uint32_t)(mem_guest_addr + (uintptr_t)((uint8_t*)strings - (uint8_t*)mem));
		strings += len;
	}
	boot->argc = 1;
	boot->argv = (uint32_t)(mem_guest_addr + (uintptr_t)((uint8_t*)argv - (uint8_t*)mem));
	boot->environment = (uint32_t)(mem_guest_addr + (uintptr_t)((uint8_t*)environ_list - (uint8_t*)mem));
	boot->page_size = 4096;
	return mem_guest_addr;
}

/* ---------- game thread ---------- */
static void *game_main(void *unused)
{
    host_logf(HOST_LOG_INFO, "Halo CE starting on Nintendo Switch");
    FILE *f = fopen("romfs:/halo_guest.elf", "rb");
    if (!f) f = fopen("sdmc:/switch/halo/halo_guest.elf", "rb");
    if (!f) host_fatal("Cannot open halo_guest.elf");
    fseek(f, 0, SEEK_END);
    size_t image_size = ftell(f);
    rewind(f);
    void *image_data = malloc(image_size);
    fread(image_data, 1, image_size, f);
    fclose(f);
    if (host_load_image(image_data, image_size) != 0) host_fatal("host_load_image failed!");
    free(image_data);
    struct environment environment = { { 0 }, 0 };
    env_set(&environment, "HALO_DISPLAY_WIDTH", "852");
    uint32_t boot = make_boot(&environment);
    host_run_guest_main(boot);
    return NULL;
}

int main(int argc, char *argv[])
{
	init_logging();
	chdir("sdmc:/switch/halo");
	host_memory_preinit();
	romfsInit();
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) < 0) host_fatal("SDL_Init failed");
	consoleInit(NULL);
	printf("\n[HALO] Live console initialized.\n");
	
	/* Vibration test (all common controller IDs) */
	HidVibrationDeviceHandle vib_h[10];
	HidNpadIdType ids[] = {HidNpadIdType_No1, HidNpadIdType_No2, HidNpadIdType_Handheld};
	for (int i = 0; i < 3; i++) {
		hidInitializeVibrationDevices(vib_h, 2, ids[i], HidNpadStyleTag_NpadFullKey | HidNpadStyleTag_NpadHandheld);
		HidVibrationValue vib_v = {0.4f, 160.0f, 0.4f, 320.0f};
		hidSendVibrationValue(vib_h[0], &vib_v);
		hidSendVibrationValue(vib_h[1], &vib_v);
		svcSleepThread(100000000ULL);
		vib_v = (HidVibrationValue){0.0f, 160.0f, 0.0f, 320.0f};
		hidSendVibrationValue(vib_h[0], &vib_v);
		hidSendVibrationValue(vib_h[1], &vib_v);
	}

	game_main(NULL);
	return 0;
}
