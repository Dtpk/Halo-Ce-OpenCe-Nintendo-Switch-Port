/* Generated import table for Nintendo Switch host */
#include <string.h>
#include "host.h"
#if __has_include("posix.h")
#include "posix.h"
#else
#include "../../linux/src/posix.h"
#endif

extern void host_sdl_init();
extern void host_sdl_set_hint();
extern void host_sdl_get_error();
extern void host_sdl_ticks();
extern void host_sdl_thread_id();
extern void host_sdl_create_window();
extern void host_sdl_window_size_in_pixels();
extern void host_sdl_set_relative_mouse();
extern void host_sdl_gl_set_attribute();
extern void host_sdl_gl_create_context();
extern void host_sdl_gl_make_current();
extern void host_sdl_gl_set_swap_interval();
extern void host_sdl_gl_swap_window();
extern void host_sdl_poll_event();
extern void host_sdl_set_clipboard_text();
extern void host_sdl_get_clipboard_text();
extern void host_sdl_show_toast();
extern void host_sdl_show_simple_message_box();
extern void host_sdl_get_gamepads();
extern void host_sdl_open_gamepad();
extern void host_sdl_gamepad_from_id();
extern void host_sdl_gamepad_axis();
extern void host_sdl_gamepad_button();
extern void host_sdl_gamepad_type();
extern void host_sdl_rumble_gamepad();
extern void host_sdl_open_audio_stream();
extern void host_sdl_put_audio_stream_data();
extern void host_sdl_resume_audio_stream_device();
extern void host_gl_get_string();
extern void host_gl_has_extension();
extern void host_gl_read_buffer_word();
extern void host_gl_buffer_write();
extern void host_gl_fence_frame();
extern void host_gl_wait_frame();

