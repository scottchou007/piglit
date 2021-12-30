/*
 * Copyright (c) 2011 VMware, Inc.
 * Copyright (c) 2015 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT.  IN NO EVENT SHALL VMWARE AND/OR THEIR SUPPLIERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * Tests GL_ARB_texture_storage
 * Note: only the glTexStorage2D() function is tested with actual rendering.
 * Brian Paul
 *
 * Additional author(s):
 *    Nicolai Hähnle <nicolai.haehnle@amd.com>
 */

#include "piglit-util-gl.h"

PIGLIT_GL_TEST_CONFIG_BEGIN

	config.supports_gl_compat_version = 12;

	config.window_visual = PIGLIT_GL_VISUAL_RGBA | PIGLIT_GL_VISUAL_DOUBLE;
	config.khr_no_error_support = PIGLIT_HAS_ERRORS;

PIGLIT_GL_TEST_CONFIG_END

static const char *TestName = "texture-storage";

static GLubyte Colors[][4] = {
	{255,	0,	0, 255},
	{  0, 255,	0, 255},
	{  0,	0, 255, 255},
	{  0, 255, 255, 255},
	{255,	0, 255, 255},
	{255, 255,	0, 255},
	{255, 255, 255, 255},
	{128,	0,	0, 255},
	{  0, 128,	0, 255},
	{  0,	0, 128, 255}
};


static void
do_texture_storage(GLuint tex, GLenum target, bool ext_dsa,
				   GLint levels, GLenum format,
				   GLint width, GLint height, GLint depth)
{
	if (ext_dsa) {
		GLuint tex2;
		glGenTextures(1, &tex2);
		/* Bind a different texture to make sure dsa calls apply to the specified
		 * texture name, not the bound one.
		 */
		glBindTexture(target, tex2);

		if (target == GL_TEXTURE_1D) {
			glTextureStorage1DEXT(tex, target, levels, format, width);
		}
		else if (target == GL_TEXTURE_2D || target == GL_TEXTURE_CUBE_MAP) {
			glTextureStorage2DEXT(tex, target, levels, format, width, height);
		}
		else if (target == GL_TEXTURE_3D || target == GL_TEXTURE_CUBE_MAP_ARRAY) {
			glTextureStorage3DEXT(tex, target, levels, format, width, height, depth);
		}
		glDeleteTextures(1, &tex2);

		glBindTexture(target, tex);
	} else {
		if (target == GL_TEXTURE_1D) {
			glTexStorage1D(target, levels, format, width);
		}
		else if (target == GL_TEXTURE_2D || target == GL_TEXTURE_CUBE_MAP) {
			glTexStorage2D(target, levels, format, width, height);
		}
		else if (target == GL_TEXTURE_3D || target == GL_TEXTURE_CUBE_MAP_ARRAY) {
			glTexStorage3D(target, levels, format, width, height, depth);
		}
	}
}

/**
 * Do error-check tests for a non-mipmapped texture.
 */
