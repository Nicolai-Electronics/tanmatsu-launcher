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

#include "pax_codecs.h"
#include "pax_internal.h"
#include "spng.h"
#include <inttypes.h>
#include <stdlib.h>

static const char *TAG = "pax_codecs";

static const uint32_t adam7_x_start[7] = { 0, 4, 0, 2, 0, 1, 0 };
static const uint32_t adam7_x_delta[7] = { 8, 8, 4, 4, 2, 2, 1 };

static bool png_info(pax_png_info_t *info, spng_ctx *ctx);
static bool png_encode(const pax_buf_t *framebuffer, spng_ctx *ctx, int x, int y, int width, int height);
static bool png_decode(pax_buf_t *framebuffer, spng_ctx *ctx, pax_buf_type_t buf_type, int flags, int x, int y);
static bool png_decode_progressive(pax_buf_t *framebuffer, spng_ctx *ctx, struct spng_ihdr ihdr, pax_buf_type_t buf_type, int dx, int dy, int flags);

// Decodes a PNG file into a PAX buffer with the specified type.
// Returns 1 on successful decode, refer to pax_last_error otherwise.
// It is not gauranteed the type equals buf_type.
bool pax_info_png_fd(pax_png_info_t *info, FILE *fd) {
	spng_ctx *ctx = spng_ctx_new(0);
	int err = spng_set_png_file(ctx, fd);
	if (err) {
		spng_ctx_free(ctx);
		return false;
	}
	bool ret = png_info(info, ctx);
	spng_ctx_free(ctx);
	return ret;
}

// Decodes a PNG buffer into a PAX buffer with the specified type.
// Returns 1 on successful decode, refer to pax_last_error otherwise.
// It is not gauranteed the type equals buf_type.
bool pax_info_png_buf(pax_png_info_t *info, const void *buf, size_t buf_len) {
	spng_ctx *ctx = spng_ctx_new(0);
	int err = spng_set_png_buffer(ctx, buf, buf_len);
	if (err) {
		spng_ctx_free(ctx);
		return false;
	}
	bool ret = png_info(info, ctx);
	spng_ctx_free(ctx);
	return ret;
}


// Encodes a pax buffer into a PNG file.
// Returns 1 on successful encode, refer to pax_last_error otherwise.
bool pax_encode_png_fd(const pax_buf_t *buf, FILE *fd, int x, int y, int width, int height) {
	spng_ctx *ctx = spng_ctx_new(SPNG_CTX_ENCODER);
	int err = spng_set_png_file(ctx, fd);
	if (err) {
		PAX_LOGE(TAG, "%s", spng_strerror(err));
		spng_ctx_free(ctx);
		#if PAX_VERSION_MAJOR >= 2
		pax_set_err(PAX_ERR_ENCODE);
		#else
		pax_last_error = PAX_ERR_ENCODE;
		#endif
		return false;
	}
	bool ret = png_encode(buf, ctx, x, y, width, height);
	spng_ctx_free(ctx);
	return ret;
}

// Encodes a pax buffer into a PNG buffer.
// Returns 1 on successful encode, refer to pax_last_error otherwise.
bool pax_encode_png_buf(const pax_buf_t *buf, void **outbuf, size_t *len, int x, int y, int width, int height) {
	spng_ctx *ctx = spng_ctx_new(SPNG_CTX_ENCODER);
    spng_set_option(ctx, SPNG_ENCODE_TO_BUFFER, 1);
	bool ret = png_encode(buf, ctx, x, y, width, height);
	if (!ret) {
		spng_ctx_free(ctx);
		#if PAX_VERSION_MAJOR >= 2
		pax_set_err(PAX_ERR_ENCODE);
		#else
		pax_last_error = PAX_ERR_ENCODE;
		#endif
		return 0;
	}
	
	int err;
	*outbuf = spng_get_png_buffer(ctx, len, &err);
	spng_ctx_free(ctx);
	if (err) {
		PAX_LOGE(TAG, "%s", spng_strerror(err));
		#if PAX_VERSION_MAJOR >= 2
		pax_set_err(PAX_ERR_ENCODE);
		#else
		pax_last_error = PAX_ERR_ENCODE;
		#endif
		*outbuf = NULL;
		*len = 0;
	}
	return !err;
}


