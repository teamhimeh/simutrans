/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef DESCRIPTOR_IMAGE_H
#define DESCRIPTOR_IMAGE_H


#include "../display/simgraph.h"
#include "../display/simimg.h"
#include "obj_desc.h"


// number of special colors
#define SPECIAL (31)

#define SPECIAL_TRANSPARENT (0x00E7FFFF)

#ifdef SIM_ENABLE_RGB32_OUTPUT
#define IMAGE_TRUECOLOR_FLAG (0x01000000u)
#define IMAGE_TRUECOLOR_MASK (0x00FFFFFFu)
#endif

typedef uint16 image_pixel_t;



/**
 * Data of one image
 *
 * Child nodes:
 *  (none)
 */
class image_t : public obj_desc_t
{
public:
	static const uint32 rgbtab[SPECIAL];

	size_t len;       ///< length of data[] in PIXVAL units
	scr_coord_val x;  ///< x offset of data[] image
	scr_coord_val y;  ///< y offset of data[] image
	scr_coord_val w;  ///< width of data[] image
	scr_coord_val h;  ///< height of data[] image
	image_id imageid; ///< set by register_image()
	uint8 zoomable;   ///< some images may not be zoomed i.e. icons
#ifdef SIM_ENABLE_RGB32_OUTPUT
	bool truecolor;   ///< image data stores RGB888 pixels rather than RGB555 indices
	PIXVAL *truecolor_data; ///< RLE encoded RGB888/ARGB8888 image data
#endif
	image_pixel_t *data;     ///< RLE encoded 16-bit pak image data

	image_t(size_t len_=0) : len(0), imageid(IMG_EMPTY), zoomable(0),
#ifdef SIM_ENABLE_RGB32_OUTPUT
		truecolor(false), truecolor_data(NULL),
#endif
		data(NULL)
	{
		if (len_) {
			alloc(len_);
		}
	}

	~image_t()
	{
		delete [] data;
#ifdef SIM_ENABLE_RGB32_OUTPUT
		delete [] truecolor_data;
#endif
	}

	void alloc(size_t len_)
	{
		delete [] data;
		data = new image_pixel_t[len_];
#ifdef SIM_ENABLE_RGB32_OUTPUT
		delete [] truecolor_data;
		truecolor_data = NULL;
		truecolor = false;
#endif
		len = len_;
	}

#ifdef SIM_ENABLE_RGB32_OUTPUT
	void alloc_truecolor(size_t len_)
	{
		delete [] data;
		data = NULL;
		delete [] truecolor_data;
		truecolor_data = new PIXVAL[len_];
		truecolor = true;
		len = len_;
	}
#endif

	static image_t* copy_image(const image_t& other);

	const image_t* get_pic() const { return this; }

	image_pixel_t const* get_data() const { return data; }
	image_pixel_t*       get_data()       { return data; }

	image_id get_id() const { return imageid; }

	/* rotate_image_data - produces a (rotated) image
	 * only rotates by 90 degrees or multiples thereof, and assumes a square image
	 * Otherwise it will only succeed for angle=0;
	 */
	image_t* copy_rotate(const sint16 angle) const;

	image_t* copy_flipvertical() const;
	image_t* copy_fliphorizontal() const;

	static image_t* create_single_pixel();

	void register_image() { ::register_image(this); }

private:
	friend class image_reader_t;
};

#endif
