/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef GUI_CITYBUILDING_EDIT_H
#define GUI_CITYBUILDING_EDIT_H


#include "extend_edit.h"
#include "simwin.h"

#include "components/gui_building.h"
#include "../dataobj/citybuilding_preset.h"
#include "../utils/cbuffer_t.h"

class building_desc_t;
class tool_build_house_t;
class citybuilding_preset_confirm_t;


/*
 * The citybuilding editor (urban buildings builder)
 */
class citybuilding_edit_frame_t : public extend_edit_gui_t
{
	friend class citybuilding_preset_confirm_t;

private:
	static tool_build_house_t* haus_tool;
	static cbuffer_t param_str;

	const building_desc_t *desc;

	vector_tpl<const building_desc_t *>building_list;

	button_t bt_res;
	button_t bt_com;
	button_t bt_ind;

	gui_label_t lb_name_filter_input;
	static char name_filter_value[64];
	gui_textinput_t name_filter_input;
	gui_combobox_t cb_preset;
	gui_textinput_t preset_name_input;
	button_t bt_preset_load, bt_preset_save, bt_preset_delete;
	char preset_name_value[128];
	vector_tpl<citybuilding_preset_t> presets;

	void fill_list() OVERRIDE;
	void put_item_in_list( const building_desc_t* desc );

	void change_item_info( sint32 i ) OVERRIDE;
	void refresh_preset_list(sint32 selection = -1);
	void load_preset();
	void save_preset();
	void delete_preset();
	void commit_preset_save(const citybuilding_preset_t &preset, bool require_existing);
	void commit_preset_delete(const std::string &name);

public:
	citybuilding_edit_frame_t(player_t* player);

	static bool sortreverse;

	/**
	* in top-level windows the name is displayed in titlebar
	* @return the non-translated component name
	*/
	const char* get_name() const { return "citybuilding builder"; }

	/**
	* Set the window associated helptext
	* @return the filename for the helptext, or NULL
	*/
	const char* get_help_filename() const OVERRIDE { return "citybuilding_build.txt"; }

	bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE;

	uint32 get_rdwr_id() OVERRIDE { return magic_citybuilding_edit; }

	void rdwr( loadsave_t *file ) OVERRIDE;
};

#endif