static enum piglit_result
test_one_level_errors(GLenum target, bool ext_dsa)
{
	const GLint width = 64, height = 4, depth = 8;
	GLuint tex;
	GLint v;

	assert(target == GL_TEXTURE_1D ||
	       target == GL_TEXTURE_2D ||
	       target == GL_TEXTURE_3D);

	glGenTextures(1, &tex);
	glBindTexture(target, tex);
	do_texture_storage(tex, target, ext_dsa, 1, GL_RGBA8,width, height, depth);
	piglit_check_gl_error(GL_NO_ERROR);

	glGetTexLevelParameteriv(target, 0, GL_TEXTURE_WIDTH, &v);
	if (v != width) {
		printf("%s: bad width: %d, should be %d\n", TestName, v, width);
		return PIGLIT_FAIL;
	}

	if (target != GL_TEXTURE_1D) {
		glGetTexLevelParameteriv(target, 0, GL_TEXTURE_HEIGHT, &v);
		if (v != height) {
			printf("%s: bad height: %d, should be %d\n", TestName,
			       v, height);
			return PIGLIT_FAIL;
		}
	}

	if (target == GL_TEXTURE_3D) {
		glGetTexLevelParameteriv(target, 0, GL_TEXTURE_DEPTH, &v);
		if (v != depth) {
			printf("%s: bad depth: %d, should be %d\n", TestName,
			       v, depth);
			return PIGLIT_FAIL;
		}
	}

	/* The ARB_texture_storage spec says:
	 *
	 *     "Using any of the following commands with the same texture will
	 *     result in the error INVALID_OPERATION being generated, even if
	 *     it does not affect the dimensions or format:
	 *
	 *         - TexImage*
	 *         - CompressedTexImage*
	 *         - CopyTexImage*
	 *         - TexStorage*"
	 */
	if (target == GL_TEXTURE_2D) {
		glTexImage2D(target, 0, GL_RGBA, width, height, 0,
			     GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		if (glGetError() != GL_INVALID_OPERATION) {
			printf("%s: glTexImage2D failed to generate error\n",
			       TestName);
			return PIGLIT_FAIL;
		}

		glTexStorage2D(target, 1, GL_RGBA8, width, height);
		if (glGetError() != GL_INVALID_OPERATION) {
			printf("%s: glTexStorage2D() failed to generate "
			       "error\n", TestName);
			return PIGLIT_FAIL;
		}

		glCopyTexImage2D(target, 0, GL_RGBA, 0, 0, width, height, 0);
		if (glGetError() != GL_INVALID_OPERATION) {
			printf("%s: glCopyTexImage2D() failed to generate "
			       "error\n", TestName);
			return PIGLIT_FAIL;
		}
	}

	glDeleteTextures(1, &tex);

	return PIGLIT_PASS;
}


/**
 * Do error-check tests for a mipmapped texture.
 */
static enum piglit_result
test_mipmap_errors(GLenum target, bool ext_dsa)
{
	GLint width = 128, height = 64, depth = 4, levels = 8;
	const char *targetString = piglit_get_gl_enum_name(target);
	GLuint tex;
	GLint v, l;

	assert(target == GL_TEXTURE_1D ||
	       target == GL_TEXTURE_2D ||
	       target == GL_TEXTURE_3D);

	glGenTextures(1, &tex);
	glBindTexture(target, tex);
	do_texture_storage(tex, target, ext_dsa, levels, GL_RGBA8, width, height, depth);
	piglit_check_gl_error(GL_NO_ERROR);

	glGetTexParameteriv(target, GL_TEXTURE_IMMUTABLE_FORMAT, &v);
	if (!v) {
		printf("%s: %s GL_TEXTURE_IMMUTABLE_FORMAT query returned "
		       "false\n",
		       TestName, targetString);
		return PIGLIT_FAIL;
	}

	for (l = 0; l < levels; l++) {
		glGetTexLevelParameteriv(target, l, GL_TEXTURE_WIDTH, &v);
		if (v != width) {
			printf("%s: %s level %d: bad width: %d, should be %d\n",
			       TestName, targetString, l, v, width);
			return PIGLIT_FAIL;
		}

		if (target != GL_TEXTURE_1D) {
			glGetTexLevelParameteriv(target, l, GL_TEXTURE_HEIGHT,
						 &v);
			if (v != height) {
				printf("%s: %s level %d: bad height: %d, "
				       "should be %d\n",
				       TestName, targetString, l, v, height);
				return PIGLIT_FAIL;
			}
		}

		if (target == GL_TEXTURE_3D) {
			glGetTexLevelParameteriv(target, l, GL_TEXTURE_DEPTH,
						 &v);
			if (v != depth) {
				printf("%s: %s level %d: bad depth: %d, "
				       "should be %d\n",
				       TestName, targetString, l, v, depth);
				return PIGLIT_FAIL;
			}
		}

		if (width > 1)
			width /= 2;
		if (height > 1)
			height /= 2;
		if (depth > 1)
			depth /= 2;
	}

	glDeleteTextures(1, &tex);

	return PIGLIT_PASS;
}


static enum piglit_result
test_cube_texture(bool ext_dsa)
{
	const GLint width = 16, height = 16;
	const GLenum target = GL_TEXTURE_CUBE_MAP;
	GLuint tex;
	enum piglit_result result = PIGLIT_PASS;

	if (piglit_get_gl_version() < 13
	    && !piglit_is_extension_supported("GL_ARB_texture_cube_map")) {
		return PIGLIT_SKIP;
	}

	/* Test valid cube dimensions */
	glGenTextures(1, &tex);
	glBindTexture(target, tex);
	do_texture_storage(tex, target, ext_dsa, 1, GL_RGBA8, width, height, 0);
	if (!piglit_check_gl_error(GL_NO_ERROR))
		result = PIGLIT_FAIL;
	glDeleteTextures(1, &tex);

	/* Test invalid cube dimensions */
	glGenTextures(1, &tex);
	glBindTexture(target, tex);
	do_texture_storage(tex, target, ext_dsa, 1, GL_RGBA8, width, height + 2, 0);
	if (!piglit_check_gl_error(GL_INVALID_VALUE))
		result = PIGLIT_FAIL;
	glDeleteTextures(1, &tex);

	return result;
}


static enum piglit_result
test_cube_array_texture(bool ext_dsa)
{
	const GLint width = 16, height = 16;
	const GLenum target = GL_TEXTURE_CUBE_MAP_ARRAY;
	GLuint tex;
	enum piglit_result result = PIGLIT_PASS;

	if (!piglit_is_extension_supported("GL_ARB_texture_cube_map_array"))
		return PIGLIT_SKIP;

	/* Test valid cube array dimensions */
	glGenTextures(1, &tex);
	glBindTexture(target, tex);
	do_texture_storage(tex, target, ext_dsa, 1, GL_RGBA8, width, height, 12);
	if (!piglit_check_gl_error(GL_NO_ERROR))
		result = PIGLIT_FAIL;
	glDeleteTextures(1, &tex);

	/* Test invalid cube array width, height dimensions */
	glGenTextures(1, &tex);
	glBindTexture(target, tex);
	do_texture_storage(tex, target, ext_dsa, 1, GL_RGBA8, width, height + 3, 12);
	if (!piglit_check_gl_error(GL_INVALID_VALUE))
		result = PIGLIT_FAIL;
	glDeleteTextures(1, &tex);

	/* Test invalid cube array depth */
	glGenTextures(1, &tex);
	glBindTexture(target, tex);
	do_texture_storage(tex, target, ext_dsa, 1, GL_RGBA8, width, height, 12 + 2);
	if (!piglit_check_gl_error(GL_INVALID_VALUE))
		result = PIGLIT_FAIL;
	glDeleteTextures(1, &tex);

	return result;
}


/**
 * Create a single-color image.
 */
static GLubyte *
create_image(GLint w, GLint h, const GLubyte color[4])
{
	GLubyte *buf = (GLubyte *) malloc(w * h * 4);
	int i;
	for (i = 0; i < w * h; i++) {
		buf[i*4+0] = color[0];
		buf[i*4+1] = color[1];
		buf[i*4+2] = color[2];
		buf[i*4+3] = color[3];
	}
	return buf;
}


/**
 * Test a mip-mapped texture w/ rendering.
 */
static enum piglit_result
test_2d_mipmap_rendering(bool ext_dsa)
{
	GLuint tex;
	GLint width = 128, height = 64, levels = 8;
	GLint v, l;

	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);

	do_texture_storage(tex, GL_TEXTURE_2D, ext_dsa, levels, GL_RGBA8, width, height, 0);

	piglit_check_gl_error(GL_NO_ERROR);

	/* check that the mipmap level sizes are correct */
	for (l = 0; l < levels; l++) {
		GLubyte *buf = create_image(width, height, Colors[l]);

		glTexSubImage2D(GL_TEXTURE_2D, l, 0, 0, width, height,
				GL_RGBA, GL_UNSIGNED_BYTE, buf);

		free(buf);

		glGetTexLevelParameteriv(GL_TEXTURE_2D, l, GL_TEXTURE_WIDTH,
					 &v);
		if (v != width) {
			printf("%s: level %d: bad width: %d, should be %d\n",
					 TestName, l, v, width);
			return PIGLIT_FAIL;
		}

		glGetTexLevelParameteriv(GL_TEXTURE_2D, l, GL_TEXTURE_HEIGHT,
					 &v);
		if (v != height) {
			printf("%s: level %d: bad height: %d, should be %d\n",
					 TestName, l, v, height);
			return PIGLIT_FAIL;
		}

		if (width > 1)
			width /= 2;
		if (height > 1)
			height /= 2;
	}

	/* This should generate and error (bad level) */
	{
		GLubyte *buf = create_image(width, height, Colors[l]);
		GLenum err;

		glTexSubImage2D(GL_TEXTURE_2D, levels, 0, 0, width, height,
				GL_RGBA, GL_UNSIGNED_BYTE, buf);

		err = glGetError();
		if (err == GL_NO_ERROR) {
			printf("%s: glTexSubImage2D(illegal level) failed to "
			       "generate an error.\n",
			       TestName);
			return PIGLIT_FAIL;
		}

		free(buf);
	}

	/* now do a rendering test */
	glEnable(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
			GL_NEAREST_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	/* draw a quad using each texture mipmap level */
	for (l = 0; l < levels; l++) {
		GLfloat expected[4];
		GLint p;

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, l);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, l);

		glClear(GL_COLOR_BUFFER_BIT);

		piglit_draw_rect_tex(-1.0, -1.0, 2.0, 2.0,
				     0.0, 0.0, 1.0, 1.0);

		expected[0] = Colors[l][0] / 255.0;
		expected[1] = Colors[l][1] / 255.0;
		expected[2] = Colors[l][2] / 255.0;
		expected[3] = Colors[l][3] / 255.0;

		p = piglit_probe_pixel_rgb(piglit_width/2, piglit_height/2,
					   expected);

		piglit_present_results();

		if (!p) {
			printf("%s: wrong color for mipmap level %d\n",
			       TestName, l);
			return PIGLIT_FAIL;
		}
	}

	glDisable(GL_TEXTURE_2D);

	glDeleteTextures(1, &tex);

	return PIGLIT_PASS;
}


