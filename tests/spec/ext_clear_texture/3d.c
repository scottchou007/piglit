/*
 * Copyright (c) 2014 Intel Corporation
 * Copyright (c) 2021 Collabora Ltd
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/** @file 3d.c
 *
 * A test of using glClearTexSubImage to clear sub-regions of a 3D
 * texture. A 4x4x4 texture is created with all green data. The region
 * 1x2x2+1+1+1 is cleared to zeroes by setting the data to NULL and
 * the region 1x2x2+2+1+1 is cleared to red. All four 4x4 images are
 * then drawn to the screen in left-to-right order.
 */

#define TEX_WIDTH 4
#define TEX_HEIGHT 4
#define TEX_DEPTH 4

#include "common.h"

PIGLIT_GL_TEST_CONFIG_BEGIN

	config.supports_gl_es_version = 31;

	config.window_visual = PIGLIT_GL_VISUAL_RGB | PIGLIT_GL_VISUAL_DOUBLE;

	config.khr_no_error_support = PIGLIT_NO_ERRORS;

PIGLIT_GL_TEST_CONFIG_END

static const float green[3] = {0.0, 1.0, 0.0};
static const float red[3] = {1.0, 0.0, 0.0};
static const float black[3] = {0.0, 0.0, 0.0};

static GLuint
create_texture(void)
{
	GLubyte tex_data[TEX_WIDTH * TEX_HEIGHT * TEX_DEPTH * 3];
	static const GLubyte green_bytes[3] = { 0x00, 0xff, 0x00 };
	GLuint tex;
	int i;

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	for (i = 0; i < TEX_WIDTH * TEX_HEIGHT * TEX_DEPTH; i++)
		memcpy(tex_data + i * 3, green_bytes, sizeof green_bytes);

	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_3D, tex);
	glTexImage3D(GL_TEXTURE_3D,
		     0, /* level */
		     GL_RGB,
		     TEX_WIDTH, TEX_HEIGHT, TEX_DEPTH,
		     0, /* border */
		     GL_RGB,
		     GL_UNSIGNED_BYTE,
		     tex_data);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	if (!piglit_check_gl_error(GL_NO_ERROR))
		piglit_report_result(PIGLIT_FAIL);

	return tex;
}

static void
clear_texture(GLuint tex)
{
	glClearTexSubImageEXT(tex,
			      0, /* level */
			      1, 1, 1, /* x/y/z */
			      1, 2, 2, /* width/height/depth */
			      GL_RGB,
			      GL_UNSIGNED_BYTE,
			      NULL);
	glClearTexSubImageEXT(tex,
			      0, /* level */
			      2, 1, 1, /* x/y/z */
			      1, 2, 2, /* width/height/depth */
			      GL_RGB,
			      GL_FLOAT,
			      red);

	if (!piglit_check_gl_error(GL_NO_ERROR))
		piglit_report_result(PIGLIT_FAIL);
}

void
piglit_init(int argc, char **argv)
{
	piglit_require_extension("GL_EXT_clear_texture");
	init_program("3D");
}

static void
draw_rect(float x, float y, float width, float height, float tex_z)
{
	struct {
		float x, y;
		float tx, ty, tz;
	} attribs[] = {
		{ x, y, 0.0f, 0.0f, tex_z },
		{ x + width, y, 1.0f, 0.0f, tex_z },
		{ x, y + height, 0.0f, 1.0f, tex_z },
		{ x + width, y + height, 1.0f, 1.0f, tex_z },
	};

	glEnableVertexAttribArray(PIGLIT_ATTRIB_POS);
	glVertexAttribPointer(PIGLIT_ATTRIB_POS,
			      2, /* size */
			      GL_FLOAT,
			      GL_FALSE, /* normalized */
			      sizeof attribs[0],
			      &attribs[0].x);
	glEnableVertexAttribArray(PIGLIT_ATTRIB_TEX);
	glVertexAttribPointer(PIGLIT_ATTRIB_TEX,
			      3, /* size */
			      GL_FLOAT,
			      GL_FALSE, /* normalized */
			      sizeof attribs[0],
			      &attribs[0].tx);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisableVertexAttribArray(PIGLIT_ATTRIB_POS);
	glDisableVertexAttribArray(PIGLIT_ATTRIB_TEX);
}

enum piglit_result
piglit_display(void)
{
	bool pass = true;
	GLuint tex;
	int i;

	tex = create_texture();

	clear_texture(tex);

	glBindTexture(GL_TEXTURE_3D, tex);

	/* Render all of the images to the screen */
	for (i = 0; i < TEX_DEPTH; i++) {
		draw_rect(i * TEX_WIDTH, 0.0, /* x/y */
			  TEX_WIDTH, TEX_HEIGHT,
			  i / (TEX_DEPTH - 1.0f));
	}

	glBindTexture(GL_TEXTURE_3D, 0);

	glDeleteTextures(1, &tex);

	/* First image is all green */
	pass &= piglit_probe_rect_rgb(0, 0, 4, 4, green);

	/* Second image is green with with a short black and red bar
	 * in the middle */
	pass &= piglit_probe_rect_rgb(4, 0, 1, 4, green);
	pass &= piglit_probe_pixel_rgb(5, 0, green);
	pass &= piglit_probe_rect_rgb(5, 1, 1, 2, black);
	pass &= piglit_probe_pixel_rgb(5, 3, green);
	pass &= piglit_probe_pixel_rgb(6, 0, green);
	pass &= piglit_probe_rect_rgb(6, 1, 1, 2, red);
	pass &= piglit_probe_pixel_rgb(6, 3, green);
	pass &= piglit_probe_rect_rgb(7, 0, 1, 4, green);
	/* Third image is the same */
	pass &= piglit_probe_rect_rgb(8, 0, 1, 4, green);
	pass &= piglit_probe_pixel_rgb(9, 0, green);
	pass &= piglit_probe_rect_rgb(9, 1, 1, 2, black);
	pass &= piglit_probe_pixel_rgb(9, 3, green);
	pass &= piglit_probe_pixel_rgb(10, 0, green);
	pass &= piglit_probe_rect_rgb(10, 1, 1, 2, red);
	pass &= piglit_probe_pixel_rgb(10, 3, green);
	pass &= piglit_probe_rect_rgb(11, 0, 1, 4, green);

	/* Fourth image is all green */
	pass &= piglit_probe_rect_rgb(12, 0, 4, 4, green);

	piglit_present_results();

	return pass ? PIGLIT_PASS : PIGLIT_FAIL;
}