// Decodes a PNG file into a buffer with the specified type.
// Returns 1 on successful decode, refer to pax_last_error otherwise.
bool pax_decode_png_fd(pax_buf_t *framebuffer, FILE *fd, pax_buf_type_t buf_type, int flags) {
	spng_ctx *ctx = spng_ctx_new(0);
	int err = spng_set_png_file(ctx, fd);
	if (err) {
		spng_ctx_free(ctx);
		return false;
	}
	bool ret = png_decode(framebuffer, ctx, buf_type, flags, 0, 0);
	spng_ctx_free(ctx);
	return ret;
}

// Decodes a PNG buffer into a PAX buffer with the specified type.
// Returns 1 on successful decode, refer to pax_last_error otherwise.
bool pax_decode_png_buf(pax_buf_t *framebuffer, const void *buf, size_t buf_len, pax_buf_type_t buf_type, int flags) {
	spng_ctx *ctx = spng_ctx_new(0);
	int err = spng_set_png_buffer(ctx, buf, buf_len);
	if (err) {
		spng_ctx_free(ctx);
		return false;
	}
	bool ret = png_decode(framebuffer, ctx, buf_type, flags, 0, 0);
	spng_ctx_free(ctx);
	return ret;
}


// Decodes a PNG file into an existing PAX buffer.
// Takes an x/y pair for offset.
// Returns 1 on successful decode, refer to pax_last_error otherwise.
bool pax_insert_png_fd(pax_buf_t *framebuffer, FILE *fd, int x, int y, int flags) {
	spng_ctx *ctx = spng_ctx_new(0);
	int err = spng_set_png_file(ctx, fd);
	if (err) {
		spng_ctx_free(ctx);
		return false;
	}
	bool ret = png_decode(framebuffer, ctx, framebuffer->type, flags | CODEC_FLAG_EXISTING, x, y);
	spng_ctx_free(ctx);
	return ret;
}

// Decodes a PNG buffer into an existing PAX buffer.
// Takes an x/y pair for offset.
// Returns 1 on successful decode, refer to pax_last_error otherwise.
bool pax_insert_png_buf(pax_buf_t *framebuffer, const void *png, size_t png_len, int x, int y, int flags) {
	spng_ctx *ctx = spng_ctx_new(0);
	int err = spng_set_png_buffer(ctx, png, png_len);
	if (err) {
		spng_ctx_free(ctx);
		return false;
	}
	bool ret = png_decode(framebuffer, ctx, framebuffer->type, flags | CODEC_FLAG_EXISTING, x, y);
	spng_ctx_free(ctx);
	return ret;
}


// A generic wrapper for getting PNG infos.
static bool png_info(pax_png_info_t *info, spng_ctx *ctx) {
	struct spng_ihdr ihdr;
	int err = spng_get_ihdr(ctx, &ihdr);
	if (err) {
		PAX_LOGE(TAG, "Failed at spng_get_ihdr");
		PAX_LOGE(TAG, "PNG decode error %d: %s", err, spng_strerror(err));
		return false;
	}
	info->width  = ihdr.width;
	info->height = ihdr.height;
	info->bit_depth = ihdr.bit_depth;
	info->color_type = ihdr.color_type;
	return true;
}

