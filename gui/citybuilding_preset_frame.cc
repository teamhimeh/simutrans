/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */
#include "citybuilding_preset_frame.h"
#include "citybuilding_edit.h"
#include "messagebox.h"
#include "../dataobj/environment.h"
#include "../dataobj/translator.h"
#include "../sys/simsys.h"
#include "../utils/cbuffer_t.h"
#include "../utils/searchfolder.h"
#include <set>
#include <sys/stat.h>
#include <ctype.h>

static void preset_message(const char *text)
{
	create_win(new news_img(translator::translate(text)), w_time_delete, magic_none);
}

static bool exists(const std::string &path)
{
	struct stat info;
	return dr_stat(path.c_str(), &info) == 0;
}

static void recovery_failure(const cbuffer_t &failures)
{
	cbuffer_t message;
	message.append(translator::translate("Could not recover building presets:"));
	message.append(failures.get_str());
	create_win(new news_img(message.get_str()), w_info, magic_none);
}

class citybuilding_preset_confirm_t : public gui_frame_t, private action_listener_t
{
	citybuilding_preset_frame_t *owner;
	std::string path;
	citybuilding_preset_t snapshot;
	cbuffer_t question;
	gui_label_t message;
	button_t confirm, cancel;
public:
	citybuilding_preset_confirm_t(citybuilding_preset_frame_t *owner, const std::string &path, const citybuilding_preset_t &snapshot) :
		gui_frame_t(translator::translate("Overwrite preset")), owner(owner), path(path), snapshot(snapshot)
	{
		set_table_layout(1, 0);
		question.printf(translator::translate("Overwrite preset \"%s\"?"), snapshot.name.c_str());
		message.set_text_pointer(question.get_str());
		add_component(&message);
		gui_aligned_container_t *buttons = add_table(2, 1);
		confirm.init(button_t::roundbox, "Overwrite");
		cancel.init(button_t::roundbox, "Cancel");
		confirm.add_listener(this);
		cancel.add_listener(this);
		buttons->add_component(&confirm);
		buttons->add_component(&cancel);
		end_table();
		buttons->set_focus(&cancel);
		set_focus(buttons);
		reset_min_windowsize();
		set_windowsize(get_min_windowsize());
	}
	bool action_triggered(gui_action_creator_t *comp, value_t) OVERRIDE
	{
		if (comp == &confirm && win_get_magic(magic_citybuilding_preset_file) == owner) {
			if (owner->write_confirmed(path, snapshot)) {
				destroy_win(owner);
			}
		}
		destroy_win(this);
		return true;
	}
	bool infowin_event(const event_t *event) OVERRIDE
	{
		if (event->ev_class == EVENT_KEYBOARD && event->ev_code == 27) {
			destroy_win(this);
			return true;
		}
		return gui_frame_t::infowin_event(event);
	}
};

citybuilding_preset_frame_t::citybuilding_preset_frame_t(citybuilding_edit_frame_t *owner, bool load, const citybuilding_preset_t &snapshot) :
	savegame_frame_t(".tab", false, NULL, false), owner(owner), do_load(load), preset(snapshot),
	directory(env_t::pak_dir + "building_preset/")
{
	add_path(directory.c_str());
	label_enabled = false;
	set_name(translator::translate(load ? "Load preset" : "Save preset"));
	savebutton.set_text(load ? "Load preset" : "Save preset");
}

citybuilding_preset_frame_t::~citybuilding_preset_frame_t()
{
	destroy_win(magic_citybuilding_preset_confirm);
}

bool citybuilding_preset_frame_t::recover(const std::string &path, cbuffer_t &failures)
{
	std::string archived;
	const citybuilding_preset_recovery_t result = citybuilding_preset_recover(path, &archived);
	if (!archived.empty()) {
		cbuffer_t message;
		message.printf(translator::translate("Corrupt building preset preserved at:\n%s"), archived.c_str());
		create_win(new news_img(message.get_str()), w_info, magic_none);
	}
	if (result == CITYBUILDING_PRESET_RECOVERY_FAILED) {
		failures.printf("\n%s", get_filename(path.c_str()));
		return false;
	}
	return true;
}

