/*
	MIT License

	Copyright (c) 2022 Julian Scheffers

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
*/

#ifndef PAX_CODECS_H
#define PAX_CODECS_H

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

#include "pax_gfx.h"
#include <stdio.h>

typedef struct {
	uint32_t width, height;
	int bit_depth;
	int color_type;
} pax_png_info_t;

// Indicates that any buffer format is acceptable.
// The codec will select the most optimal format available.
#define CODEC_FLAG_OPTIMAL  0x0001
// Indicates that the format must be as strictly met as possible.
// This is not a gaurantee that the exact type is used, especially for palette types.
#define CODEC_FLAG_STRICT   0x0002
#define CODEC_FLAG_EXISTING 0x0100
// Don't try to fix the order of the palette.
#define CODEC_FLAG_KEEP_PAL 0x0004


// Retrieves basic PNG metadata from a file.
// Returns 1 on successful decode, refer to pax_last_error otherwise.
bool pax_info_png_fd (pax_png_info_t *info, FILE *fd);
// Retrieves basic PNG metadata from a buffer.
// Returns 1 on successful decode, refer to pax_last_error otherwise.
bool pax_info_png_buf(pax_png_info_t *info, const void *png, size_t png_len);

// Encodes a pax buffer into a PNG file.
// Returns 1 on successful encode, refer to pax_last_error otherwise.
bool pax_encode_png_fd (const pax_buf_t *buf, FILE *fd, int x, int y, int width, int height);
// Encodes a pax buffer into a PNG buffer.
// Returns 1 on successful encode, refer to pax_last_error otherwise.
bool pax_encode_png_buf(const pax_buf_t *buf, void **outbuf, size_t *len, int x, int y, int width, int height);

// Decodes a PNG file into a PAX buffer with the specified type.
// Returns 1 on successful decode, refer to pax_last_error otherwise.
// It is not gauranteed the type equals buf_type.
bool pax_decode_png_fd (pax_buf_t *buf, FILE *fd, pax_buf_type_t buf_type, int flags);
// Decodes a PNG buffer into a PAX buffer with the specified type.
// Returns 1 on successful decode, refer to pax_last_error otherwise.
// It is not gauranteed the type equals buf_type.
bool pax_decode_png_buf(pax_buf_t *buf, const void *png, size_t png_len, pax_buf_type_t buf_type, int flags);

// Decodes a PNG file into an existing PAX buffer.
// Takes an x/y pair for offset.
// Returns 1 on successful decode, refer to pax_last_error otherwise.
bool pax_insert_png_fd (pax_buf_t *buf, FILE *fd, int x, int y, int flags);
// Decodes a PNG buffer into an existing PAX buffer.
// Takes an x/y pair for offset.
// Returns 1 on successful decode, refer to pax_last_error otherwise.
bool pax_insert_png_buf(pax_buf_t *buf, const void *png, size_t png_len, int x, int y, int flags);

#ifdef __cplusplus
}
#endif //__cplusplus

#endif //PAX_CODECS_H