// A generic wrapper for encoding PNGs.
static bool png_encode(const pax_buf_t *framebuffer, spng_ctx *ctx, int dx, int dy, int width, int height) {
	// Clamp: horizontal.
	if (dx < 0) {
		dx     = 0;
		width += dx;
	}
	if (dx > pax_buf_get_width(framebuffer)) {
		// Out of bounds error.
		#if PAX_VERSION_MAJOR >= 2
		pax_set_err(PAX_ERR_BOUNDS);
		#else
		pax_last_error = PAX_ERR_BOUNDS;
		#endif
		return 0;
	}
	if (dx + width > pax_buf_get_width(framebuffer)) {
		width = pax_buf_get_width(framebuffer) - dx;
	}
	
	// Clamp: vertical.
	if (dy < 0) {
		dy      = 0;
		height += dy;
	}
	if (dy > pax_buf_get_height(framebuffer)) {
		// Out of bounds error.
		#if PAX_VERSION_MAJOR >= 2
		pax_set_err(PAX_ERR_BOUNDS);
		#else
		pax_last_error = PAX_ERR_BOUNDS;
		#endif
		return 0;
	}
	if (dy + height > pax_buf_get_height(framebuffer)) {
		height = pax_buf_get_height(framebuffer) - dy;
	}
	
	// Set image properties.
    struct spng_ihdr ihdr = {0};
    ihdr.width = width;
    ihdr.height = height;
    ihdr.color_type = SPNG_COLOR_TYPE_TRUECOLOR_ALPHA;
    ihdr.bit_depth = 8;
	spng_set_ihdr(ctx, &ihdr);
	
	// Set encoding mode.
	int err = spng_encode_image(ctx, NULL, 0, SPNG_FMT_PNG, SPNG_ENCODE_PROGRESSIVE | SPNG_ENCODE_FINALIZE);
	
	// Encode a few rows.
	size_t   rowbufcap = sizeof(uint8_t) * 4 * width;
	uint8_t *rowbuf    = malloc(rowbufcap);
	if (!rowbuf) {
		#if PAX_VERSION_MAJOR >= 2
		pax_set_err(PAX_ERR_NOMEM);
		#else
		pax_last_error = PAX_ERR_NOMEM;
		#endif
		return 0;
	}
	
	for (int y = 0; y < height; y++) {
		// Grab a row of pixels.
		for (int x = 0; x < width; x++) {
			pax_col_t col = pax_get_pixel(framebuffer, x + dx, y + dy);
			rowbuf[4*x+0] = col >> 16; // R
			rowbuf[4*x+1] = col >> 8;  // G
			rowbuf[4*x+2] = col >> 0;  // B
			rowbuf[4*x+3] = col >> 24; // A
		}
		
		// Feed it to the encoder.
		err = spng_encode_row(ctx, rowbuf, rowbufcap);
		if (err) break;
	}
	free(rowbuf);
	
	if (err != SPNG_EOI) {
		PAX_LOGE(TAG, "%s", spng_strerror(err));
		#if PAX_VERSION_MAJOR >= 2
		pax_set_err(PAX_ERR_ENCODE);
		#else
		pax_last_error = PAX_ERR_ENCODE;
		#endif
		return 0;
	}
	
	return 1;
}

