/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "dataobj/height_map_exporter.h"
#include "dataobj/height_map_loader.h"
#include "dataobj/koord.h"
#include "io/raw_image.h"
#include "io/classify_file.h"
#include "simdebug.h"
#include "pathes.h"
#include "sys/simsys.h"

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <png.h>
#include <string>
#include <sys/stat.h>
#include <vector>
#ifdef _WIN32
#include <windows.h>
void dr_fatal_notify(const char *) { std::abort(); }
#endif

// The world adapter supplies data only: no encoding or export behavior lives here.
struct test_settings_t
{
	sint8 minimum, maximum;
	sint8 get_minimumheight() const { return minimum; }
	sint8 get_maximumheight() const { return maximum; }
};

class karte_t
{
public:
	koord size;
	test_settings_t settings;
	std::vector<sint8> heights;
	karte_t(sint16 width, sint16 height, sint8 minimum = -10, sint8 maximum = 20) :
		size(width, height), settings{minimum, maximum}, heights((width + 1) * (height + 1), 0) {}
	koord get_size() const { return size; }
	const test_settings_t &get_settings() const { return settings; }
	sint8 get_groundwater() const { return -2; }
	sint8 lookup_hgt(sint16 x, sint16 y) const { return heights.at(x + y * (size.x + 1)); }
	void set(sint16 x, sint16 y, sint8 height) { heights.at(x + y * (size.x + 1)) = height; }
};

// Stubs are restricted to logging. Real allocation, PNG codecs and loader link in.
log_t::log_t(const char *, bool, bool, bool, const char *, const char *) {}
void log_t::error(const char *, const char *, ...) {}
void log_t::warning(const char *, const char *, ...) {}
void log_t::fatal(const char *, const char *, ...) { std::abort(); }
log_t *dbg = new log_t(NULL, false, false, false, NULL);
sint8 env_t::pak_height_conversion_factor = 1;

static bool fail_next_png_close = false;
static int fail_after_png_rows = -1;
static unsigned successful_png_rows = 0;
static bool png_row_failure_injected = false;

void heightmap_test_png_write_row(png_structp png, png_const_bytep row)
{
	if(fail_after_png_rows >= 0 && successful_png_rows == unsigned(fail_after_png_rows)) {
		png_row_failure_injected = true;
		png_error(png, "Injected mid-write heightmap failure");
	}
	png_write_row(png, row);
	if(fail_after_png_rows >= 0) {
		++successful_png_rows;
	}
}

int heightmap_test_fclose(FILE *file)
{
	const int result = std::fclose(file);
	if(fail_next_png_close) {
		fail_next_png_close = false;
		errno = ENOSPC;
		return EOF;
	}
	return result;
}

#include "filesystem.inc"
static bool fail_rename = false;
int dr_rename(const char *source, const char *destination)
{
	if(fail_rename) {
		errno = EACCES;
		return -1;
	}
	return production_dr_rename(source, destination);
}
#include "exporter.inc"

// Only the GUI text sink is adapted; set_filename and string helpers are real.
class savegame_frame_t
{
public:
	char ibuf[128];
	struct input_t {
		std::string value;
		void set_text(char *text, size_t size) { assert(size == 128); value = text; }
	} input;
	savegame_frame_t() { std::memset(ibuf, 0, sizeof(ibuf)); }
	void set_filename(const char *filename);
};
#include "filename_setter.inc"

static void test_filename_setter()
{
	savegame_frame_t frame;
	const char *names[] = {"a", "ab", "abc", "heightmap"};
	for(const char *name : names) {
		frame.set_filename((std::string(name) + ".png").c_str());
		assert(frame.input.value == name);
	}
	frame.set_filename("save/a.sve");
	assert(frame.input.value == "a");
	for(const char *prefix : {"", "save/"}) {
		const std::string accepted(127, 'a');
		frame.set_filename((std::string(prefix) + accepted + ".png").c_str());
		assert(frame.input.value == accepted);
		const std::string rejected(128, 'b');
		frame.set_filename((std::string(prefix) + rejected + ".png").c_str());
		assert(frame.input.value == accepted);
	}
	std::cout << "Real save-dialog filename setter: short PNG names, save prefix and 127/128 boundary: PASS\n";
}

