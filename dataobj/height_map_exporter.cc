/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "height_map_exporter.h"

#include "../simworld.h"
#include "../io/raw_image.h"
#include "../sys/simsys.h"

#include <cerrno>
#include <cstdio>
#include <string>
#include <sys/stat.h>


namespace {

// For gray pixels, h0/0x5FA in height_map_loader_t reduces to pixel/255.
sint16 decode_height(uint8 pixel, sint8 minimum_height, sint8 maximum_height)
{
	return minimum_height + (sint32(pixel) * (maximum_height - minimum_height)) / 255 + 1;
}


struct height_lookup_t
{
	uint8 pixels[256];
	sint16 decoded[256];

	height_lookup_t(sint8 minimum_height, sint8 maximum_height)
	{
		for(sint16 height = -128; height <= 127; ++height) {
			const uint8 pixel = height_map_exporter_t::encode_height(height, minimum_height, maximum_height);
			pixels[height + 128] = pixel;
			decoded[height + 128] = decode_height(pixel, minimum_height, maximum_height);
		}
	}
};


bool path_is_missing(const char *filename)
{
	struct stat status;
	return dr_stat(filename, &status) != 0  &&  errno == ENOENT;
}

}


height_map_exporter_t::report_t::report_t() :
	minimum_height(0), maximum_height(0), groundwater(0),
	changed_heights(0), changed_border_heights(0), maximum_error(0)
{
}


uint8
height_map_exporter_t::encode_height(sint16 height, sint8 minimum_height, sint8 maximum_height)
{
	if(minimum_height >= maximum_height) {
		return 0;
	}

	uint8 best_pixel = 0;
	sint32 best_error = 65536;
	for(sint16 pixel = 0; pixel <= 255; ++pixel) {
		const sint32 difference = sint32(height) - decode_height(uint8(pixel), minimum_height, maximum_height);
		const sint32 error = difference < 0 ? -difference : difference;
		if(error < best_error) {
			best_error = error;
			best_pixel = uint8(pixel);
		}
	}
	return best_pixel;
}


height_map_exporter_t::report_t
height_map_exporter_t::analyze(const karte_t &world)
{
	report_t report;
	report.minimum_height = world.get_settings().get_minimumheight();
	report.maximum_height = world.get_settings().get_maximumheight();
	report.groundwater = world.get_groundwater();
	const koord size = world.get_size();
	if(report.minimum_height >= report.maximum_height  ||  size.x <= 0  ||  size.y <= 0) {
		return report;
	}

	const height_lookup_t lookup(report.minimum_height, report.maximum_height);
	for(sint16 y = 0; y < size.y; ++y) {
		for(sint16 x = 0; x < size.x; ++x) {
			const sint16 height = world.lookup_hgt(x, y);
			const sint16 difference = height - lookup.decoded[height + 128];
			const sint16 error = difference < 0 ? -difference : difference;
			if(error != 0) {
				++report.changed_heights;
			}
			if(error > report.maximum_error) {
				report.maximum_error = error;
			}
		}
		if(world.lookup_hgt(size.x, y) != world.lookup_hgt(size.x - 1, y)) {
			++report.changed_border_heights;
		}
	}
	for(sint32 x = 0; x <= size.x; ++x) {
		const sint16 source_x = x == size.x ? size.x - 1 : x;
		if(world.lookup_hgt(x, size.y) != world.lookup_hgt(source_x, size.y - 1)) {
			++report.changed_border_heights;
		}
	}
	return report;
}


bool
height_map_exporter_t::write_png(const karte_t &world, const char *filename, report_t &report)
{
	report = analyze(world);
	const koord size = world.get_size();
	if(filename == NULL  ||  *filename == '\0'  ||  size.x <= 0  ||  size.y <= 0  ||
		report.minimum_height >= report.maximum_height  ||  !path_is_missing(filename)) {
		return false;
	}

	// Keep failed output away from the requested name, and do not reuse old temps.
	std::string temporary;
	for(uint32 i = 0; i < 1000; ++i) {
		char suffix[40];
		std::sprintf(suffix, ".heightmap-%u.tmp", unsigned(i));
		temporary = std::string(filename) + suffix;
		if(path_is_missing(temporary.c_str())) {
			break;
		}
		temporary.clear();
	}
	if(temporary.empty()) {
		return false;
	}

	bool success = false;
	{
		const height_lookup_t lookup(report.minimum_height, report.maximum_height);
		raw_image_t row(size.x, 1, raw_image_t::FMT_GRAY8);
		raw_image_png_writer_t writer(temporary.c_str(), size.x, size.y, raw_image_t::FMT_GRAY8);
		success = writer.is_valid();
		for(sint16 y = 0; success  &&  y < size.y; ++y) {
			uint8 *pixels = row.access_pixel(0, 0);
			for(sint16 x = 0; x < size.x; ++x) {
				pixels[x] = lookup.pixels[sint16(world.lookup_hgt(x, y)) + 128];
			}
			success = writer.write_rows(row, 1);
		}
		if(success) {
			success = writer.finish();
		}
	} // Close the writer before rename or UTF-8-aware cleanup.

	if(success) {
		// dr_rename replaces files; recheck immediately before publishing the PNG.
		success = path_is_missing(filename)  &&  dr_rename(temporary.c_str(), filename) == 0;
	}
	if(!success) {
		dr_remove(temporary.c_str());
	}
	return success;
}
