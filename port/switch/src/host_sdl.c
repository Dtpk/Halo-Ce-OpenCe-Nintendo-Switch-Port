/*
HOST_SDL.C

SDL2 implementation for Nintendo Switch.
Maps guest handles to SDL2 windows, GL contexts, controllers, and audio streams.
*/

#include "host.h"

#include <SDL2/SDL.h>
#include <pthread.h>
#include <string.h>

#define HANDLE_COUNT 256

enum handle_type
{
	_handle_free,
	_handle_window,
	_handle_context,
	_handle_gamepad,
	_handle_audio,
};

struct handle
{
	int type;
	void *object;
};

static struct handle handles[HANDLE_COUNT];
static pthread_mutex_t handle_lock = PTHREAD_MUTEX_INITIALIZER;

static uint32_t handle_new(int type, void *object)
{
	host_logf(HOST_LOG_INFO, "[SDL HOOK] called %s", __func__); fflush(stdout);
	uint32_t index;

	if (!object)
		return 0;
	pthread_mutex_lock(&handle_lock);
	for (index = 1; index < HANDLE_COUNT; index++)
	{
		if (handles[index].type == type && handles[index].object == object)
		{
			pthread_mutex_unlock(&handle_lock);
			return index;
		}
	}
	for (index = 1; index < HANDLE_COUNT; index++)
	{
		if (handles[index].type == _handle_free)
		{
			handles[index].type = type;
			handles[index].object = object;
			pthread_mutex_unlock(&handle_lock);
			return index;
		}
	}
	pthread_mutex_unlock(&handle_lock);
	host_logf(HOST_LOG_ERROR, "out of SDL handles");
	return 0;
}

static void *handle_get(uint32_t handle, int type)
{
	host_logf(HOST_LOG_INFO, "[SDL HOOK] called %s", __func__); fflush(stdout);
	void *object = NULL;

	if (handle == 0 || handle >= HANDLE_COUNT)
		return NULL;
	pthread_mutex_lock(&handle_lock);
	if (handles[handle].type == type)
		object = handles[handle].object;
	pthread_mutex_unlock(&handle_lock);
	return object;
}

/* ---------- general */

int host_sdl_init(uint32_t flags)
{
	host_logf(HOST_LOG_INFO, "[SDL HOOK] called %s", __func__); fflush(stdout);
	/* Ensure video, joystick, and audio are initialized */
	return SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO);
}

int host_sdl_set_hint(const char *name, const char *value)
{
	host_logf(HOST_LOG_INFO, "[SDL HOOK] called %s", __func__); fflush(stdout);
	return SDL_SetHint(name, value);
}

void host_sdl_get_error(char *buffer, uint32_t size)
{
	host_logf(HOST_LOG_INFO, "[SDL HOOK] called %s", __func__); fflush(stdout);
	snprintf(buffer, size, "%s", SDL_GetError());
}

int64_t host_sdl_ticks(void)
{
	host_logf(HOST_LOG_INFO, "[SDL HOOK] called %s", __func__); fflush(stdout);
	return (int64_t)SDL_GetTicks();
}

int64_t host_sdl_thread_id(void)
{
	host_logf(HOST_LOG_INFO, "[SDL HOOK] called %s", __func__); fflush(stdout);
	return (int64_t)SDL_ThreadID();
}

/* ---------- video */