/**
 * Per issue 27 of the spec, only sized internalFormat values are allowed.
 * Ex: GL_RGBA8 is OK but GL_RGBA is illegal.
 * Check some common formats here.  These lists aren't exhaustive since
 * there are many extensions/versions that could effect the lists (ex:
 * integer formats, etc.)
 */
static enum piglit_result
test_internal_formats(bool ext_dsa)
{
	const GLenum target = GL_TEXTURE_2D;
	static const GLenum legal_formats[] = {
		GL_RGB4,
		GL_RGB5,
		GL_RGB8,
		GL_RGBA2,
		GL_RGBA4,
		GL_RGBA8,
		GL_DEPTH_COMPONENT16,
		GL_DEPTH_COMPONENT32
	};
	static const GLenum illegal_formats[] = {
		GL_ALPHA,
		GL_LUMINANCE,
		GL_LUMINANCE_ALPHA,
		GL_INTENSITY,
		GL_RGB,
		GL_RGBA,
		GL_DEPTH_COMPONENT,
		GL_COMPRESSED_ALPHA,
		GL_COMPRESSED_LUMINANCE_ALPHA,
		GL_COMPRESSED_LUMINANCE,
		GL_COMPRESSED_INTENSITY,
		GL_COMPRESSED_RGB,
		GL_COMPRESSED_RGBA,
		GL_COMPRESSED_RGBA,
		GL_COMPRESSED_SRGB,
		GL_COMPRESSED_SRGB_ALPHA,
		GL_COMPRESSED_SLUMINANCE,
		GL_COMPRESSED_SLUMINANCE_ALPHA
	};
	GLuint tex;
	enum piglit_result result = PIGLIT_PASS;
	int i;

	for (i = 0; i < ARRAY_SIZE(legal_formats); i++) {
		glGenTextures(1, &tex);
		glBindTexture(target, tex);

		do_texture_storage(tex, target, ext_dsa, 1, legal_formats[i], 32, 32, 0);

		if (!piglit_check_gl_error(GL_NO_ERROR)) {
			printf("%s: internal format %s should be legal"
			       " but raised an error.",
			       TestName,
			       piglit_get_gl_enum_name(legal_formats[i]));
			result = PIGLIT_FAIL;
		}

		glDeleteTextures(1, &tex);
	}

	for (i = 0; i < ARRAY_SIZE(illegal_formats); i++) {
		glGenTextures(1, &tex);
		glBindTexture(target, tex);

		do_texture_storage(tex, target, ext_dsa, 1, illegal_formats[i], 32, 32, 0);

		if (!piglit_check_gl_error(GL_INVALID_ENUM)) {
			printf("%s: internal format %s should be illegal"
			       " but didn't raised an error.",
			       TestName,
			       piglit_get_gl_enum_name(illegal_formats[i]));
			result = PIGLIT_FAIL;
		}

		glDeleteTextures(1, &tex);
	}

	return result;
}
	