static bool exists(const char *path)
{
	struct stat status;
	return dr_stat(path, &status) == 0;
}

static std::vector<sint16> load_heights(const char *path, sint8 minimum, sint8 maximum, sint16 width, sint16 height)
{
	height_map_loader_t loader(minimum, maximum, env_t::HEIGHT_CONV_LINEAR);
	sint8 *field = NULL;
	sint16 actual_width = 0, actual_height = 0;
	assert(loader.get_height_data_from_file(path, -2, field, actual_width, actual_height, false));
	assert(actual_width == width && actual_height == height);
	std::vector<sint16> result(width * height);
	for(size_t i = 0; i < result.size(); ++i) {
		// World initialization adds one after the loader; slope correction is not simulated.
		result[i] = sint16(field[i]) + 1;
	}
	free(field);
	return result;
}

static void test_encoding()
{
	raw_image_t ramp(256, 1, raw_image_t::FMT_GRAY8);
	for(int pixel = 0; pixel <= 255; ++pixel) {
		*ramp.access_pixel(pixel, 0) = uint8(pixel);
	}
	assert(ramp.write_png("ramp.png"));
	const sint16 limits[][2] = {{-10, 20}, {-127, 127}, {-128, 127}, {-3, 1}, {0, 1}, {120, 127}};
	for(const auto &limit : limits) {
		// Oracle is the real loader decoding each possible gray pixel, not a copied formula.
		const auto decoded = load_heights("ramp.png", limit[0], limit[1], 256, 1);
		for(sint16 height = -129; height <= 129; ++height) {
			int best = 0;
			for(int pixel = 1; pixel <= 255; ++pixel) {
				if(std::abs(int(decoded[pixel]) - height) < std::abs(int(decoded[best]) - height)) {
					best = pixel;
				}
			}
			assert(height_map_exporter_t::encode_height(height, limit[0], limit[1]) == best);
		}
	}
	assert(height_map_exporter_t::encode_height(0, 10, 10) == 0);
	assert(height_map_exporter_t::encode_height(0, 10, -10) == 0);
	std::cout << "Encoding boundaries, negative heights and nearest gray pixel via real loader: PASS\n";
}

static void test_png_and_report()
{
	karte_t world(3, 2);
	const sint8 values[6] = {-10, -9, -2, 0, 10, 20};
	for(int y = 0; y < 2; ++y) {
		for(int x = 0; x < 3; ++x) {
			world.set(x, y, values[x + y * 3]);
		}
		world.set(3, y, world.lookup_hgt(2, y));
	}
	for(int x = 0; x <= 3; ++x) {
		world.set(x, 2, world.lookup_hgt(std::min(x, 2), 1));
	}
	const auto before = world.heights;
	height_map_exporter_t::report_t report;
	assert(height_map_exporter_t::write_png(world, u8"日本語-地形.png", report));
	assert(world.heights == before);
	assert(report.minimum_height == -10 && report.maximum_height == 20 && report.groundwater == -2);
	assert(report.changed_heights == 1 && report.maximum_error == 1 && report.changed_border_heights == 0);
	raw_image_t image;
	assert(image.read_from_file(u8"日本語-地形.png"));
	assert(image.get_width() == 3 && image.get_height() == 2);
	assert(image.get_format() == raw_image_t::FMT_GRAY8 && image.get_bpp() == 8);
	const auto decoded = load_heights(u8"日本語-地形.png", -10, 20, 3, 2);
	for(size_t i = 0; i < decoded.size(); ++i) {
		assert(decoded[i] == (i == 0 ? -9 : values[i]));
	}
	// Every omitted right/bottom vertex differs: 2 + 4 = 6, corner counted once.
	for(int y = 0; y < 2; ++y) { world.set(3, y, 5); }
	for(int x = 0; x <= 3; ++x) { world.set(x, 2, 5); }
	report = height_map_exporter_t::analyze(world);
	assert(report.changed_border_heights == 6);
	assert(report.changed_heights == 1 && report.maximum_error == 1);
	std::cout << "Non-square PNG, Unicode paths, loader +1, quantization and distinct outer vertices: PASS\n";
}

