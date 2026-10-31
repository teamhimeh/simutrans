/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "save_heightmap_frame.h"
#include "messagebox.h"
#include "simwin.h"
#include "../dataobj/height_map_exporter.h"
#include "../dataobj/translator.h"
#include "../simworld.h"
#include "../sys/simsys.h"

#include <cerrno>
#include <sys/stat.h>


save_heightmap_frame_t::save_heightmap_frame_t() :
	savegame_frame_t(".png", false, "maps/", false), saved(false)
{
	label_enabled = false;
	set_name(translator::translate("Export heightmap"));
	savebutton.set_text("Save as PNG");
	set_filename("heightmap.png");

	// Reuse the file dialog, keeping import settings visible while naming the PNG.
	top_frame.new_component<gui_label_t>("Heightmap size:");
	top_frame.add_component(&size_label);
	top_frame.new_component<gui_label_t>("Height limits:");
	top_frame.add_component(&limits_label);
	top_frame.new_component<gui_label_t>("Water level");
	top_frame.add_component(&sea_label);
	top_frame.new_component_span<gui_label_t>("Lake levels, buildings and ways are not stored.", 2);
	update_settings();
	reset_min_windowsize();
	set_windowsize(get_min_windowsize());
}


void
save_heightmap_frame_t::update_settings()
{
	const koord size = welt->get_size();
	size_label.buf().printf("%i x %i", int(size.x), int(size.y));
	size_label.update();
	limits_label.buf().printf("%i .. %i", int(welt->get_settings().get_minimumheight()), int(welt->get_settings().get_maximumheight()));
	limits_label.update();
	sea_label.buf().printf("%i", int(welt->get_groundwater()));
	sea_label.update();
}


bool
save_heightmap_frame_t::save(const char *filename)
{
	saved = false;
	if(*get_input_filename() == '\0') {
		create_win(new news_img(translator::translate("Please enter a heightmap filename.")), w_info, magic_none);
		return false;
	}

	std::string path(filename);
	// The generic file dialog appends its suffix even when it was typed explicitly.
	if(path.size() >= 8  &&  path.compare(path.size() - 8, 8, ".png.png") == 0) {
		path.resize(path.size() - 4);
	}
	struct stat status;
	if(dr_stat(path.c_str(), &status) == 0  ||  errno != ENOENT) {
		create_win(new news_img(translator::translate("This file already exists or cannot be accessed. Choose another filename.")), w_info, magic_none);
		return false;
	}

	height_map_exporter_t::report_t report;
	saved = height_map_exporter_t::write_png(*welt, path.c_str(), report);
	update_settings();
	if(saved) {
		cbuffer_t message;
		message.printf(translator::translate("Heightmap saved to:\n%s"), path.c_str());
		message.printf("\n%s %i .. %i\n%s %i\n", translator::translate("Height limits:"), int(report.minimum_height), int(report.maximum_height), translator::translate("Water level"), int(report.groundwater));
		create_win(new news_img(message.get_str()), w_info, magic_none);
	}
	else {
		create_win(new news_img(translator::translate("Heightmap export failed. Check the filename and write permissions.")), w_info, magic_none);
	}
	return saved;
}


bool
save_heightmap_frame_t::ok_action(const char *filename)
{
	return save(filename);
}


bool
save_heightmap_frame_t::item_action(const char *filename)
{
	// Selecting an existing image only copies its name; it never overwrites it.
	set_filename(get_filename(filename).c_str());
	return false;
}