static enum piglit_result
test_immutablity(GLenum target, bool ext_dsa)
{
	GLuint tex;
	GLint level;
	GLint immutable_format;

	enum piglit_result result = PIGLIT_PASS;

	glGenTextures(1, &tex);
	glBindTexture(target, tex);

	do_texture_storage(tex, target, ext_dsa, 3, GL_RGBA8, 256, 256, 0);

	glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, 4);
	glGetTexParameteriv(target, GL_TEXTURE_MAX_LEVEL, &level);
	glGetTexParameteriv(target, GL_TEXTURE_IMMUTABLE_FORMAT,
			    &immutable_format);

	if (immutable_format != GL_TRUE) {
		printf("%s: GL_TEXTURE_IMMUTABLE_FORMAT was not set to "
		       "GL_TRUE after glTexStorage2D\n", TestName);
		result = PIGLIT_FAIL;
	}
	if (level != 2) {
		/* The ARB_texture_storage spec says:
		 *
		 *     "However, if TEXTURE_IMMUTABLE_FORMAT is TRUE, then
		 *     level_base is clamped to the range [0, <levels> - 1]
		 *     and level_max is then clamped to the range [level_base,
		 *     <levels> - 1], where <levels> is the parameter passed
		 *     the call to TexStorage* for the texture object"
		 */
		printf("%s: GL_TEXTURE_MAX_LEVEL changed to %d, which is "
		       "outside the clamp range for immutables\n",
		       TestName, level);
		result = PIGLIT_FAIL;
	}

	/* Other immutable tests happen per-format above */

	glDeleteTextures(1, &tex);
	return result;
}