// A generic wrapper for decoding PNGs.
// Sets up the framebuffer if required.
static bool png_decode(pax_buf_t *framebuffer, spng_ctx *ctx, pax_buf_type_t buf_type, int flags, int x_offset, int y_offset) {
	bool do_alloc = !(flags & CODEC_FLAG_EXISTING);
	if (do_alloc) {
		framebuffer->width  = 0;
		framebuffer->height = 0;
	} else {
		buf_type = framebuffer->type;
	}
	
	// Fetch the IHDR.
	struct spng_ihdr ihdr;
	int err = spng_get_ihdr(ctx, &ihdr);
	if (err) {
		PAX_LOGE(TAG, "Failed at spng_get_ihdr");
		PAX_LOGE(TAG, "PNG decode error %d: %s", err, spng_strerror(err));
		return false;
	}
	uint32_t width      = ihdr.width;
	uint32_t height     = ihdr.height;
	if (do_alloc) {
		framebuffer->width  = width;
		framebuffer->height = height;
	} else {
		pax_mark_dirty2(framebuffer, x_offset, y_offset, width, height);
	}
	
#if PAX_VERSION_MAJOR >= 2
	bool is_palette = pax_buf_type_info(buf_type).fmt_type == PAX_BUF_SUBTYPE_PALETTE;
#else
	bool is_palette = PAX_IS_PALETTE(buf_type);
#endif
	
	// Select a good buffer type.
	if (do_alloc && is_palette && ihdr.color_type != 3) {
		// This is not a palleted image, change the output type.
		#if PAX_VERSION_MAJOR >= 2
		int bpp = pax_buf_type_info(buf_type).bpp;
		#else
		int bpp = PAX_GET_BPP(buf_type);
		#endif
		if (bpp == 1) {
			// For 1BPP, the only option is greyscale.
			buf_type = PAX_BUF_1_GREY;
		} else if (bpp == 2) {
			// For 2BPP, the only option is also greyscale.
			buf_type = PAX_BUF_2_PAL;
		} else if (bpp == 4) {
			if ((ihdr.color_type & 4) || (ihdr.color_type & 2)) {
				// With alpha and/or color.
				buf_type = PAX_BUF_4_1111ARGB;
			} else {
				// Greyscale.
				buf_type = PAX_BUF_4_GREY;
			}
		} else if (bpp == 8) {
			if (ihdr.color_type & 4) {
				// With alpha and/or color.
				buf_type = PAX_BUF_8_2222ARGB;
			} else if (ihdr.color_type & 2) {
				// With color.
				buf_type = PAX_BUF_8_332RGB;
			} else {
				// Greyscale.
				buf_type = PAX_BUF_8_GREY;
			}
		} else {
			if (ihdr.color_type & 4) {
				// With alpha and/or color.
				buf_type = PAX_BUF_16_4444ARGB;
			} else if (ihdr.color_type & 2) {
				// With color.
				buf_type = PAX_BUF_16_565RGB;
			} else {
				// Greyscale.
				buf_type = PAX_BUF_8_GREY;
			}
		}
		PAX_LOGW(TAG, "Changing buffer type to %08x", (int)buf_type);
	}
	
	// Determine whether to allocate a buffer.
	if (do_alloc) {
		// Allocate some funny.
		PAX_LOGD(TAG, "Decoding PNG %dx%d to %08x", (int) width, (int) height, buf_type);
		#if PAX_VERSION_MAJOR >= 2
		if (!pax_buf_init(framebuffer, NULL, width, height, buf_type)) return false;
		#else
		pax_buf_init(framebuffer, NULL, width, height, buf_type);
		if (pax_last_error) return false;
		#endif
	}
	
	// Decd.
	if (!png_decode_progressive(framebuffer, ctx, ihdr, buf_type, x_offset, y_offset, flags)) {
		goto error;
	}
	
	// Success.
	return true;
	
	error:
	if (do_alloc) {
		// Clean up in case of erruer.
		pax_buf_destroy(framebuffer);
	}
	return false;
}

// Get the closest palette color.
static pax_col_t closest_palette_index(pax_buf_t *buf, pax_col_t argb, bool ignore_alpha) {
	pax_col_t closest_index = 0;
	uint16_t  closest_err   = UINT16_MAX;
	for (size_t y = 0; y < buf->palette_size; y++) {
		// Extract color components.
		uint8_t  fb_a  = buf->palette[y] >> 24;
		uint8_t  fb_r  = buf->palette[y] >> 16;
		uint8_t  fb_g  = buf->palette[y] >> 8;
		uint8_t  fb_b  = buf->palette[y];
		uint8_t  png_a = argb >> 24;
		uint8_t  png_r = argb >> 16;
		uint8_t  png_g = argb >> 8;
		uint8_t  png_b = argb;
		// Determine how close the two are.
		uint16_t err   = abs(png_r - fb_r) + abs(png_g - fb_g) + abs(png_b - fb_b);
		if (!ignore_alpha) {
			err += abs(png_a - fb_a);
		}
		if (err < closest_err) {
			closest_err   = err;
			closest_index = y;
		}
	}
	return closest_index;
}

