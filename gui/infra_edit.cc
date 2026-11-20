/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include <stdio.h>
#include <string.h>
#include <algorithm>

#include "../simworld.h"
#include "../simmenu.h"
#include "../simevent.h"

#include "../descriptor/obj_base_desc.h"
#include "../descriptor/intro_dates.h"

#include "../dataobj/translator.h"
#include "../dataobj/loadsave.h"
#include "../dataobj/environment.h"

#include "../player/simplay.h"

#include "../display/simgraph.h"

#include "../utils/simstring.h"
#include "../utils/cbuffer_t.h"

#include "infra_edit.h"
#include "simwin.h"


/// ribi names as used in the dat files of ways and way objects
const char* const infra_ribi_codes[26] = {
	"-", "n",  "e",  "ne",  "s",  "ns",  "se",  "nse",
	"w", "nw", "ew", "new", "sw", "nsw", "sew", "nsew",
	"nse1", "new1", "nsw1", "sew1", "nsew1",
	"nse2", "new2", "nsw2", "sew2", "nsew2",
};

/// ribis with extended switch images (in the order of the image list)
const ribi_t::ribi infra_switch_ribis[5] = { 7, 11, 13, 14, 15 };

/// slopes in the order of imageup[3], imageup[6], imageup[9], imageup[12]
const slope_t::type infra_up_slopes[4] = { slope_t::north, slope_t::west, slope_t::east, slope_t::south };
const int infra_up_slope_numbers[4] = { 3, 6, 9, 12 };

/// diagonal ribis in the order of the image list
const ribi_t::ribi infra_diagonal_ribis[4] = { 3, 6, 9, 12 };


gui_infra_image_t::gui_infra_image_t() : back(IMG_EMPTY), front(IMG_EMPTY), player_nr(0), tl(0, 0), box(0, 0)
{
}


void gui_infra_image_t::set_images(image_id back_, image_id front_, sint8 player_nr_)
{
	back = back_;
	front = front_;
	player_nr = player_nr_;

	bool any = false;
	scr_coord_val x0 = 0, y0 = 0, x1 = 0, y1 = 0;
	const image_id imgs[2] = { back, front };
	for(  int i = 0;  i < 2;  i++  ) {
		if(  imgs[i] == IMG_EMPTY  ) {
			continue;
		}
		scr_coord_val x, y, w, h;
		display_get_base_image_offset( imgs[i], &x, &y, &w, &h );
		if(  w <= 0  ||  h <= 0  ) {
			continue;
		}
		if(  !any  ) {
			x0 = x;  y0 = y;  x1 = x + w;  y1 = y + h;
			any = true;
		}
		else {
			x0 = min(x0, x);  y0 = min(y0, y);
			x1 = max(x1, (scr_coord_val)(x + w));  y1 = max(y1, (scr_coord_val)(y + h));
		}
	}
	if(  any  ) {
		tl = scr_coord(x0, y0);
		box = scr_size(x1 - x0, y1 - y0);
	}
	else {
		tl = scr_coord(0, 0);
		box = scr_size(0, 0);
	}
	set_size(box);
}


void gui_infra_image_t::draw(scr_coord offset)
{
	const scr_coord p = offset + get_pos() - tl;
	if(  back != IMG_EMPTY  ) {
		display_base_img( back, p.x, p.y, player_nr, false, true );
	}
	if(  front != IMG_EMPTY  ) {
		display_base_img( front, p.x, p.y, player_nr, false, true );
	}
}