/*
 * According to the ARB_texture_storage specification, issue #22, it is
 * possible to use GenerateMipmap with an incomplete mipmap pyramid. Since the
 * texture is immutable, no new levels are generated.
 */
static enum piglit_result
test_generate_mipmap(bool ext_dsa)
{
	if (!piglit_is_extension_supported("GL_EXT_framebuffer_object"))
		return PIGLIT_SKIP;

	static float level_1_image[2*2*4] = {
		1.0, 0.0, 0.0, 1.0,
		0.0, 1.0, 0.0, 1.0,
		0.0, 0.0, 1.0, 1.0,
		1.0, 1.0, 0.0, 1.0
	};

	float level_0_image[4*4*4];
	float *ptr;
	GLuint tex;
	GLint level;
	int x, y;
	enum piglit_result result = PIGLIT_PASS;

	ptr = level_0_image;
	for (y = 0; y < 4; ++y) {
		for (x = 0; x < 4; ++x, ptr += 4) {
			float *src = &level_1_image[4 * (x / 2 + 2 * (y / 2))];
			ptr[0] = src[0];
			ptr[1] = src[1];
			ptr[2] = src[2];
			ptr[3] = src[3];
		}
	}

	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);

	do_texture_storage(tex, GL_TEXTURE_2D, ext_dsa, 2, GL_RGBA8, 4, 4, 0);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_RGBA,
			GL_FLOAT, level_0_image);
	glGenerateMipmapEXT(GL_TEXTURE_2D);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
			GL_NEAREST_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glEnable(GL_TEXTURE_2D);

	for (level = 0; level <= 1 && result == PIGLIT_PASS; ++level) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, level);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, level);

		glClear(GL_COLOR_BUFFER_BIT);

		piglit_draw_rect_tex(-1.0, -1.0, 2.0, 2.0,
				     0.0, 0.0, 1.0, 1.0);

		ptr = level_1_image;
		for (y = 0; y < 2 && result == PIGLIT_PASS; ++y) {
			for (x = 0; x < 2 && result == PIGLIT_PASS; ++x, ptr += 4) {
				int px = (piglit_width / 4) * (1 + 2 * x);
				int py = (piglit_height / 4) * (1 + 2 * y);
				if (!piglit_probe_pixel_rgba(px, py, ptr)) {
					printf("%s: wrong color for mipmap "
					       "level %d\n", TestName, level);
					result = PIGLIT_FAIL;
				}
			}
		}

		piglit_present_results();
	}

	glDisable(GL_TEXTURE_2D);

	glDeleteTextures(1, &tex);
	return result;
}

