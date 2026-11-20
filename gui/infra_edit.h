/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef GUI_INFRA_EDIT_H
#define GUI_INFRA_EDIT_H


#include "extend_edit.h"
#include "components/gui_label.h"
#include "components/gui_textinput.h"
#include "components/gui_numberinput.h"
#include "simwin.h"

#include "../tpl/vector_tpl.h"
#include "../simtypes.h"
#include "../dataobj/ribi.h"

class obj_desc_timelined_t;
class tool_t;

/// ribi names as used in the dat files of ways and way objects (image[ns] ...)
extern const char* const infra_ribi_codes[26];
/// ribis with extended switch images (in the order of the image list)
extern const ribi_t::ribi infra_switch_ribis[5];
/// slopes in the order of imageup[3], imageup[6], imageup[9], imageup[12]
extern const slope_t::type infra_up_slopes[4];
extern const int infra_up_slope_numbers[4];
/// diagonal ribis in the order of the image list
extern const ribi_t::ribi infra_diagonal_ribis[4];


/**
 * Draws a back and a front image on top of each other (e.g. a way tile).
 * The component is as large as the bounding box of both images.
 */
class gui_infra_image_t : public gui_component_t
{
	image_id back, front;
	sint8 player_nr;
	scr_coord tl;  ///< top left corner of the bounding box (image offsets)
	scr_size box;
public:
	gui_infra_image_t();

	void set_images(image_id back, image_id front, sint8 player_nr);

	scr_size get_min_size() const OVERRIDE { return box; }
	scr_size get_max_size() const OVERRIDE { return box; }

	void draw(scr_coord offset) OVERRIDE;
};


/**
 * One entry of the image selection combobox.
 * The text is the key used in the dat file of the object (e.g. image[ns]).
 */
class gui_image_choice_item_t : public gui_scrolled_list_t::const_text_scrollitem_t
{
	char text[64];
public:
	image_id back, front;
	sint32 value;  ///< free value, e.g. layout of buildings

	gui_image_choice_item_t(const char *name, image_id back, image_id front, sint32 value = -1);

	char const* get_text() const OVERRIDE { return text; }
};


/**
 * One selectable image of an object.
 */
struct infra_image_choice_t
{
	char name[64];  ///< key used in the dat file (e.g. image[ns]), or Icon/Cursor
	image_id back, front;
	sint32 value;   ///< free value, e.g. layout of buildings (-1 = none)
};


/**
 * Thumbnail in the list, showing a back and a front image fitted into the cell.
 */
class infra_thumbnail_t : public gui_scrolled_list_t::scrollitem_t
{
	image_id back, front;
	const char *name;
	scr_size cell_size;
public:
	infra_thumbnail_t(image_id back_, image_id front_, const char *name_, scr_size cell_size_) : back(back_), front(front_), name(name_), cell_size(cell_size_) {}
	const char *get_text() const OVERRIDE { return name; }
	scr_size get_min_size() const OVERRIDE { return cell_size; }
	scr_size get_max_size() const OVERRIDE { return cell_size; }
	void draw(scr_coord offset) OVERRIDE;
};


/**
 * Entry of the list of selectable objects.
 */
struct infra_entry_t
{
	const obj_desc_timelined_t *desc;
	const char *name;      ///< object name (not translated)
	const char *copyright;
	tool_t *tool;          ///< tool to build this object
	image_id icon;
	image_id cursor;
	sint64 price;          ///< in 1/100 credits
	sint64 maintenance;    ///< in 1/100 credits per month (not yet scaled with month length)
	sint32 speed;          ///< km/h (or minimum speed for roadsigns)
	sint32 level;
	uint16 intro_date, retire_date;
	PIXVAL color;          ///< text color in the list
	uint16 kind;           ///< type of the entry, free use by sub classes (used for sorting by type)

	infra_entry_t() : desc(NULL), name(""), copyright(NULL), tool(NULL), icon(IMG_EMPTY), cursor(IMG_EMPTY), price(0), maintenance(0), speed(0), level(0),
		intro_date(0), retire_date(0), color(0), kind(0) {}
};


/**
 * Base class for the dialogs to select ways, way objects, stops and signs.
 * The selected object will be built with the same tool as from the toolbar.
 */
class infra_edit_frame_t : public extend_edit_gui_t
{
protected:
	/// waytype of the dialog (from the tool parameter), ignore_wt = all
	waytype_t waytype;
	/// waytype to show; selectable by cb_waytype if the dialog is for all waytypes
	waytype_t filter_waytype;
	gui_combobox_t cb_waytype;

	char title[128];

	vector_tpl<infra_entry_t> entries;

	/// currently selected object
	const obj_desc_timelined_t *selected_desc;
	tool_t *selected_tool;