void infra_thumbnail_t::draw(scr_coord offset)
{
	const scr_coord p = offset + get_pos();
	if(  selected  ) {
		display_fillbox_wh_clip_rgb(p.x, p.y, get_size().w, get_size().h,
			focused ? SYSCOL_LIST_BACKGROUND_SELECTED_F : SYSCOL_LIST_BACKGROUND_SELECTED_NF, true);
	}

	// bounding box of both images (in image coordinates)
	const image_id imgs[2] = { back, front };
	scr_coord_val xs[2], ys[2], ws[2], hs[2];
	bool valid[2] = { false, false };
	scr_coord_val x0 = 0, y0 = 0, x1 = 0, y1 = 0;
	bool any = false;
	for(  int i = 0;  i < 2;  i++  ) {
		if(  imgs[i] == IMG_EMPTY  ) {
			continue;
		}
		display_get_base_image_offset( imgs[i], &xs[i], &ys[i], &ws[i], &hs[i] );
		if(  ws[i] <= 0  ||  hs[i] <= 0  ) {
			continue;
		}
		valid[i] = true;
		if(  !any  ) {
			x0 = xs[i];  y0 = ys[i];  x1 = xs[i] + ws[i];  y1 = ys[i] + hs[i];
			any = true;
		}
		else {
			x0 = min(x0, xs[i]);  y0 = min(y0, ys[i]);
			x1 = max(x1, (scr_coord_val)(xs[i] + ws[i]));  y1 = max(y1, (scr_coord_val)(ys[i] + hs[i]));
		}
	}

	if(  any  ) {
		// fit the box into the cell, preserving the aspect ratio, never enlarge
		const scr_size bounds = cell_size - scr_size(4, 4);
		const int w = x1 - x0, h = y1 - y0;
		const int den = max(max(w, h), 1);
		const int num = min(den, min(bounds.w*den/w, bounds.h*den/h));
		const scr_coord base = p + scr_coord(2 + (bounds.w - w*num/den)/2, 2 + (bounds.h - h*num/den)/2);
		for(  int i = 0;  i < 2;  i++  ) {
			if(  valid[i]  ) {
				const scr_coord pos = base + scr_coord( (xs[i]-x0)*num/den, (ys[i]-y0)*num/den );
				display_base_img_scaled( imgs[i], scr_rect( pos, scr_size( max(1, ws[i]*num/den), max(1, hs[i]*num/den) ) ) );
			}
		}
	}

	if(  get_mouse_x() >= p.x  &&  get_mouse_x() < p.x+get_size().w  &&
		get_mouse_y() >= p.y  &&  get_mouse_y() < p.y+get_size().h  ) {
		win_set_tooltip( get_mouse_x() + TOOLTIP_MOUSE_OFFSET_X, p.y + get_size().h + TOOLTIP_MOUSE_OFFSET_Y, name, this );
	}
}


gui_image_choice_item_t::gui_image_choice_item_t(const char *name, image_id back_, image_id front_, sint32 value_) :
	gui_scrolled_list_t::const_text_scrollitem_t(NULL, SYSCOL_TEXT),
	back(back_),
	front(front_),
	value(value_)
{
	tstrncpy( text, name, lengthof(text) );
	set_text_pointer( text );
}


const char *infra_edit_frame_t::get_waytype_name(waytype_t wt)
{
	switch(  wt  ) {
		case ignore_wt:        return translator::translate("All");
		case road_wt:          return translator::translate("Road");
		case track_wt:         return translator::translate("Train");
		case water_wt:         return translator::translate("Ship");
		case overheadlines_wt: return translator::translate("overheadlines");
		case monorail_wt:      return translator::translate("Monorail");
		case maglev_wt:        return translator::translate("Maglev");
		case tram_wt:          return translator::translate("Tram");
		case narrowgauge_wt:   return translator::translate("Narrowgauge");
		case air_wt:           return translator::translate("Airport");
		case powerline_wt:     return translator::translate("Powerlines");
		default:               return translator::translate("unknown");
	}
}


