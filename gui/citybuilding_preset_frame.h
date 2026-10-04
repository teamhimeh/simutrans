/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */
#ifndef GUI_CITYBUILDING_PRESET_FRAME_H
#define GUI_CITYBUILDING_PRESET_FRAME_H

#include "savegame_frame.h"
#include "../dataobj/citybuilding_preset.h"

class citybuilding_edit_frame_t;

class citybuilding_preset_frame_t : public savegame_frame_t
{
	citybuilding_edit_frame_t *owner;
	bool do_load;
	citybuilding_preset_t preset;
	std::string directory;
	bool request(const std::string &filename);
protected:
	bool close_after_ok() const OVERRIDE { return false; }
	bool ok_action(const char *) OVERRIDE;
	bool item_action(const char *path) OVERRIDE;
	const char *get_info(const char *) OVERRIDE { return ""; }
	bool check_file(const char *path, const char *) OVERRIDE;
public:
	citybuilding_preset_frame_t(citybuilding_edit_frame_t *, bool, const citybuilding_preset_t &);
	~citybuilding_preset_frame_t();
	bool infowin_event(const event_t *) OVERRIDE;
	bool write_confirmed(const std::string &path, const citybuilding_preset_t &snapshot);
};
#endif