// A WIP decode inator.
static bool png_decode_progressive(pax_buf_t *framebuffer, spng_ctx *ctx, struct spng_ihdr ihdr, pax_buf_type_t buf_type, int x_offset, int y_offset, int flags) {
	int err = 0;
	uint8_t          *row  = NULL;
	struct spng_plte *plte = NULL;
	struct spng_trns *trns = NULL;
	
#if PAX_VERSION_MAJOR >= 2
bool is_palette = pax_buf_type_info(buf_type).fmt_type == PAX_BUF_SUBTYPE_PALETTE;
#else
bool is_palette = PAX_IS_PALETTE(buf_type);
#endif
	
	PAX_LOGD(TAG, "Decode with flags 0x%08x", flags);
	
	// Get image parameters.
	uint32_t width    = ihdr.width;
	uint32_t height   = ihdr.height;
	
	// Reduce 16pbc back to 8pbc.
	int png_fmt;
	int bits_per_pixel;
	uint32_t channel_mask;
	uint_fast8_t shift_max = 0;
	switch (ihdr.color_type) {
		case 0:
			// Greyscale.
			png_fmt = SPNG_FMT_G8;
			bits_per_pixel = 1 * 8;
			channel_mask = 0x000000ff;
			break;
		case 2:
			// RGB.
			png_fmt = SPNG_FMT_RGB8;
			bits_per_pixel = 3 * 8;
			channel_mask = 0x00ffffff;
			break;
		case 3:
			// Palette.
			png_fmt = SPNG_FMT_RAW;
			bits_per_pixel = 1 * ihdr.bit_depth;
			channel_mask = (1 << bits_per_pixel) - 1;
			shift_max = 8 - ihdr.bit_depth;
			break;
		case 4:
			// Greyscale and alpha.
			png_fmt = SPNG_FMT_GA8;
			bits_per_pixel = 2 * 8;
			channel_mask = 0x0000ffff;
			break;
		case 6:
		default:
			// RGBA.
			png_fmt = SPNG_FMT_RGBA8;
			bits_per_pixel = 4 * 8;
			channel_mask = 0xffffffff;
			break;
	}
	PAX_LOGD(TAG, "PNG FMT %d", png_fmt);
	
	// Get the size for the fancy buffer.
	size_t   decd_len = 0;
	err = spng_decoded_image_size(ctx, png_fmt, &decd_len);
	if (err) {
		PAX_LOGE(TAG, "Failed at spng_decoded_image_size");
		goto error;
	}
	size_t   row_size = decd_len / height;
	row = malloc(row_size);
	err = spng_decode_chunks(ctx);
	if (err) {
		PAX_LOGE(TAG, "Failed at spng_decode_chunks (1)");
		goto error;
	}
	
	// Get the palette, if any.
	bool has_palette = ihdr.color_type == 3;
	bool has_trns    = has_palette;
	plte = malloc(sizeof(struct spng_plte));
	trns = malloc(sizeof(struct spng_trns));
	if (!plte || !trns) {
		PAX_LOGE(TAG, "Out of memory");
		goto error;
	}
	if (has_palette) {
		PAX_LOGD(TAG, "PNG has palette");
		
		// Color part of palette.
		err = spng_get_plte(ctx, plte);
		if (err && err != SPNG_ECHUNKAVAIL) goto error;
		
		// Alpha part of palette.
		err = spng_get_trns(ctx, trns);
		if (err == SPNG_ECHUNKAVAIL) has_trns = false;
		else if (err) goto error;
	}
	
	// Set the image to decode progressive.
	err = spng_decode_image(ctx, NULL, 0, png_fmt, SPNG_DECODE_PROGRESSIVE);
	if (err) {
		PAX_LOGE(TAG, "Failed at spng_decode_image");
		goto error;
	}
	
	// Decoding time!
	struct spng_row_info info;
	while (1) {
		// Get row metadata.
		err = spng_get_row_info(ctx, &info);
		if (err && err != SPNG_EOI) goto error;
		
		// Decode a row's data.
		err = spng_decode_scanline(ctx, row, row_size);
		if (err && err != SPNG_EOI) goto error;
		
		// Have it sharted out.
		uint32_t dx     = 1;
		size_t   offset = 0;
		uint32_t x      = 0;
		if (ihdr.interlace_method) {
			// Adam7 interlace.
			x  = adam7_x_start[info.pass];
			dx = adam7_x_delta[info.pass];
		}
		for (; x < width; x += dx) {
			// Get the raw data.
			void* address = row + (offset / 8);
			// A slightly complicated bit extraction.
			uint32_t raw = channel_mask & (*(uint32_t *) address >> (shift_max - (offset % 8)));
			// Fix endianness.
			if (bits_per_pixel == 16) raw = (raw << 8) | (raw >> 8);
			else if (bits_per_pixel == 24) raw = (raw << 16) | (raw >> 16) | (raw & 0x00ff00);
			else if (bits_per_pixel == 32) raw = (raw << 24) | ((raw << 8) & 0x00ff0000) | ((raw >> 8) & 0x0000ff00) | (raw >> 24);
			offset += bits_per_pixel;
			
			// Decode color information.
			pax_col_t color = 0;
			if (has_palette && is_palette) {
				color = raw;
			} else if (has_palette) {
				if (raw >= plte->n_entries) raw = 0;
				// Alpha palette.
				if (has_trns && raw < trns->n_type3_entries) {
					color = trns->type3_alpha[raw] << 24;
				} else {
					color = 0xff000000;
				}
				// Non-alpha palette.
				struct spng_plte_entry entry = plte->entries[raw];
				color |= (entry.red << 16) | (entry.green << 8) | entry.blue;
			} else if (ihdr.color_type == 0) {
				// Greyscale.
				color = 0xff000000 | (raw * 0x010101);
			} else if (ihdr.color_type == 2) {
				// RGB.
				color = 0xff000000 | raw;
			} else if (ihdr.color_type == 4) {
				// Greyscale and alpha.
				color = (raw << 24) | ((raw >> 8) * 0x00010101);
			} else if (ihdr.color_type == 6) {
				// RGBA.
				color = (raw >> 8) | (raw << 24);
			}
			
			// Output the pixel to the right spot.
			if (!has_palette && is_palette) {
				color = closest_palette_index(framebuffer, color, true);
				pax_set_pixel(framebuffer, color, x_offset + x, y_offset + info.row_num);
			} else if (flags & CODEC_FLAG_EXISTING && !(has_palette && is_palette)) {
				pax_merge_pixel(framebuffer, color, x_offset + x, y_offset + info.row_num);
			} else {
				pax_set_pixel(framebuffer, color, x_offset + x, y_offset + info.row_num);
			}
		}
		
		if (err == SPNG_EOI) break;
	}
	
	err = spng_decode_chunks(ctx);
	if (err) {
		PAX_LOGE(TAG, "Failed at spng_decode_chunks (2)");
	}
	
	// Get the palette, attempt two.
	if (has_palette) {
		// Color part of palette.
		err = spng_get_plte(ctx, plte);
		if (err) {
			PAX_LOGE(TAG, "spng_get_plte 2");
			goto error;
		}
		
		// Re-map palette written from IDAT.
		if (is_palette && (flags & CODEC_FLAG_EXISTING) && !(flags & CODEC_FLAG_KEEP_PAL)) {
			// Search for closest fitting palette.
			uint16_t *remap = malloc(sizeof(uint16_t) * plte->n_entries);
			PAX_LOGD(TAG, "Remapping palette");
			if (!remap) {
				PAX_LOGE(TAG, "Out of memory");
				goto error;
			}
			for (size_t x = 0; x < plte->n_entries; x++) {
				pax_col_t argb = (plte->entries[x].red << 16) | (plte->entries[x].green << 8) | plte->entries->blue;
				remap[x] = closest_palette_index(framebuffer, argb, true);
				PAX_LOGD(TAG, "%"PRId16" -> %"PRId16, x, remap[x]);
			}
			
			// Go over all written pixels and change the palette index.
			for (int y = y_offset; y < height; y++) {
				for (int x = x_offset; x < width; x++) {
					uint32_t raw = pax_get_pixel(framebuffer, x, y);
					if (raw > plte->n_entries) {
						pax_set_pixel(framebuffer, x, y, 0);
					} else {
						pax_set_pixel(framebuffer, x, y, remap[raw]);
					}
				}
			}
			
			free(remap);
		}
	}
	
	if (has_palette && is_palette && !(flags & CODEC_FLAG_EXISTING)) {
		// Copy over the palette.
		pax_col_t *palette = malloc(sizeof(pax_col_t) * plte->n_entries);
		for (size_t i = 0; i < plte->n_entries; i++) {
			// if (has_trns && i < trns->n_type3_entries) {
			// 	palette[i] = trns->type3_alpha[i] << 24;
			// } else {
				palette[i] = 0xff000000;
			// }
			struct spng_plte_entry entry = plte->entries[i];
			palette[i] |= (entry.red << 16) | (entry.green << 8) | entry.blue;
		}
		framebuffer->palette      = palette;
		framebuffer->palette_size = plte->n_entries;
		framebuffer->do_free_pal  = true;
	}
	
	free(plte);
	free(trns);
	free(row);
	return true;
	
	error:
	if (row)  free(row);
	if (plte) free(plte);
	if (trns) free(trns);
	PAX_LOGE(TAG, "PNG decode error %d: %s", err, spng_strerror(err));
	return false;
}
