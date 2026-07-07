/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include <stdio.h>
#include <string.h>

#include "../../display/simgraph.h"
#include "../../simdebug.h"
#include "../../display/simimg.h"

#include "../image.h"
#include "image_reader.h"
#include "../obj_node_info.h"

#include <zlib.h>
#include "../../tpl/inthashtable_tpl.h"
#include "../../tpl/array_tpl.h"


// if without graphics backend, do not copy any pixel
#if COLOUR_DEPTH != 0
#define skip_reading_pixels_if_no_graphics
#else
#define skip_reading_pixels_if_no_graphics goto adjust_image
#endif

#define ALPHA_THRESHOLD (0xF8000000u)
#define TRANSPARENT_RUN (0x8000u)

#ifdef SIM_ENABLE_RGB32_OUTPUT
static PIXVAL rgb888_to_runtime_pixval(uint32 rgb)
{
	rgb &= 0x00FFFFFFu;

	for(  int i = 0;  i < SPECIAL;  i++  ) {
		if(  image_t::rgbtab[i] == rgb  ) {
			return 0x8000 + i;
		}
	}

	return IMAGE_TRUECOLOR_FLAG | rgb;
}

static PIXVAL argb8888_to_runtime_pixval(uint32 argb)
{
	assert(  argb < ALPHA_THRESHOLD  );

	if(  argb <= 0x00FFFFFFu  ) {
		return rgb888_to_runtime_pixval(argb);
	}

	const uint32 rgb = argb & 0x00FFFFFFu;
	const uint32 alpha = 30 - (argb >> 24) / 8;

	for(  int i = 0;  i < SPECIAL;  i++  ) {
		if(  image_t::rgbtab[i] == rgb  ) {
			return 0x8020 + i * 31 + alpha;
		}
	}

	const PIXVAL pix = ((rgb >> 14) & 0x0380) | ((rgb >> 9) & 0x0078) | ((rgb >> 5) & 0x07);
	return 0x8020 + 31 * 31 + pix * 31 + alpha;
}
#endif