	// name filter
	char name_filter[64];
	gui_textinput_t name_filter_input;

	// filter components, handled by this class (state is saved and changes trigger fill_list())
	vector_tpl<button_t *> filter_buttons;
	vector_tpl<gui_numberinput_t *> filter_numbers;

	/// all filter checkboxes and number inputs are added here (2 columns)
	gui_aligned_container_t *tbl_filter;

	// image selection
	gui_combobox_t cb_image;
	/// name of the image the user wants to see (preview and thumbnails)
	char wanted_image[64];
	/// target of add_image_choice() while collecting images
	vector_tpl<infra_image_choice_t> *image_out;
	gui_aligned_container_t cont_preview;
	gui_infra_image_t preview;

	/// Adds a checkbox to the filter table
	void add_filter_button(button_t &b, const char *text, bool pressed, bool full_row = false);

	/// Adds a labelled number input to the filter table
	void add_filter_number(gui_numberinput_t &ni, const char *label, sint32 value, sint32 max, sint32 step = 1, const char *tooltip = NULL);

	/// @returns true if the timeline settings allow @p desc to be shown
	bool is_timeline_ok(const obj_desc_timelined_t *desc);

	/// @returns true if the name filter matches @p name
	bool is_name_ok(const char *name) const;

	/// @returns true if @p value (in 1/100 credits) is accepted by a filter with @p limit in credits (0 = no limit)
	static bool is_below_limit(sint64 value, sint32 limit) { return limit <= 0  ||  value <= (sint64)limit*100; }

	/// @returns maintenance per month in 1/100 credits, scaled with month length
	sint64 get_monthly_maintenance(sint64 maintenance) const;

	/// @returns true if the waytype of the object @p wt fits to the waytype of this dialog
	bool is_waytype_ok(waytype_t wt) const { return filter_waytype == ignore_wt  ||  wt == filter_waytype; }

	/// selects @p sortedby in the sorting combobox (if available)
	void set_sortedby(uint8 sortedby);

	/// Adds an image to the list of images (only to be called from fill_image_choices())
	void add_image_choice(const char *name, image_id back, image_id front, sint32 value = -1);

	/// @returns all selectable images of @p e (icon, cursor and the object specific ones)
	void get_images(const infra_entry_t &e, vector_tpl<infra_image_choice_t> &out);

	/// @returns the image of @p e named wanted_image, or the icon if there is no such image
	infra_image_choice_t get_wanted_image(const infra_entry_t &e);

	/// fills the image combobox with all image names (only once, the names are fixed)
	void build_image_combobox();
	bool image_combobox_built;

	/// selects the entry named wanted_image in the image combobox
	void select_wanted_image();

	/// @returns the entry of the selected object, or NULL
	const infra_entry_t *get_selected_entry() const;

	/// @returns true if @p tool builds the object of @p e
	static bool is_same_tool(const tool_t *tool, const infra_entry_t &e);

	/// last active tool, to notice tool changes from elsewhere
	tool_t *last_seen_tool;

	/// collect all objects matching the filters into entries (not sorted)
	virtual void collect_entries() = 0;

	/// add the object specific images with add_image_choice() (icon and cursor are already there)
	virtual void fill_image_choices(const infra_entry_t &) {}

	/// add the names of all images this dialog can show with add_image_choice() (images are ignored),
	/// so the image combobox always offers all of them
	virtual void fill_all_image_names() {}

	/// adds the image names of ways and way objects (image[-] ... diagonal[close2])
	void add_way_image_names();

	/// add object specific text to buf
	virtual void append_info(const infra_entry_t &) {}

	/// shows the currently selected image
	virtual void update_preview();

	/// show and fill the options of the tool of @p e (NULL = nothing selected)
	virtual void update_tool_options(const infra_entry_t *) {}

	/// @returns the tool whose options should be changed for @p e
	tool_t *get_options_tool(const infra_entry_t &e) const;

	/// called when a filter has been changed
	virtual void filter_changed() { fill_list(); }

	/// called after the filter states were loaded
	virtual void filter_state_loaded() {}

	/// creates a thumbnail for the list
	virtual void new_thumbnail(const infra_entry_t &e, const char *name);

	void fill_list() OVERRIDE;

	void change_item_info( sint32 i ) OVERRIDE;

	/// select the query tool, if our tool is active
	void release_tool();

public:
	infra_edit_frame_t(const char *title_key, player_t* player, waytype_t wt);

	waytype_t get_waytype() const { return waytype; }

	/// @returns translated name of a waytype
	static const char *get_waytype_name(waytype_t wt);

	bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE;

	bool infowin_event(event_t const*) OVERRIDE;

	void draw(scr_coord pos, scr_size size) OVERRIDE;

	void rdwr( loadsave_t *file ) OVERRIDE;
};

#endif