infra_edit_frame_t::infra_edit_frame_t(const char *title_key, player_t* player_, waytype_t wt) :
	extend_edit_gui_t("", player_),
	waytype(wt),
	filter_waytype(wt),
	entries(32),
	selected_desc(NULL),
	selected_tool(NULL)
{
	last_seen_tool = NULL;
	image_out = NULL;
	image_combobox_built = false;
	tstrncpy( wanted_image, translator::translate("Icon"), lengthof(wanted_image) );
	snprintf( title, lengthof(title), "%s (%s)", translator::translate(title_key), get_waytype_name(waytype) );
	set_name( title );

	// climates are meaningless for these objects
	tbl_climate->set_visible(false);
	bt_climates.set_visible(false);

	// name filter
	name_filter[0] = 0;
	gui_aligned_container_t *tbl = cont_filter.add_table(2,0);
	tbl->new_component<gui_label_t>("Filter names:");
	name_filter_input.set_text( name_filter, lengthof(name_filter) );
	name_filter_input.add_listener(this);
	tbl->add_component(&name_filter_input);
	cont_filter.end_table();

	// waytype selection, only if the dialog is not for a single waytype
	if(  waytype == ignore_wt  ) {
		static const waytype_t waytypes[] = { ignore_wt, road_wt, track_wt, tram_wt, monorail_wt, maglev_wt, narrowgauge_wt, water_wt, air_wt, powerline_wt };
		gui_aligned_container_t *tbl_wt = cont_filter.add_table(2,0);
		tbl_wt->new_component<gui_label_t>("Waytype");
		tbl_wt->add_component(&cb_waytype);
		for(  uint i = 0;  i < lengthof(waytypes);  i++  ) {
			cb_waytype.new_component<gui_image_choice_item_t>( get_waytype_name(waytypes[i]), IMG_EMPTY, IMG_EMPTY, (sint32)waytypes[i] );
		}
		cb_waytype.set_selection(0);
		cb_waytype.add_listener(this);
		cont_filter.end_table();
	}

	// filled by the sub classes
	tbl_filter = cont_filter.add_table(2,0);
	cont_filter.end_table();

	init_thumbnail_options();

	// sorting
	cb_sortedby.new_component<gui_sorting_item_t>(gui_sorting_item_t::BY_DATE_INTRO);
	cb_sortedby.new_component<gui_sorting_item_t>(gui_sorting_item_t::BY_DATE_RETIRE);
	cb_sortedby.new_component<gui_sorting_item_t>(gui_sorting_item_t::BY_COST);

	// image selection
	gui_aligned_container_t *tbl_img = cont_options.add_table(2,0);
	tbl_img->new_component<gui_label_t>("Image");
	tbl_img->add_component(&cb_image);
	cb_image.add_listener(this);
	// filled by build_image_combobox() (needs the sub class)
	cont_options.end_table();

	// description and image
	cont_scrolly.remove_all();
	cont_scrolly.set_table_layout(2, 0);
	cont_scrolly.set_margin(scr_size(0, D_V_SPACE), scr_size(0, D_V_SPACE));
	cont_scrolly.add_component(&info_text, 2);
	cont_scrolly.add_component(&cont_preview);
	cont_preview.set_table_layout(1, 0);
	cont_preview.add_component(&preview);
	cont_preview.add_component(&building_image);
	building_image.set_visible(false);
	cont_scrolly.new_component<gui_fill_t>(true, false);
	cont_scrolly.new_component_span<gui_fill_t>(false, true, 2);

	// sub classes have to call fill_list() at the end of their constructor
}


void infra_edit_frame_t::add_filter_button(button_t &b, const char *text, bool pressed, bool full_row)
{
	b.init( button_t::square_state, text );
	b.pressed = pressed;
	b.add_listener(this);
	tbl_filter->add_component( &b, full_row ? 2 : 1 );
	filter_buttons.append( &b );
}


void infra_edit_frame_t::add_filter_number(gui_numberinput_t &ni, const char *label, sint32 value, sint32 max, sint32 step, const char *tooltip)
{
	gui_label_t *lb = tbl_filter->new_component<gui_label_t>(label);
	if(  tooltip  ) {
		lb->set_tooltip( translator::translate(tooltip) );
	}
	ni.init( value, 0, max, step, false, 7 );
	ni.add_listener(this);
	tbl_filter->add_component( &ni );
	filter_numbers.append( &ni );
}


void infra_edit_frame_t::set_sortedby(uint8 sortedby)
{
	for(  int i = 0;  i < cb_sortedby.count_elements();  i++  ) {
		if(  gui_sorting_item_t *item = dynamic_cast<gui_sorting_item_t *>( cb_sortedby.get_element(i) )  ) {
			if(  (uint8)item->get_sortedby() == sortedby  ) {
				cb_sortedby.set_selection(i);
				return;
			}
		}
	}
}


bool infra_edit_frame_t::is_timeline_ok(const obj_desc_timelined_t *desc)
{
	if(  !bt_timeline.pressed  &&  !bt_timeline_custom.pressed  ) {
		return true;
	}
	const uint16 month_now = (uint16)(bt_timeline.pressed ? welt->get_current_month() : ni_timeline_year.get_value()*12 + ni_timeline_month.get_value()-1);
	return !desc->is_future(month_now)  &&  (!desc->is_retired(month_now)  ||  bt_obsolete.pressed);
}


