/*
HOST_GL.C

OpenGL ES 2.0 / EGL driver interface for Nintendo Switch Mesa.
*/

#include "host.h"

#include <EGL/egl.h>
#include <GLES3/gl32.h>
#include <string.h>

struct gl_dispatch {
	const char *name;
	void *func;
};

/* The guest calls functions prefixed with 'hostgl_' */
static const struct gl_dispatch gl_table[] = {
	{ "hostgl_glGetIntegerv", (void *)glGetIntegerv },
	{ "hostgl_glGenerateMipmap", (void *)glGenerateMipmap },
	{ "hostgl_glGetError", (void *)glGetError },
	{ "hostgl_glEnable", (void *)glEnable },
	{ "hostgl_glDisable", (void *)glDisable },
	{ "hostgl_glViewport", (void *)glViewport },
	{ "hostgl_glDepthRangef", (void *)glDepthRangef },
	{ "hostgl_glScissor", (void *)glScissor },
	{ "hostgl_glClearColor", (void *)glClearColor },
	{ "hostgl_glClearDepthf", (void *)glClearDepthf },
	{ "hostgl_glClearStencil", (void *)glClearStencil },
	{ "hostgl_glClear", (void *)glClear },
	{ "hostgl_glColorMask", (void *)glColorMask },
	{ "hostgl_glDepthMask", (void *)glDepthMask },
	{ "hostgl_glDepthFunc", (void *)glDepthFunc },
	{ "hostgl_glStencilFunc", (void *)glStencilFunc },
	{ "hostgl_glStencilOp", (void *)glStencilOp },
	{ "hostgl_glStencilMask", (void *)glStencilMask },
	{ "hostgl_glBlendFunc", (void *)glBlendFunc },
	{ "hostgl_glBlendEquation", (void *)glBlendEquation },
	{ "hostgl_glBlendColor", (void *)glBlendColor },
	{ "hostgl_glCullFace", (void *)glCullFace },
	{ "hostgl_glFrontFace", (void *)glFrontFace },
	{ "hostgl_glPolygonOffset", (void *)glPolygonOffset },
	{ "hostgl_glLineWidth", (void *)glLineWidth },
	{ "hostgl_glPixelStorei", (void *)glPixelStorei },
	{ "hostgl_glReadPixels", (void *)glReadPixels },
	{ "hostgl_glFinish", (void *)glFinish },
	{ "hostgl_glFlush", (void *)glFlush },
	{ "hostgl_glGenTextures", (void *)glGenTextures },
	{ "hostgl_glDeleteTextures", (void *)glDeleteTextures },
	{ "hostgl_glBindTexture", (void *)glBindTexture },
	{ "hostgl_glActiveTexture", (void *)glActiveTexture },
	{ "hostgl_glTexImage2D", (void *)glTexImage2D },
	{ "hostgl_glTexSubImage2D", (void *)glTexSubImage2D },
	{ "hostgl_glCompressedTexImage2D", (void *)glCompressedTexImage2D },
	{ "hostgl_glTexParameteri", (void *)glTexParameteri },
	{ "hostgl_glTexParameteriv", (void *)glTexParameteriv },
	{ "hostgl_glTexParameterf", (void *)glTexParameterf },
	{ "hostgl_glTexParameterfv", (void *)glTexParameterfv },
	{ "hostgl_glGenFramebuffers", (void *)glGenFramebuffers },
	{ "hostgl_glDeleteFramebuffers", (void *)glDeleteFramebuffers },
	{ "hostgl_glBindFramebuffer", (void *)glBindFramebuffer },
	{ "hostgl_glFramebufferTexture2D", (void *)glFramebufferTexture2D },
	{ "hostgl_glCheckFramebufferStatus", (void *)glCheckFramebufferStatus },
	{ "hostgl_glGenBuffers", (void *)glGenBuffers },
	{ "hostgl_glDeleteBuffers", (void *)glDeleteBuffers },
	{ "hostgl_glBindBuffer", (void *)glBindBuffer },
	{ "hostgl_glBufferData", (void *)glBufferData },
	{ "hostgl_glBufferSubData", (void *)glBufferSubData },
	{ "hostgl_glGenVertexArrays", (void *)glGenVertexArrays },
	{ "hostgl_glBindVertexArray", (void *)glBindVertexArray },
	{ "hostgl_glEnableVertexAttribArray", (void *)glEnableVertexAttribArray },
	{ "hostgl_glDisableVertexAttribArray", (void *)glDisableVertexAttribArray },
	{ "hostgl_glVertexAttribPointer", (void *)glVertexAttribPointer },
	{ "hostgl_glVertexAttrib4fv", (void *)glVertexAttrib4fv },
	{ "hostgl_glDrawArrays", (void *)glDrawArrays },
	{ "hostgl_glDrawElements", (void *)glDrawElements },
	{ "hostgl_glCreateShader", (void *)glCreateShader },
	{ "hostgl_glShaderSource", (void *)glShaderSource },
	{ "hostgl_glCompileShader", (void *)glCompileShader },
	{ "hostgl_glGetShaderiv", (void *)glGetShaderiv },
	{ "hostgl_glGetShaderInfoLog", (void *)glGetShaderInfoLog },
	{ "hostgl_glDeleteShader", (void *)glDeleteShader },
	{ "hostgl_glCreateProgram", (void *)glCreateProgram },
	{ "hostgl_glAttachShader", (void *)glAttachShader },
	{ "hostgl_glBindAttribLocation", (void *)glBindAttribLocation },
	{ "hostgl_glLinkProgram", (void *)glLinkProgram },
	{ "hostgl_glGetProgramiv", (void *)glGetProgramiv },
	{ "hostgl_glGetProgramInfoLog", (void *)glGetProgramInfoLog },
	{ "hostgl_glUseProgram", (void *)glUseProgram },
	{ "hostgl_glGetUniformLocation", (void *)glGetUniformLocation },
	{ "hostgl_glUniform1i", (void *)glUniform1i },
	{ "hostgl_glUniform1iv", (void *)glUniform1iv },
	{ "hostgl_glUniform1f", (void *)glUniform1f },
	{ "hostgl_glUniform4fv", (void *)glUniform4fv },
	{ "hostgl_glUniform2f", (void *)glUniform2f },
};

