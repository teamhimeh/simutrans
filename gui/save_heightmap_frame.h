/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef GUI_SAVE_HEIGHTMAP_FRAME_H
#define GUI_SAVE_HEIGHTMAP_FRAME_H

#include "savegame_frame.h"

/** Saves terrain as a PNG for the linear heightmap loader. */
class save_heightmap_frame_t : public savegame_frame_t
{
private:
	bool saved;
	gui_label_buf_t size_label, limits_label, sea_label;

	void update_settings();
	bool save(const char *filename);

protected:
	bool close_after_ok() const OVERRIDE { return saved; }
	bool ok_action(const char *filename) OVERRIDE;
	bool item_action(const char *filename) OVERRIDE;
	const char *get_info(const char *) OVERRIDE { return ""; }

public:
	save_heightmap_frame_t();
};

#endif