bool infra_edit_frame_t::is_name_ok(const char *name) const
{
	return name_filter[0] == 0  ||  utf8caseutf8(name, name_filter)  ||  utf8caseutf8(translator::translate(name), name_filter);
}


sint64 infra_edit_frame_t::get_monthly_maintenance(sint64 maintenance) const
{
	return welt->scale_with_month_length(maintenance);
}


void infra_edit_frame_t::add_image_choice(const char *name, image_id back, image_id front, sint32 value)
{
	if(  image_out  ) {
		infra_image_choice_t c;
		tstrncpy( c.name, name, lengthof(c.name) );
		c.back = back;
		c.front = front;
		c.value = value;
		image_out->append( c );
	}
}


void infra_edit_frame_t::add_way_image_names()
{
	char name[64];
	for(  int r = 0;  r < 26;  r++  ) {
		sprintf( name, "image[%s]", infra_ribi_codes[r] );
		add_image_choice( name, IMG_EMPTY, IMG_EMPTY );
	}
	for(  uint8 h = 1;  h <= 2;  h++  ) {
		for(  uint8 k = 0;  k < 4;  k++  ) {
			sprintf( name, h == 1 ? "imageup[%d]" : "imageup2[%d]", infra_up_slope_numbers[k] );
			add_image_choice( name, IMG_EMPTY, IMG_EMPTY );
		}
	}
	for(  uint8 k = 0;  k < 4;  k++  ) {
		sprintf( name, "diagonal[%s]", infra_ribi_codes[infra_diagonal_ribis[k]] );
		add_image_choice( name, IMG_EMPTY, IMG_EMPTY );
	}
	for(  uint8 k = 0;  k < 2;  k++  ) {
		sprintf( name, "diagonal[close%d]", k+1 );
		add_image_choice( name, IMG_EMPTY, IMG_EMPTY );
	}
}


void infra_edit_frame_t::get_images(const infra_entry_t &e, vector_tpl<infra_image_choice_t> &out)
{
	out.clear();
	image_out = &out;
	add_image_choice( translator::translate("Icon"),   e.icon,   IMG_EMPTY );
	add_image_choice( translator::translate("Cursor"), e.cursor, IMG_EMPTY );
	fill_image_choices( e );
	image_out = NULL;
}


infra_image_choice_t infra_edit_frame_t::get_wanted_image(const infra_entry_t &e)
{
	static vector_tpl<infra_image_choice_t> images;
	get_images( e, images );
	FOR(vector_tpl<infra_image_choice_t>, const& c, images) {
		if(  strcmp( c.name, wanted_image ) == 0  &&  (c.value >= 0  ||  c.back != IMG_EMPTY  ||  c.front != IMG_EMPTY)  ) {
			return c;
		}
	}
	// no such image => icon
	return images[0];
}


void infra_edit_frame_t::build_image_combobox()
{
	if(  image_combobox_built  ) {
		return;
	}
	image_combobox_built = true;

	// all image names this dialog knows, independent of the objects
	// (the images themselves are taken from the selected object when shown)
	vector_tpl<infra_image_choice_t> all;
	image_out = &all;
	add_image_choice( translator::translate("Icon"),   IMG_EMPTY, IMG_EMPTY );
	add_image_choice( translator::translate("Cursor"), IMG_EMPTY, IMG_EMPTY );
	fill_all_image_names();
	image_out = NULL;

	cb_image.clear_elements();
	int sel = 0;
	FOR(vector_tpl<infra_image_choice_t>, const& c, all) {
		if(  strcmp( c.name, wanted_image ) == 0  ) {
			sel = cb_image.count_elements();
		}
		cb_image.new_component<gui_image_choice_item_t>( c.name, IMG_EMPTY, IMG_EMPTY, (sint32)-1 );
	}
	cb_image.set_selection( sel );
}


void infra_edit_frame_t::select_wanted_image()
{
	for(  int i = 0;  i < cb_image.count_elements();  i++  ) {
		if(  strcmp( cb_image.get_element(i)->get_text(), wanted_image ) == 0  ) {
			cb_image.set_selection( i );
			return;
		}
	}
}