#define X(f, n)                                                                      \
	do {                                                                         \
		const enum piglit_result subtest_result = (f);                       \
		piglit_report_subtest_result(subtest_result,                         \
					     (n " %s"), ext_dsa ? "(EXT_dsa)" : ""); \
		piglit_merge_result(&result, subtest_result);                        \
	} while (0)

enum piglit_result
piglit_display(void)
{
	enum piglit_result result = PIGLIT_SKIP;
	bool ext_dsa_supported = piglit_is_extension_supported("GL_EXT_direct_state_access");
	bool ext_dsa = false;

retry_with_ext_dsa:

	X(test_one_level_errors(GL_TEXTURE_1D, ext_dsa), "1D non-mipmapped");
	X(test_one_level_errors(GL_TEXTURE_2D, ext_dsa), "2D non-mipmapped");
	X(test_one_level_errors(GL_TEXTURE_3D, ext_dsa), "3D non-mipmapped");
	X(test_mipmap_errors(GL_TEXTURE_1D, ext_dsa), "1D mipmapped");
	X(test_mipmap_errors(GL_TEXTURE_2D, ext_dsa), "2D mipmapped");
	X(test_mipmap_errors(GL_TEXTURE_3D, ext_dsa), "3D mipmapped");
	X(test_2d_mipmap_rendering(ext_dsa), "2D mipmap rendering");
	X(test_internal_formats(ext_dsa), "internal formats");
	X(test_immutablity(GL_TEXTURE_2D, ext_dsa), "immutability");
	X(test_cube_texture(ext_dsa), "cube texture");
	X(test_cube_array_texture(ext_dsa), "cube array texture");
	X(test_generate_mipmap(ext_dsa), "generate mipmap");

	if (ext_dsa_supported && !ext_dsa) {
		ext_dsa = true;
		goto retry_with_ext_dsa;
	}

	return result;
}


void
piglit_init(int argc, char **argv)
{
	piglit_require_extension("GL_ARB_texture_storage");
}