void *host_gl_resolve(const char *name)
{
	size_t i;
	for (i = 0; i < sizeof(gl_table) / sizeof(gl_table[0]); i++)
	{
		if (strcmp(gl_table[i].name, name) == 0)
			return gl_table[i].func;
	}

	/* Fallback for extensions - strip 'hostgl_' prefix if present */
	const char *real_name = (strncmp(name, "hostgl_", 7) == 0) ? name + 7 : name;
	void *function = (void *)eglGetProcAddress(real_name);
	return function;
}

void host_gl_get_string(uint32_t name, int index, char *buffer, uint32_t size)
{
	const GLubyte *text = index >= 0 ? glGetStringi(name, (GLuint)index) : glGetString(name);

	if (!size)
		return;
	buffer[0] = 0;
	if (text)
	{
		strncpy(buffer, (const char *)text, size - 1);
		buffer[size - 1] = 0;
	}
}

int host_gl_has_extension(const char *name)
{
	GLint count = 0, index;

	glGetIntegerv(GL_NUM_EXTENSIONS, &count);
	for (index = 0; index < count; index++)
	{
		const char *extension = (const char *)glGetStringi(GL_EXTENSIONS, (GLuint)index);

		if (extension && !strcmp(extension, name))
			return 1;
	}
	return 0;
}

uint32_t host_gl_read_buffer_word(uint32_t buffer, uint32_t offset)
{
	uint32_t value = 0;
	GLint previous = 0;
	const void *mapping;

	glGetIntegerv(GL_ATOMIC_COUNTER_BUFFER_BINDING, &previous);
	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, buffer);
	mapping = glMapBufferRange(GL_ATOMIC_COUNTER_BUFFER, offset, sizeof(value), GL_MAP_READ_BIT);
	if (mapping)
	{
		memcpy(&value, mapping, sizeof(value));
		glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);
	}
	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, (GLuint)previous);
	return value;
}

#define FRAME_FENCE_SLOTS 8
static GLsync frame_fences[FRAME_FENCE_SLOTS];

void host_gl_fence_frame(uint32_t slot)
{
	if (slot >= FRAME_FENCE_SLOTS)
		return;
	if (frame_fences[slot])
		glDeleteSync(frame_fences[slot]);
	frame_fences[slot] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

void host_gl_wait_frame(uint32_t slot)
{
	if (slot >= FRAME_FENCE_SLOTS || !frame_fences[slot])
		return;
	glClientWaitSync(frame_fences[slot], GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000ULL);
}

void host_gl_buffer_write(uint32_t target, uint32_t offset, uint32_t size, const void *data)
{
	void *mapping = glMapBufferRange(target, offset, size,
		GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);

	if (!mapping)
	{
		glBufferSubData(target, offset, size, data);
		return;
	}
	memcpy(mapping, data, size);
	glUnmapBuffer(target);
}