const infra_entry_t *infra_edit_frame_t::get_selected_entry() const
{
	const sint32 sel = scl.get_selection();
	return sel >= 0  &&  sel < (sint32)entries.get_count()  &&  entries[sel].desc == selected_desc ? &entries[sel] : NULL;
}


void infra_edit_frame_t::new_thumbnail(const infra_entry_t &e, const char *name)
{
	const infra_image_choice_t img = get_wanted_image( e );
	scl.new_component<infra_thumbnail_t>( img.back, img.front, name, get_thumbnail_size() );
}


namespace {
	struct infra_entry_compare_t
	{
		uint8 sortedby;
		bool reverse;

		template<class T> static int cmp(T a, T b) { return a < b ? -1 : (b < a ? 1 : 0); }

		bool operator()(const infra_entry_t &a, const infra_entry_t &b) const
		{
			int diff = 0;
			switch(  sortedby  ) {
				case gui_sorting_item_t::BY_NAME_OBJECT: diff = strcmp(a.name, b.name);             break;
				case gui_sorting_item_t::BY_DATE_INTRO:  diff = cmp(a.intro_date, b.intro_date);    break;
				case gui_sorting_item_t::BY_DATE_RETIRE: diff = cmp(a.retire_date, b.retire_date);  break;
				case gui_sorting_item_t::BY_COST:        diff = cmp(a.price, b.price);              break;
				case gui_sorting_item_t::BY_MAINTENANCE: diff = cmp(a.maintenance, b.maintenance);  break;
				case gui_sorting_item_t::BY_SPEED:       diff = cmp(a.speed, b.speed);              break;
				case gui_sorting_item_t::BY_LEVEL:       diff = cmp(a.level, b.level);              break;
				case gui_sorting_item_t::BY_TYPE:        diff = cmp(a.kind, b.kind);                break;
				default: break;
			}
			if(  diff == 0  ) {
				diff = strcmp( translator::translate(a.name), translator::translate(b.name) );
			}
			if(  diff == 0  ) {
				diff = strcmp( a.name, b.name );
			}
			if(  diff == 0  ) {
				diff = cmp(a.kind, b.kind);
			}
			return reverse ? diff > 0 : diff < 0;
		}
	};
}


void infra_edit_frame_t::fill_list()
{
	build_image_combobox();

	entries.clear();
	collect_entries();

	infra_entry_compare_t cmp;
	cmp.sortedby = get_sortedby();
	cmp.reverse = sort_order.pressed;
	std::sort( entries.begin(), entries.end(), cmp );

	// now build scrolled list
	scl.clear_elements();
	scl.set_selection(-1);
	FOR(vector_tpl<infra_entry_t>, const& e, entries) {
		char const* const name = get_sortedby()==gui_sorting_item_t::BY_NAME_OBJECT ? e.name : translator::translate(e.name);
		if(  bt_thumbnails.pressed  ) {
			new_thumbnail( e, name );
		}
		else {
			scl.new_component<gui_scrolled_list_t::const_text_scrollitem_t>( name, e.color ? e.color : SYSCOL_TEXT );
		}
		if(  e.desc == selected_desc  ) {
			scl.set_selection( scl.get_count()-1 );
		}
	}
	update_thumbnail_columns();
	// always update current selection (since the tool may depend on it)
	change_item_info( scl.get_selection() );
	show_selected_building();

	reset_min_windowsize();
}


void infra_edit_frame_t::update_preview()
{
	const sint8 player_nr = welt->get_active_player_nr();
	const infra_entry_t *e = get_selected_entry();
	if(  e == NULL  ) {
		preview.set_images( IMG_EMPTY, IMG_EMPTY, player_nr );
		return;
	}
	// the image with the selected name, or the icon if this object has no such image
	const infra_image_choice_t img = get_wanted_image( *e );
	preview.set_images( img.back, img.front, player_nr );
}


void infra_edit_frame_t::release_tool()
{
	player_t *pl = welt->get_active_player();
	if(  selected_tool  &&  welt->get_tool( pl->get_player_nr() ) == selected_tool  ) {
		welt->set_tool( tool_t::general_tool[TOOL_QUERY], pl );
	}
	selected_tool = NULL;
	last_seen_tool = welt->get_tool( pl->get_player_nr() );
}