uint32_t host_sdl_create_window(const char *title, int width, int height, int64_t flags)
{
	host_logf(HOST_LOG_INFO, "[SDL HOOK] called %s", __func__); fflush(stdout);
	int win_w = width > 0 ? width : 1280;
	int win_h = height > 0 ? height : 720;

	host_logf(HOST_LOG_INFO, "[HOST SDL] SDL_CreateWindow: %s, %dx%d (flags: 0x%llx)", title, win_w, win_h, (unsigned long long)flags);

	/* On Switch, create OpenGL shown window without fullscreen flag to let SDL handle display scaling */
	SDL_Window *window = SDL_CreateWindow(title,
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		win_w, win_h, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

	if (window)
		host_logf(HOST_LOG_INFO, "SDL window created successfully: %dx%d", win_w, win_h);
	else
		host_logf(HOST_LOG_ERROR, "SDL_CreateWindow failed: %s", SDL_GetError());

	return handle_new(_handle_window, window);
}

void host_sdl_window_size_in_pixels(uint32_t window, int *width, int *height)
{
	host_logf(HOST_LOG_INFO, "[SDL HOOK] called %s", __func__); fflush(stdout);
	SDL_Window *object = handle_get(window, _handle_window);

	if (object)
	{
		SDL_GL_GetDrawableSize(object, width, height);
		if (*width <= 0 || *height <= 0)
			SDL_GetWindowSize(object, width, height);
	}
	else
	{
		*width = 1280;
		*height = 720;
	}
}

int host_sdl_set_relative_mouse(uint32_t window, int enabled)
{
	host_logf(HOST_LOG_INFO, "[SDL HOOK] called %s", __func__); fflush(stdout);
	return SDL_SetRelativeMouseMode(enabled ? SDL_TRUE : SDL_FALSE);
}

int host_sdl_gl_set_attribute(int attribute, int value)
{
	host_logf(HOST_LOG_INFO, "[SDL HOOK] called %s", __func__); fflush(stdout);
	return SDL_GL_SetAttribute((SDL_GLattr)attribute, value);
}

uint32_t host_sdl_gl_create_context(uint32_t window)
{
	host_logf(HOST_LOG_INFO, "[SDL HOOK] called %s", __func__); fflush(stdout);
	SDL_Window *object = handle_get(window, _handle_window);

	host_logf(HOST_LOG_INFO, "[HOST SDL] gl_create_context for window %u", window);

	uint32_t context = object ? handle_new(_handle_context, SDL_GL_CreateContext(object)) : 0;
	if (context == 0) {
		host_logf(HOST_LOG_ERROR, "[HOST SDL] gl_create_context failed: %s", SDL_GetError());
	} else {
		host_logf(HOST_LOG_INFO, "[HOST SDL] gl_create_context success for window %u, context %u", window, context);
	}
	return context;
}

int host_sdl_gl_make_current(uint32_t window, uint32_t context)
{
	host_logf(HOST_LOG_INFO, "[SDL HOOK] called %s", __func__); fflush(stdout);
	return SDL_GL_MakeCurrent(handle_get(window, _handle_window), handle_get(context, _handle_context));
}

int host_sdl_gl_set_swap_interval(int interval)
{
	host_logf(HOST_LOG_INFO, "[SDL HOOK] called %s", __func__); fflush(stdout);
	return SDL_GL_SetSwapInterval(interval);
}

int host_sdl_gl_swap_window(uint32_t window)
{
	host_logf(HOST_LOG_INFO, "[SDL HOOK] called %s", __func__); fflush(stdout);
	SDL_Window *object = handle_get(window, _handle_window);
	static uint32_t swap_count = 0;
	swap_count++;
	if (swap_count == 1 || (swap_count % 120) == 0)
	{
		host_logf(HOST_LOG_INFO, "[HOST GL] SwapWindow #%u", swap_count);
	}
	return object ? (SDL_GL_SwapWindow(object), 0) : 0;
}

/* ---------- events */

int host_sdl_poll_event(void *event)
{
	host_logf(HOST_LOG_INFO, "[SDL HOOK] called %s", __func__); fflush(stdout);
	if (!appletMainLoop())
	{
		host_logf(HOST_LOG_INFO, "[HOST] appletMainLoop requested exit, quitting cleanly...");
		host_exit(0);
	}

	SDL_Event host_event;

	if (!SDL_PollEvent(&host_event))
		return 0;
	memcpy(event, &host_event, sizeof(host_event));
	return 1;
}

/* ---------- gamepads / controllers */

int host_sdl_get_gamepads(uint32_t *ids, int capacity)
{
	host_logf(HOST_LOG_INFO, "[SDL HOOK] called %s", __func__); fflush(stdout);
	int num_joysticks = SDL_NumJoysticks();
	int count = 0;
	int i;

	for (i = 0; i < num_joysticks && count < capacity; i++)
	{
		if (SDL_IsGameController(i))
		{
			ids[count++] = (uint32_t)i;
		}
	}
	return count;
}

uint32_t host_sdl_open_gamepad(uint32_t id)
{
	host_logf(HOST_LOG_INFO, "[SDL HOOK] called %s", __func__); fflush(stdout);
	SDL_GameController *controller = SDL_GameControllerOpen((int)id);

	if (controller)
		host_logf(HOST_LOG_INFO, "controller %u opened: %s", id, SDL_GameControllerName(controller));
	return handle_new(_handle_gamepad, controller);
}

uint32_t host_sdl_gamepad_from_id(uint32_t id)
{
	host_logf(HOST_LOG_INFO, "[SDL HOOK] called %s", __func__); fflush(stdout);
	SDL_GameController *controller = SDL_GameControllerFromInstanceID((SDL_JoystickID)id);
	return handle_new(_handle_gamepad, controller);
}

int host_sdl_gamepad_axis(uint32_t gamepad, int axis)
{
	host_logf(HOST_LOG_INFO, "[SDL HOOK] called %s", __func__); fflush(stdout);
	SDL_GameController *object = handle_get(gamepad, _handle_gamepad);

	return object ? SDL_GameControllerGetAxis(object, (SDL_GameControllerAxis)axis) : 0;
}

int host_sdl_gamepad_button(uint32_t gamepad, int button)
{
	host_logf(HOST_LOG_INFO, "[SDL HOOK] called %s", __func__); fflush(stdout);
	SDL_GameController *object = handle_get(gamepad, _handle_gamepad);

	return object ? SDL_GameControllerGetButton(object, (SDL_GameControllerButton)button) : 0;
}

int host_sdl_gamepad_type(uint32_t gamepad)
{
	host_logf(HOST_LOG_INFO, "[SDL HOOK] called %s", __func__); fflush(stdout);
	(void)gamepad;
	return 0; /* Standard gamepad */
}

int host_sdl_rumble_gamepad(uint32_t gamepad, uint32_t low, uint32_t high, uint32_t milliseconds)
{
	host_logf(HOST_LOG_INFO, "[SDL HOOK] called %s", __func__); fflush(stdout);
	SDL_GameController *object = handle_get(gamepad, _handle_gamepad);

	return object ? SDL_GameControllerRumble(object, (Uint16)low, (Uint16)high, milliseconds) : 0;
}

/* ---------- audio ---------- */

struct audio_binding
{
	uint32_t handle;
	uint32_t callback;
	uint32_t userdata;
	SDL_AudioDeviceID device;
	SDL_AudioStream *stream;
};

static void SDLCALL audio_callback_sdl2(void *userdata, Uint8 *stream, int len)
{
	host_logf(HOST_LOG_INFO, "[SDL HOOK] called %s", __func__); fflush(stdout);
	struct audio_binding *binding = userdata;
	int available;

	if (!binding || !binding->stream)
	{
		memset(stream, 0, len);
		return;
	}

	available = SDL_AudioStreamAvailable(binding->stream);
	if (available < len && binding->callback)
	{
		host_call_guest(binding->callback, binding->userdata, binding->handle, (uint32_t)(len - available), (uint32_t)len);
	}

	int read_bytes = SDL_AudioStreamGet(binding->stream, stream, len);
	if (read_bytes < len)
		memset(stream + read_bytes, 0, len - read_bytes);
}

uint32_t host_sdl_open_audio_stream(uint32_t device, const void *spec, uint32_t callback, uint32_t userdata)
{
	host_logf(HOST_LOG_INFO, "[SDL HOOK] called %s", __func__); fflush(stdout);
	const SDL_AudioSpec *desired = (const SDL_AudioSpec *)spec;
	SDL_AudioSpec obtained;
	struct audio_binding *binding = SDL_calloc(1, sizeof(*binding));

	if (!binding)
		return 0;

	binding->callback = callback;
	binding->userdata = userdata;

	SDL_AudioSpec target_spec = desired ? *desired : (SDL_AudioSpec){ .freq = 44100, .format = AUDIO_S16SYS, .channels = 2, .samples = 1024 };
	target_spec.callback = audio_callback_sdl2;
	target_spec.userdata = binding;

	binding->device = SDL_OpenAudioDevice(NULL, 0, &target_spec, &obtained, 0);
	if (!binding->device)
	{
		host_logf(HOST_LOG_ERROR, "failed to open audio device: %s", SDL_GetError());
		SDL_free(binding);
		return 0;
	}

	binding->stream = SDL_NewAudioStream(target_spec.format, target_spec.channels, target_spec.freq,
		obtained.format, obtained.channels, obtained.freq);
	binding->handle = handle_new(_handle_audio, binding);

	SDL_PauseAudioDevice(binding->device, 0);
	return binding->handle;
}

int host_sdl_put_audio_stream_data(uint32_t stream, const void *data, int length)
{
	host_logf(HOST_LOG_INFO, "[SDL HOOK] called %s", __func__); fflush(stdout);
	struct audio_binding *binding = handle_get(stream, _handle_audio);

	if (!binding || !binding->stream)
		return 0;
	return SDL_AudioStreamPut(binding->stream, data, length);
}

int host_sdl_resume_audio_stream_device(uint32_t stream)
{
	host_logf(HOST_LOG_INFO, "[SDL HOOK] called %s", __func__); fflush(stdout);
	struct audio_binding *binding = handle_get(stream, _handle_audio);

	if (binding && binding->device)
	{
		SDL_PauseAudioDevice(binding->device, 0);
		return 1;
	}
	return 0;
}

/* ---------- clipboard & message box */

int host_sdl_set_clipboard_text(const char *text)
{
	host_logf(HOST_LOG_INFO, "[SDL HOOK] called %s", __func__); fflush(stdout);
	return SDL_SetClipboardText(text) == 0;
}

void host_sdl_get_clipboard_text(char *buffer, uint32_t size)
{
	host_logf(HOST_LOG_INFO, "[SDL HOOK] called %s", __func__); fflush(stdout);
	char *text = SDL_GetClipboardText();
	snprintf(buffer, size, "%s", text ? text : "");
	if (text)
		SDL_free(text);
}

int host_sdl_show_toast(const char *message, int duration, int gravity, int x, int y)
{
	host_logf(HOST_LOG_INFO, "[SDL HOOK] called %s", __func__); fflush(stdout);
	(void)duration;
	(void)gravity;
	(void)x;
	(void)y;
	host_logf(HOST_LOG_INFO, "[TOAST] %s", message);
	return 1;
}

int host_sdl_show_simple_message_box(uint32_t flags, const char *title, const char *message)
{
	host_logf(HOST_LOG_INFO, "[SDL HOOK] called %s", __func__); fflush(stdout);
	host_logf(HOST_LOG_WARN, "[MESSAGE BOX %s] %s", title, message);
	return SDL_ShowSimpleMessageBox((Uint32)flags, title, message, NULL) == 0;
}