void citybuilding_preset_frame_t::fill_list()
{
	searchfolder_t files;
	files.search(directory, "", false, false);
	std::set<std::string> targets;
	FOR(searchfolder_t, const &name, files) {
		std::string filename(name);
		if (filename.size() <= 8 || (filename.substr(filename.size() - 8) != ".tab.tmp" && filename.substr(filename.size() - 8) != ".tab.bak")) continue;
		struct stat info;
		if (dr_stat((directory + filename).c_str(), &info) == 0 && (info.st_mode & S_IFMT) == S_IFREG) {
			targets.insert(directory + filename.substr(0, filename.size() - 4));
		}
	}
	cbuffer_t failures;
	for (std::set<std::string>::const_iterator i = targets.begin(); i != targets.end(); ++i) recover(*i, failures);
	if (failures.len()) {
		recovery_failure(failures);
	}
	savegame_frame_t::fill_list();
}

bool citybuilding_preset_frame_t::infowin_event(const event_t *event)
{
	if (event->ev_class == EVENT_KEYBOARD && event->ev_code == 27) {
		destroy_win(this);
		return true;
	}
	return savegame_frame_t::infowin_event(event);
}

bool citybuilding_preset_frame_t::check_file(const char *path, const char *)
{
	const std::string name = get_filename(path);
	struct stat info;
	return name.size() > 4 && name.substr(name.size() - 4) == ".tab" &&
		dr_stat(path, &info) == 0 && (info.st_mode & S_IFMT) == S_IFREG;
}

bool citybuilding_preset_frame_t::ok_action(const char *)
{
	if (request(get_input_filename())) destroy_win(this);
	return false;
}

bool citybuilding_preset_frame_t::item_action(const char *path)
{
	return request(get_filename(path));
}

bool citybuilding_preset_frame_t::request(const std::string &filename)
{
	if (win_get_magic(magic_edit_house) != owner) return false;
	destroy_win(magic_citybuilding_preset_confirm);
	std::string name = filename;
	const size_t begin = name.find_first_not_of(" \t");
	if (begin == std::string::npos) { preset_message("Preset name is required."); return false; }
	name = name.substr(begin, name.find_last_not_of(" \t") - begin + 1);
	if (name.size() >= 4) {
		std::string ext = name.substr(name.size() - 4);
		for (size_t i = 0; i < ext.size(); ++i) ext[i] = (char)tolower((unsigned char)ext[i]);
		if (ext == ".tab") name.resize(name.size() - 4);
	}
	bool invalid = name.empty() || name.size() > 127 || name[name.size() - 1] == '.' || name[name.size() - 1] == ' ';
	for (size_t i = 0; i < name.size(); ++i) {
		unsigned char c = name[i];
		if (c < 32 || c == 127 || strchr("<>:\"/\\|?*", c)) invalid = true;
	}
	std::string base = name.substr(0, name.find('.'));
	while (!base.empty() && base[base.size() - 1] == ' ') base.resize(base.size() - 1);
	for (size_t i = 0; i < base.size(); ++i) base[i] = (char)toupper((unsigned char)base[i]);
	if (base == "CON" || base == "PRN" || base == "AUX" || base == "NUL" || base == "CONIN$" || base == "CONOUT$" ||
		(base.size() == 4 && (base.substr(0, 3) == "COM" || base.substr(0, 3) == "LPT") && base[3] >= '1' && base[3] <= '9')) invalid = true;
	if (base.size() == 5 && (base.substr(0, 3) == "COM" || base.substr(0, 3) == "LPT") &&
		(base.substr(3) == "\xC2\xB9" || base.substr(3) == "\xC2\xB2" || base.substr(3) == "\xC2\xB3")) invalid = true;
	if (invalid) { preset_message("Invalid building preset filename."); return false; }
	const std::string path = directory + name + ".tab";
	cbuffer_t failures;
	if (!recover(path, failures)) { recovery_failure(failures); return false; }
	if (do_load) {
		citybuilding_preset_t loaded;
		if (!citybuilding_preset_load(path, loaded)) { preset_message("Could not load building preset."); return false; }
		owner->load_preset(loaded);
		return true;
	}
	preset.name = name;
	if (exists(path)) {
		create_win(new citybuilding_preset_confirm_t(this, path, preset), w_info, magic_citybuilding_preset_confirm);
		return false;
	}
	if (!citybuilding_preset_save(path, preset)) { preset_message("Could not save building presets."); return false; }
	preset_message("Building preset saved.");
	return true;
}

bool citybuilding_preset_frame_t::write_confirmed(const std::string &path, const citybuilding_preset_t &snapshot)
{
	if (win_get_magic(magic_edit_house) != owner) return false;
	cbuffer_t failures;
	if (!recover(path, failures)) { recovery_failure(failures); return false; }
	if (!exists(path)) return false;
	if (!citybuilding_preset_save(path, snapshot)) { preset_message("Could not save building presets."); return false; }
	preset_message("Building preset saved.");
	return true;
}