bool infra_edit_frame_t::is_same_tool(const tool_t *tool, const infra_entry_t &e)
{
	if(  tool == NULL  ||  e.tool == NULL  ) {
		return false;
	}
	if(  tool == e.tool  ) {
		return true;
	}
	// another instance of the same tool (e.g. defined in menuconf.tab): compare the object name,
	// which is the first part of the default parameter
	if(  tool->get_id() != e.tool->get_id()  ) {
		return false;
	}
	const char *param = tool->get_default_param(NULL);
	if(  param == NULL  ) {
		return false;
	}
	const size_t len = strlen( e.name );
	return strncmp( param, e.name, len ) == 0  &&  (param[len] == 0  ||  param[len] == ',');
}


void infra_edit_frame_t::draw(scr_coord pos, scr_size size)
{
	// follow tool changes from elsewhere (e.g. the toolbar)
	player_t *pl = welt->get_active_player();
	tool_t *tool = pl ? welt->get_tool( pl->get_player_nr() ) : NULL;
	if(  tool != last_seen_tool  ) {
		last_seen_tool = tool;
		if(  tool != selected_tool  ) {
			sint32 found = -1;
			for(  uint32 i = 0;  i < entries.get_count();  i++  ) {
				if(  is_same_tool( tool, entries[i] )  ) {
					found = (sint32)i;
					break;
				}
			}
			if(  found >= 0  ) {
				scl.set_selection( found );
				change_item_info( found );
				show_selected_building();
			}
			else if(  selected_desc  ) {
				// the tool is not ours anymore => deselect (but keep the new tool)
				selected_tool = NULL;
				scl.set_selection( -1 );
				// set_selection(-1) keeps the highlight of the old item
				for(  sint32 i = 0;  i < scl.get_count();  i++  ) {
					if(  gui_scrolled_list_t::scrollitem_t *item = scl.get_element(i)  ) {
						item->selected = false;
					}
				}
				selected_desc = NULL;
				buf.clear();
				info_text.recalc_size();
				update_preview();
				reset_min_windowsize();
			}
		}
	}
	extend_edit_gui_t::draw( pos, size );
}


void infra_edit_frame_t::change_item_info(sint32 entry)
{
	if(  entry >= 0  &&  entry < (sint32)entries.get_count()  ) {
		const infra_entry_t &e = entries[entry];

		if(  e.desc != selected_desc  ) {
			selected_desc = e.desc;

			buf.clear();
			buf.append( translator::translate(e.name) );
			buf.append( "\n" );
			if(  strcmp( translator::translate(e.name), e.name )  ) {
				buf.printf( "(%s)\n", e.name );
			}
			buf.append( "\n" );

			char money[64];
			money_to_string( money, (double)e.price / 100.0 );
			buf.printf( "%s: %s\n", translator::translate("Price"), money );
			money_to_string( money, (double)get_monthly_maintenance(e.maintenance) / 100.0 );
			buf.printf( "%s: %s\n", translator::translate("Maintenance"), money );

			append_info( e );

			buf.printf( "%s%u", translator::translate("\nBauzeit von"), e.intro_date / 12 );
			if(  e.retire_date != DEFAULT_RETIRE_DATE*12  ) {
				buf.printf( "%s%u", translator::translate("\nBauzeit bis"), e.retire_date / 12 );
			}
			if(  e.copyright  &&  *e.copyright  ) {
				buf.append( "\n" );
				buf.printf( translator::translate("Constructed by %s"), e.copyright );
			}
			buf.append( "\n" );
		}

		update_preview();

		// select the tool (the same as in the toolbar)
		player_t *pl = welt->get_active_player();
		if(  e.tool  &&  welt->get_tool( pl->get_player_nr() ) != e.tool  &&  !is_same_tool( welt->get_tool( pl->get_player_nr() ), e )  ) {
			welt->set_tool( e.tool, pl );
		}
		selected_tool = welt->get_tool( pl->get_player_nr() );
		last_seen_tool = selected_tool;
	}
	else {
		if(  selected_desc  ) {
			selected_desc = NULL;
			buf.clear();
		}
		release_tool();
		update_preview();
	}
	info_text.recalc_size();
	reset_min_windowsize();
}