static const struct { const char *name; void *function; } imports[] = {
	{ "host_syscall", (void *)host_syscall },
	{ "host_log", (void *)host_log },
	{ "host_abort", (void *)host_abort },
	{ "host_exit", (void *)host_exit },
	{ "host_get_tp", (void *)host_get_tp },
	{ "host_set_tp", (void *)host_set_tp },
	{ "host_thread_create", (void *)host_thread_create },
	{ "host_memory_watch_initialize", (void *)host_memory_watch_initialize },
	{ "host_memory_watch_protect", (void *)host_memory_watch_protect },
	{ "host_memory_watch_generation", (void *)host_memory_watch_generation },
	{ "host_memory_watch_serial", (void *)host_memory_watch_serial },
	{ "host_memory_watch_prepare_write", (void *)host_memory_watch_prepare_write },
	{ "host_memory_watch_forget", (void *)host_memory_watch_forget },
	{ "host_sdl_init", (void *)host_sdl_init },
	{ "host_sdl_set_hint", (void *)host_sdl_set_hint },
	{ "host_sdl_get_error", (void *)host_sdl_get_error },
	{ "host_sdl_ticks", (void *)host_sdl_ticks },
	{ "host_sdl_thread_id", (void *)host_sdl_thread_id },
	{ "host_sdl_create_window", (void *)host_sdl_create_window },
	{ "host_sdl_window_size_in_pixels", (void *)host_sdl_window_size_in_pixels },
	{ "host_sdl_set_relative_mouse", (void *)host_sdl_set_relative_mouse },
	{ "host_sdl_gl_set_attribute", (void *)host_sdl_gl_set_attribute },
	{ "host_sdl_gl_create_context", (void *)host_sdl_gl_create_context },
	{ "host_sdl_gl_make_current", (void *)host_sdl_gl_make_current },
	{ "host_sdl_gl_set_swap_interval", (void *)host_sdl_gl_set_swap_interval },
	{ "host_sdl_gl_swap_window", (void *)host_sdl_gl_swap_window },
	{ "host_sdl_poll_event", (void *)host_sdl_poll_event },
	{ "host_sdl_set_clipboard_text", (void *)host_sdl_set_clipboard_text },
	{ "host_sdl_get_clipboard_text", (void *)host_sdl_get_clipboard_text },
	{ "host_sdl_show_toast", (void *)host_sdl_show_toast },
	{ "host_sdl_show_simple_message_box", (void *)host_sdl_show_simple_message_box },
	{ "host_sdl_get_gamepads", (void *)host_sdl_get_gamepads },
	{ "host_sdl_open_gamepad", (void *)host_sdl_open_gamepad },
	{ "host_sdl_gamepad_from_id", (void *)host_sdl_gamepad_from_id },
	{ "host_sdl_gamepad_axis", (void *)host_sdl_gamepad_axis },
	{ "host_sdl_gamepad_button", (void *)host_sdl_gamepad_button },
	{ "host_sdl_gamepad_type", (void *)host_sdl_gamepad_type },
	{ "host_sdl_rumble_gamepad", (void *)host_sdl_rumble_gamepad },
	{ "host_sdl_open_audio_stream", (void *)host_sdl_open_audio_stream },
	{ "host_sdl_put_audio_stream_data", (void *)host_sdl_put_audio_stream_data },
	{ "host_sdl_resume_audio_stream_device", (void *)host_sdl_resume_audio_stream_device },
	{ "host_gl_get_string", (void *)host_gl_get_string },
	{ "host_gl_has_extension", (void *)host_gl_has_extension },
	{ "host_gl_read_buffer_word", (void *)host_gl_read_buffer_word },
	{ "host_gl_buffer_write", (void *)host_gl_buffer_write },
	{ "host_gl_fence_frame", (void *)host_gl_fence_frame },
	{ "host_gl_wait_frame", (void *)host_gl_wait_frame },
	{ "host_android_path", (void *)host_android_path },
	{ "host_errno", (void *)host_errno },
	{ "hostposix_stat", (void *)posix_stat },
	{ "hostposix_fstat", (void *)posix_fstat },
	{ "hostposix_set_file_times", (void *)posix_set_file_times },
	{ "hostposix_seek", (void *)posix_seek },
	{ "hostposix_truncate", (void *)posix_truncate },
	{ "hostposix_disk_space", (void *)posix_disk_space },
	{ "hostposix_set_read_only", (void *)posix_set_read_only },
	{ "hostposix_make_directory", (void *)posix_make_directory },
	{ "hostposix_directory_open", (void *)posix_directory_open },
	{ "hostposix_directory_next", (void *)posix_directory_next },
	{ "hostposix_directory_close", (void *)posix_directory_close },
	{ "hostposix_find_entry_case_insensitive", (void *)posix_find_entry_case_insensitive },
	{ "hostposix_socket_last_error", (void *)posix_socket_last_error },
	{ "hostposix_socket", (void *)posix_socket },
	{ "hostposix_socket_close", (void *)posix_socket_close },
	{ "hostposix_socket_bind", (void *)posix_socket_bind },
	{ "hostposix_socket_connect", (void *)posix_socket_connect },
	{ "hostposix_socket_listen", (void *)posix_socket_listen },
	{ "hostposix_socket_accept", (void *)posix_socket_accept },
	{ "hostposix_socket_send", (void *)posix_socket_send },
	{ "hostposix_socket_sendto", (void *)posix_socket_sendto },
	{ "hostposix_socket_recv", (void *)posix_socket_recv },
	{ "hostposix_socket_recvfrom", (void *)posix_socket_recvfrom },
	{ "hostposix_socket_shutdown", (void *)posix_socket_shutdown },
	{ "hostposix_socket_set_nonblocking", (void *)posix_socket_set_nonblocking },
	{ "hostposix_socket_bytes_available", (void *)posix_socket_bytes_available },
	{ "hostposix_socket_set_nodelay", (void *)posix_socket_set_nodelay },
	{ "hostposix_socket_setsockopt", (void *)posix_socket_setsockopt },
	{ "hostposix_socket_getsockopt", (void *)posix_socket_getsockopt },
	{ "hostposix_socket_getsockname", (void *)posix_socket_getsockname },
	{ "hostposix_socket_getpeername", (void *)posix_socket_getpeername },
	{ "hostposix_socket_select", (void *)posix_socket_select },
	{ "hostposix_local_ipv4_address", (void *)posix_local_ipv4_address },
	{ "hostposix_random_bytes", (void *)posix_random_bytes },
	{ "hostposix_resolve_ipv4", (void *)posix_resolve_ipv4 },
	{ "hostposix_upnp_forward_udp", (void *)posix_upnp_forward_udp },
	{ "hostposix_upnp_stop_forwarding_udp", (void *)posix_upnp_stop_forwarding_udp },
	{ "hostposix_command_line_argument", (void *)posix_command_line_argument },
	{ "hostposix_process_id", (void *)posix_process_id },
	{ "hostposix_register_url_scheme", (void *)posix_register_url_scheme },
	{ "hostposix_user_secret", (void *)posix_user_secret },
	{ "hostposix_discord_connect", (void *)posix_discord_connect },
	{ "hostposix_discord_write", (void *)posix_discord_write },
	{ "hostposix_discord_read", (void *)posix_discord_read },
	{ "hostposix_discord_close", (void *)posix_discord_close },
};

void *host_resolve_import(const char *name)
{
	unsigned long index;
	for (index = 0; index < sizeof(imports) / sizeof(imports[0]); index++)
	{
		if (!strcmp(imports[index].name, name))
			return imports[index].function;
	}

	if (strncmp(name, "hostgl_", 7) == 0)
		return host_gl_resolve(name);

	return NULL;
}