obj_desc_t *image_reader_t::read_node(FILE *fp, obj_node_info_t &node)
{
	array_tpl<char> desc_buf(node.size);
#if COLOUR_DEPTH != 0
	if (fread(desc_buf.begin(), node.size, 1, fp) != 1) {
		return NULL;
	}
#else
	fread(desc_buf.begin(), node.size, 1, fp);
#endif
	char *p = desc_buf.begin()+6;

	// always zero in old version, since length was always less than 65535
	// because a node could not hold more data
	uint8 version = decode_uint8(p);
	p = desc_buf.begin();

#if COLOUR_DEPTH != 0
	image_t *desc = new image_t();
#else
	// reserve space for one single pixel and initialize data
	image_t *desc = image_t::create_single_pixel();
#endif

	if(version==0) {
		desc->x = decode_uint8(p);
		desc->w = decode_uint8(p);
		desc->y = decode_uint8(p);
		desc->h = decode_uint8(p);
		desc->alloc(decode_uint32(p)); // len
		desc->imageid = IMG_EMPTY;
		p += 2; // dummys
		desc->zoomable = decode_uint8(p);

		skip_reading_pixels_if_no_graphics;
		//DBG_DEBUG("image_t::read_node()","x,y=%d,%d  w,h=%d,%d, len=%i",desc->x,desc->y,desc->w,desc->h, desc->len);

		image_pixel_t* dest = desc->data;
		p = desc_buf.begin()+12;

		if (desc->h > 0) {
			for (uint i = 0; i < desc->len; i++) {
				uint16 data = decode_uint16(p);
				if(data>=0x8000u  &&  data<=0x800Fu) {
					// player color offset changed
					data ++;
				}
				*dest++ = data;
			}
		}
	}
	else if(version<=2) {
		desc->x = decode_sint16(p);
		desc->y = decode_sint16(p);
		desc->w = decode_uint8(p);
		desc->h = decode_uint8(p);
		p++; // skip version information
		desc->alloc(decode_uint16(p)); // len
		desc->zoomable = decode_uint8(p);
		desc->imageid = IMG_EMPTY;

		skip_reading_pixels_if_no_graphics;
		image_pixel_t* dest = desc->data;
		if (desc->h > 0) {
			for (uint i = 0; i < desc->len; i++) {
				*dest++ = decode_uint16(p);
			}
		}
	}
	else if(version==3) {
		desc->x = decode_sint16(p);
		desc->y = decode_sint16(p);
		desc->w = decode_sint16(p);
		p++; // skip version information
		desc->h = decode_sint16(p);
		desc->alloc((node.size-10)/2); // len
		desc->zoomable = decode_uint8(p);
		desc->imageid = IMG_EMPTY;

		skip_reading_pixels_if_no_graphics;
		image_pixel_t* dest = desc->data;
		if (desc->h > 0) {
			for (uint i = 0; i < desc->len; i++) {
				*dest++ = decode_uint16(p);
			}
		}
	}
#ifdef SIM_ENABLE_RGB32_OUTPUT
	else if(version==4) {
		desc->x = decode_sint16(p);
		desc->y = decode_sint16(p);
		desc->w = decode_sint16(p);
		p++; // skip version information
		desc->h = decode_sint16(p);
		desc->alloc_truecolor((node.size-10)/4); // len
		desc->zoomable = decode_uint8(p);
		desc->imageid = IMG_EMPTY;
		desc->truecolor = true;

		skip_reading_pixels_if_no_graphics;
		PIXVAL* dest = desc->truecolor_data;
		if (desc->h > 0) {
			for (uint y = 0; y < desc->h; y++) {
				PIXVAL runlen = decode_uint32(p);
				*dest++ = runlen;
				do {
					runlen = decode_uint32(p);
					*dest++ = runlen;
					const uint32 colored = runlen & ~TRANSPARENT_RUN;
					for (uint32 i = 0; i < colored; i++) {
						*dest++ = argb8888_to_runtime_pixval(decode_uint32(p));
					}
					runlen = decode_uint32(p);
					*dest++ = runlen;
				} while(  runlen != 0  );
			}
		}
	}
#endif
	else
	{
		dbg->fatal( "image_reader_t::read_node()", "Cannot handle too new node version %i", version );
	}

#if COLOUR_DEPTH == 0
adjust_image:
	// reset image parameters, but only for non-empty images
	if(  desc->h > 0  ) {
		desc->h = 1;
	}
	if(  desc->w > 0  ) {
		desc->w = 1;
	}
	if(  desc->len > 0  ) {
		desc->len = 4;
		memset(desc->data, 0, desc->len*sizeof(image_pixel_t));
	}
	desc->x = 0;
	desc->y = 0;
#else
	if (!image_has_valid_data(desc)) {
		delete desc;
		return NULL;
	}
#endif

	// check for left corner
	if(version<2  &&  desc->h>0) {
		// find left border
		uint16 left = 255;
		image_pixel_t *dest = desc->data;
		image_pixel_t *end  = desc->data + desc->len;

		for( uint8 y=0;  y<desc->h;  y++  ) {
#if COLOUR_DEPTH != 0
			if (dest >= end) {
				delete desc;
				return NULL;
			}
			left = min(left, *dest);
#else

			if(*dest<left) {
				left = *dest;
			}
#endif

			// skip rest of the line
			do {
				dest++;
#if COLOUR_DEPTH != 0
				if (dest >= end) {
					delete desc;
					return NULL;
				}
#endif
				dest += *dest + 1;
#if COLOUR_DEPTH != 0
				if (dest >= end) {
					delete desc;
					return NULL;
				}
#endif
			} while (*dest != 0);

			dest++; // skip trailing zero
		}

		if(left<desc->x) {
			dbg->warning( "image_reader_t::read_node()","left(%i)<x(%i) (may be intended)", left, desc->x );
		}

		/// No need to check for valid dest pointer here, the code has the same structure as above
		dest = desc->data;
		for( uint8 y=0;  y<desc->h;  y++  ) {
			*dest -= left;
			// skip rest of the line
			do {
				dest++;
				dest += *dest + 1;
			} while (*dest != 0);
			dest++; // skip trailing zero
		}
	}

	if (desc->len != 0) {
		// get the adler hash (since we have zlib on board anyway ... )
		bool do_register_image = true;
		uint32 adler = adler32(0L, NULL, 0 );
		// remember len is in source pixel units
		for(  uint i = 0;  i < desc->len;  i++  ) {
#ifdef SIM_ENABLE_RGB32_OUTPUT
			if(  desc->truecolor  ) {
				const PIXVAL data = desc->truecolor_data[i];
				adler = adler32(adler, (const Bytef *)&data, sizeof(data));
			}
			else {
#endif
				const image_pixel_t data = desc->data[i];
				adler = adler32(adler, (const Bytef *)&data, sizeof(data));
#ifdef SIM_ENABLE_RGB32_OUTPUT
			}
#endif
		}
		static inthashtable_tpl<uint32, image_t *> images_adlers;
		image_t *same = images_adlers.get(adler);
		if (same) {
			// same checksum => if same then skip!
			image_t const& a = *desc;
			image_t const& b = *same;
#ifdef SIM_ENABLE_RGB32_OUTPUT
			const bool same_storage = a.truecolor == b.truecolor;
			const bool same_data = same_storage && a.len == b.len && (a.truecolor
				? memcmp(a.truecolor_data, b.truecolor_data, sizeof(*a.truecolor_data) * a.len) == 0
				: memcmp(a.data, b.data, sizeof(*a.data) * a.len) == 0);
#else
			const bool same_data = a.len == b.len && memcmp(a.data, b.data, sizeof(*a.data) * a.len) == 0;
#endif
			do_register_image =
				a.x        != b.x        ||
				a.y        != b.y        ||
				a.w        != b.w        ||
				a.h        != b.h        ||
				a.zoomable != b.zoomable ||
				a.len      != b.len      ||
				!same_data;
		}
		// unique image here
		if(  do_register_image  ) {
			if(!same) {
				images_adlers.put(adler,desc); // still with imageid == IMG_EMPTY!
			}
			// register image adds this image to the internal array maintained by simgraph??.cc
			register_image(desc);
		}
		else {
			// no need to load doubles ...
			delete desc;
			desc = same;
		}
	}

	return desc;
}

bool image_reader_t::image_has_valid_data(image_t *image_in) const
{
#ifdef SIM_ENABLE_RGB32_OUTPUT
	if(  image_in->truecolor  ) {
		PIXVAL *src = image_in->truecolor_data;
		PIXVAL *end = image_in->truecolor_data + image_in->len;

		for( int y = 0;  y < image_in->h;  ++y  ) {
			uint16 runlen = *src++;
			do {
				if (src >= end) {
					return false;
				}

				runlen = *src++ & ~TRANSPARENT_RUN;
				src += runlen;

				if (src >= end) {
					return false;
				}

				runlen = *src++;
			} while(  runlen!=0  );
		}

		return src == end;
	}
#endif
	image_pixel_t *src = image_in->data;
	image_pixel_t *end = image_in->data + image_in->len;

	for( int y = 0;  y < image_in->h;  ++y  ) {
		// decode line
		uint16 runlen = *src++;
		do {
			if (src >= end) {
				return false;
			}

			runlen = *src++ & ~TRANSPARENT_RUN;
			src += runlen;

			if (src >= end) {
				return false;
			}

			runlen = *src++;
		} while(  runlen!=0  ); // end of row: runlen == 0
	}

	return src == end;
}