bool infra_edit_frame_t::action_triggered( gui_action_creator_t *comp, value_t e )
{
	if(  comp == &bt_thumbnails  ) {
		bt_thumbnails.pressed = !bt_thumbnails.pressed;
		fill_list();
		return true;
	}
	if(  comp == &cb_waytype  ) {
		if(  const gui_image_choice_item_t *item = dynamic_cast<const gui_image_choice_item_t *>( cb_waytype.get_selected_item() )  ) {
			filter_waytype = (waytype_t)item->value;
		}
		filter_changed();
		return true;
	}
	if(  comp == &name_filter_input  ) {
		fill_list();
		return true;
	}
	if(  comp == &cb_image  ) {
		if(  const gui_image_choice_item_t *item = dynamic_cast<const gui_image_choice_item_t *>( cb_image.get_selected_item() )  ) {
			tstrncpy( wanted_image, item->get_text(), lengthof(wanted_image) );
		}
		if(  bt_thumbnails.pressed  ) {
			// thumbnails show the selected image
			fill_list();
		}
		update_preview();
		reset_min_windowsize();
		return true;
	}
	FOR(vector_tpl<button_t *>, b, filter_buttons) {
		if(  comp == b  ) {
			b->pressed ^= 1;
			filter_changed();
			return true;
		}
	}
	FOR(vector_tpl<gui_numberinput_t *>, ni, filter_numbers) {
		if(  comp == ni  ) {
			filter_changed();
			return true;
		}
	}
	return extend_edit_gui_t::action_triggered( comp, e );
}


bool infra_edit_frame_t::infowin_event(const event_t *ev)
{
	if(  ev->ev_class == INFOWIN  &&  ev->ev_code == WIN_CLOSE  ) {
		release_tool();
	}
	return extend_edit_gui_t::infowin_event(ev);
}


void infra_edit_frame_t::rdwr( loadsave_t *file )
{
	// the waytype is part of the magic number, so it is not saved here
	file->rdwr_str( name_filter, lengthof(name_filter) );
	if(  file->is_loading()  ) {
		name_filter_input.set_text( name_filter, lengthof(name_filter) );
	}

	uint8 count = (uint8)filter_buttons.get_count();
	file->rdwr_byte( count );
	for(  uint8 i = 0;  i < count;  i++  ) {
		bool pressed = i < filter_buttons.get_count() ? filter_buttons[i]->pressed : false;
		file->rdwr_bool( pressed );
		if(  i < filter_buttons.get_count()  ) {
			filter_buttons[i]->pressed = pressed;
		}
	}

	count = (uint8)filter_numbers.get_count();
	file->rdwr_byte( count );
	for(  uint8 i = 0;  i < count;  i++  ) {
		sint32 value = i < filter_numbers.get_count() ? filter_numbers[i]->get_value() : 0;
		file->rdwr_long( value );
		if(  i < filter_numbers.get_count()  ) {
			filter_numbers[i]->set_value( value );
		}
	}

	bool thumbnails = bt_thumbnails.pressed;
	file->rdwr_bool( thumbnails );
	bt_thumbnails.pressed = thumbnails;

	file->rdwr_str( wanted_image, lengthof(wanted_image) );
	if(  file->is_loading()  ) {
		select_wanted_image();
	}

	uint8 wt_sel = (uint8)max(0, cb_waytype.get_selection());
	file->rdwr_byte( wt_sel );
	if(  file->is_loading()  &&  waytype == ignore_wt  &&  wt_sel < cb_waytype.count_elements()  ) {
		cb_waytype.set_selection( wt_sel );
		if(  const gui_image_choice_item_t *item = dynamic_cast<const gui_image_choice_item_t *>( cb_waytype.get_selected_item() )  ) {
			filter_waytype = (waytype_t)item->value;
		}
	}

	if(  file->is_loading()  ) {
		// e.g. enable/disable dependent filters
		filter_state_loaded();
	}

	// this will refill the list (and the image combobox)
	extend_edit_gui_t::rdwr( file );

	if(  file->is_loading()  ) {
		update_preview();
		reset_min_windowsize();
	}
}