static void test_failure_cleanup()
{
	karte_t world(3, 2);
	height_map_exporter_t::report_t report;
	FILE *existing = dr_fopen("existing.png", "wb");
	assert(existing);
	assert(std::fputs("keep original", existing) >= 0);
	assert(std::fclose(existing) == 0);
	assert(!height_map_exporter_t::write_png(world, "existing.png", report));
	char bytes[32] = {};
	existing = dr_fopen("existing.png", "rb");
	assert(existing && std::fread(bytes, 1, sizeof(bytes), existing) == 13);
	assert(std::strcmp(bytes, "keep original") == 0);
	std::fclose(existing);
	assert(!height_map_exporter_t::write_png(world, "missing-directory/terrain.png", report));
	assert(!height_map_exporter_t::write_png(world, NULL, report));
	assert(!height_map_exporter_t::write_png(world, "", report));
	karte_t invalid(3, 2, 5, 5);
	assert(!height_map_exporter_t::write_png(invalid, "invalid.png", report));
	assert(!exists("invalid.png"));
	karte_t empty(0, 2);
	assert(!height_map_exporter_t::write_png(empty, "empty.png", report));
	assert(!exists("empty.png"));
	// A leftover temporary file belongs to another attempt and must be preserved.
	FILE *stale = dr_fopen("collision.png.heightmap-0.tmp", "wb");
	assert(stale && std::fputs("stale", stale) >= 0 && std::fclose(stale) == 0);
	assert(height_map_exporter_t::write_png(world, "collision.png", report));
	stale = dr_fopen("collision.png.heightmap-0.tmp", "rb");
	assert(stale && std::fread(bytes, 1, sizeof(bytes), stale) == 5);
	assert(std::memcmp(bytes, "stale", 5) == 0);
	std::fclose(stale);
	assert(dr_remove("collision.png.heightmap-0.tmp") == 0);
	// Fail publication after writing a real PNG; both ASCII and UTF-8 temps must go.
	fail_rename = true;
	for(const char *path : {"rename-failure.png", u8"日本語-失敗.png"}) {
		assert(!height_map_exporter_t::write_png(world, path, report));
		assert(!exists(path));
		assert(!exists((std::string(path) + ".heightmap-0.tmp").c_str()));
	}
	fail_rename = false;
	// Buffered writes can fail only at final fclose, after png_write_end succeeds.
	for(const char *path : {"close-failure.png", u8"日本語-終了失敗.png"}) {
		fail_next_png_close = true;
		assert(!height_map_exporter_t::write_png(world, path, report));
		assert(!fail_next_png_close);
		assert(!exists(path));
		assert(!exists((std::string(path) + ".heightmap-0.tmp").c_str()));
	}
	std::cout << "Existing destination preserved, invalid input, failed publication/final-close cleanup: PASS\n";
}

static void test_mid_write_failure()
{
	karte_t world(4, 5);
	for(int y = 0; y <= world.size.y; ++y) {
		for(int x = 0; x <= world.size.x; ++x) {
			world.set(x, y, sint8(x + y - 5));
		}
	}
	const auto before = world.heights;
	for(const char *path : {"mid-write-failure.png", u8"日本語-途中失敗.png"}) {
		height_map_exporter_t::report_t report;
		successful_png_rows = 0;
		png_row_failure_injected = false;
		fail_after_png_rows = 2;
		// Two real row writes succeed; the third calls libpng's actual longjmp.
		assert(!height_map_exporter_t::write_png(world, path, report));
		fail_after_png_rows = -1;
		assert(png_row_failure_injected && successful_png_rows == 2);
		assert(world.heights == before);
		assert(!exists(path));
		assert(!exists((std::string(path) + ".heightmap-0.tmp").c_str()));
	}
	std::cout << "Real libpng row-error recovery after two rows, ASCII/Unicode cleanup and terrain preserved: PASS\n";
}

int main()
{
	test_encoding();
	test_png_and_report();
	test_failure_cleanup();
	test_mid_write_failure();
	test_filename_setter();
}
