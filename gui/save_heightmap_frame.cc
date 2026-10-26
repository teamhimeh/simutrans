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
	savebutton.set_text("Save PNG");
	set_filename("heightmap.png");

	// Reuse the file dialog, keeping import settings visible while naming the PNG.
	top_frame.new_component<gui_label_t>("Load mode:");
	top_frame.new_component<gui_label_t>("linear");
	top_frame.new_component<gui_label_t>("Heightmap size:");
	top_frame.add_component(&size_label);
	top_frame.new_component<gui_label_t>("Height limits:");
	top_frame.add_component(&limits_label);
	top_frame.new_component<gui_label_t>("Water level");
	top_frame.add_component(&sea_label);
	top_frame.new_component<gui_label_t>("Changed heights / max. error:");
	top_frame.add_component(&changes_label);
	top_frame.new_component<gui_label_t>("Changed outer vertices:");
	top_frame.add_component(&border_label);
	top_frame.new_component_span<gui_label_t>("Right and bottom borders are duplicated on import.", 2);
	top_frame.new_component_span<gui_label_t>("The top-left vertex may change on import.", 2);
	top_frame.new_component_span<gui_label_t>("Slope correction may change the imported terrain.", 2);
	top_frame.new_component_span<gui_label_t>("Lake levels, buildings and ways are not stored.", 2);
	top_frame.new_component_span<gui_label_t>("Use a new filename; existing files are kept.", 2);
	update_report();
	reset_min_windowsize();
	set_windowsize(get_min_windowsize());
}


void
save_heightmap_frame_t::update_report()
{
	const height_map_exporter_t::report_t report = height_map_exporter_t::analyze(*welt);
	const koord size = welt->get_size();
	size_label.buf().printf("%i x %i", int(size.x), int(size.y));
	size_label.update();
	limits_label.buf().printf("%i .. %i", int(report.minimum_height), int(report.maximum_height));
	limits_label.update();
	sea_label.buf().printf("%i", int(report.groundwater));
	sea_label.update();
	changes_label.buf().printf("%u / %i", unsigned(report.changed_heights), int(report.maximum_error));
	changes_label.update();
	border_label.buf().printf("%u", unsigned(report.changed_border_heights));
	border_label.update();
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
	update_report();
	if(saved) {
		cbuffer_t message;
		message.printf(translator::translate("Heightmap saved to:\n%s"), path.c_str());
		message.printf("\n%s %i .. %i\n%s %i\n", translator::translate("Height limits:"), int(report.minimum_height), int(report.maximum_height), translator::translate("Water level"), int(report.groundwater));
		message.printf("%s %s", translator::translate("Load mode:"), translator::translate("linear"));
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
