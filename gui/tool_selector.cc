/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */


#include "../dataobj/environment.h"
#include "../display/simimg.h"
#include "../display/simgraph.h"
#include "../player/simplay.h"
#include "../utils/for.h"
#include "../utils/simstring.h"
#include "../simworld.h"
#include "../simmenu.h"
#include "../simskin.h"
#include "gui_frame.h"
#include "simwin.h"
#include "tool_selector.h"

#define MIN_WIDTH (80)


tool_selector_t::tool_selector_t(const char* title, const char *help_file, uint32 toolbar_id, bool allow_break) :
	gui_frame_t( translator::translate(title) ), tools(0)
{
	set_table_layout(0,0); // we do our own positioning of icons (for now)
	this->toolbar_id = toolbar_id;
	this->allow_break = allow_break;
	this->help_file = help_file;
	this->tool_icon_disp_start = 0;
	this->tool_icon_disp_end = 0;
	this->title = title;
	has_prev_next = false;
	is_dragging = false;
	is_scrollbar_dragging = false;
	last_tool_icon_disp_start = 0;
	offset = scr_coord( 0, 0 );
	set_windowsize( scr_size(max(env_t::iconsize.w,MIN_WIDTH), D_TITLEBAR_HEIGHT) );
	dirty = true;
}


/**
 * Add a new tool with values and tooltip text.
 * tool_in must be created by new tool_t(copy_tool)!
 */
void tool_selector_t::add_tool_selector(tool_t *tool_in)
{
	if(  env_t::iconsize.w <= 0  ||  env_t::iconsize.h <= 0  ) {
		// icons disabled (icon_height<=0 in menuconf.tab): do not add anything,
		// bail out before any division by iconsize below
		return;
	}

	image_id tool_img = tool_in->get_icon(welt->get_active_player());
	if(  tool_img == IMG_EMPTY  &&  tool_in!=tool_t::dummy  ) {
		return;
	}

	// only for non-empty icons ...
	tools.append(tool_in);

	int ww = max(2,(display_get_width()/env_t::iconsize.w)-2); // to avoid zero or negative ww on posix (no graphic) backends
	tool_icon_width = tools.get_count();
DBG_DEBUG4("tool_selector_t::add_tool()","ww=%i, tool_icon_width=%i",ww,tool_icon_width);
	if(  allow_break  &&  (ww<tool_icon_width
		||  (env_t::toolbar_max_width>0  &&  env_t::toolbar_max_width<tool_icon_width)
		||  (env_t::toolbar_max_width<0  &&  (ww+env_t::toolbar_max_width)<tool_icon_width))
		) {
		//break them
		int rows = (tool_icon_width/ww)+1;
DBG_DEBUG4("tool_selector_t::add_tool()","ww=%i, rows=%i",ww,rows);
		// assure equal distribution if more than a single row is needed
		tool_icon_width = (tool_icon_width+rows-1)/rows;
		if(  env_t::toolbar_max_width != 0  ) {
			// At least, 3 rows is needed to drag toolbar
			tool_icon_width = min( tool_icon_width, max(env_t::toolbar_max_width, 3) );
		}
	}
	tool_icon_height = max( (display_get_height()/env_t::iconsize.h)-3, 1 );
	if(  env_t::toolbar_max_height > 0  ) {
		tool_icon_height = min(tool_icon_height, env_t::toolbar_max_height);
	}
	dirty = true;
	has_prev_next = ((uint32)tool_icon_width*tool_icon_height < tools.get_count());
	scr_size winsize( tool_icon_width*env_t::iconsize.w, min(tool_icon_height, ((tools.get_count()-1)/tool_icon_width)+1)*env_t::iconsize.h+D_TITLEBAR_HEIGHT );
	if(  has_prev_next  ) {
		// reserve space for a scrollbar strip just outside the icon grid:
		// below it for a single row, beside it for a column/multi-row grid
		if(  tool_icon_height == 1  ) {
			winsize.h += env_t::menu_scrollbar_thickness;
		}
		else {
			winsize.w += env_t::menu_scrollbar_thickness;
		}
	}
	set_windowsize( winsize );
	tool_icon_disp_start = 0;
	tool_icon_disp_end = min( tool_icon_disp_start+tool_icon_width*tool_icon_height, tools.get_count() );

DBG_DEBUG4("tool_selector_t::add_tool()", "at position %i (width %i)", tools.get_count(), tool_icon_width);
}


// reset the tools to empty state
void tool_selector_t::reset_tools()
{
	tools.clear();
	gui_frame_t::set_windowsize( scr_size(max(env_t::iconsize.w,MIN_WIDTH), D_TITLEBAR_HEIGHT) );
	tool_icon_width = 0;
	tool_icon_disp_start = 0;
	tool_icon_disp_end = 0;
	offset = scr_coord( 0, 0 );
	is_scrollbar_dragging = false;
	last_tool_icon_disp_start = 0;
}


bool tool_selector_t::is_hit(int x, int y)
{
	if(  env_t::iconsize.w <= 0  ||  env_t::iconsize.h <= 0  ) {
		// icons disabled: nothing to hit besides the (icon-less) titlebar
		return x>=0  &&  y>=0  &&  y<D_TITLEBAR_HEIGHT  &&  x<get_windowsize().w;
	}

	if(  has_prev_next  &&  get_scrollbar_rect().contains( scr_coord(x,y) )  ) {
		return true;
	}

	const scr_coord icon_off = get_icon_area_offset();
	int dx = (x-offset.x-icon_off.x)/env_t::iconsize.w;
	int dy = (y-D_TITLEBAR_HEIGHT-offset.y-icon_off.y)/env_t::iconsize.h;

	// either click in titlebar or on an icon
	if(  x>=0   &&  y>=0  &&  ( (y<D_TITLEBAR_HEIGHT  &&  x<get_windowsize().w)  ||  (dx<tool_icon_width  &&  dy<tool_icon_height) )  ) {
		return y < D_TITLEBAR_HEIGHT || dx + tool_icon_width * dy + tool_icon_disp_start < (int)tools.get_count();
	}
	return false;
}


// window-relative rect of the scrollbar strip: below the icon row for a single-row
// toolbar, or beside the icon column(s) otherwise. Only meaningful when has_prev_next.
scr_rect tool_selector_t::get_scrollbar_rect() const
{
	const scr_size sz = get_windowsize();
	const scr_coord icon_off = get_icon_area_offset();
	if(  tool_icon_height == 1  ) {
		// icons pushed down (icon_off.y>0, MENU_BOTTOM main menu): scrollbar sits
		// above them; otherwise (MENU_TOP, or any popup) it sits below as usual
		const scr_coord_val y = icon_off.y>0 ? D_TITLEBAR_HEIGHT : D_TITLEBAR_HEIGHT + env_t::iconsize.h;
		return scr_rect( 0, y, sz.w, env_t::menu_scrollbar_thickness );
	}
	else {
		// icons pushed right (icon_off.x>0, MENU_RIGHT main menu): scrollbar sits
		// to their left; otherwise (MENU_LEFT, or any popup) it sits to their right
		const scr_coord_val x = icon_off.x>0 ? 0 : tool_icon_width*env_t::iconsize.w;
		// for a real popup toolbar window, sz.h includes its titlebar, so the track
		// starts after it and is shorter by D_TITLEBAR_HEIGHT. But for the main
		// menubar (toolbar_id==0), sz.h is the true content height with NO titlebar
		// component: its pos is pre-shifted by -D_TITLEBAR_HEIGHT (see draw()) so
		// that content, which is always placed at +D_TITLEBAR_HEIGHT, lands at the
		// true window top; subtracting D_TITLEBAR_HEIGHT from the track height here
		// too double-counts that shift, making the track (and so the drag's max
		// reach) exactly D_TITLEBAR_HEIGHT pixels short of the window's true bottom
		const scr_coord_val track_h = toolbar_id==0 ? sz.h : sz.h - D_TITLEBAR_HEIGHT;
		return scr_rect( x, D_TITLEBAR_HEIGHT, env_t::menu_scrollbar_thickness, track_h );
	}
}


// see header for rationale; (0,0) for anything but the main menubar with a
// reserved scrollbar strip
scr_coord tool_selector_t::get_icon_area_offset() const
{
	if(  toolbar_id != 0  ||  !has_prev_next  ) {
		return scr_coord( 0, 0 );
	}
	if(  env_t::menupos == MENU_BOTTOM  ) {
		return scr_coord( 0, env_t::menu_scrollbar_thickness );
	}
	if(  env_t::menupos == MENU_RIGHT  ) {
		return scr_coord( env_t::menu_scrollbar_thickness, 0 );
	}
	return scr_coord( 0, 0 );
}


void tool_selector_t::get_scroll_metrics(bool &horizontal, sint32 &unit, sint32 &visible_units, sint32 &total_units) const
{
	horizontal = (tool_icon_height == 1);
	if(  horizontal  ) {
		unit = 1;
		visible_units = tool_icon_width;
		total_units = (sint32)tools.get_count();
	}
	else if(  tool_icon_width == 1  ) {
		unit = 1;
		visible_units = tool_icon_height;
		total_units = (sint32)tools.get_count();
	}
	else {
		// a multi-column grid scrolls by whole rows, so columns stay aligned
		unit = tool_icon_width;
		visible_units = tool_icon_height;
		total_units = ((sint32)tools.get_count() + tool_icon_width - 1) / tool_icon_width;
	}
}


bool tool_selector_t::infowin_event(const event_t *ev)
{
	if(  env_t::iconsize.w <= 0  ||  env_t::iconsize.h <= 0  ) {
		// icons disabled (icon_height<=0): skip all icon-grid math below, since it
		// divides by iconsize and would otherwise crash (or hang, for the sanity-check
		// loops) with a zero or negative value
		if(  ev->ev_class==INFOWIN  &&  (ev->ev_code==WIN_TOP  ||  ev->ev_code==WIN_OPEN)  ) {
			set_name( translator::translate(title) );
		}
		return false;
	}

	// mouse-wheel scrolling anywhere over the toolbar moves the scrollbar by one unit
	if(  has_prev_next  &&  (IS_WHEELUP(ev)  ||  IS_WHEELDOWN(ev))  ) {
		bool horizontal;
		sint32 unit, visible_units, total_units;
		get_scroll_metrics( horizontal, unit, visible_units, total_units );
		sint32 cur_unit = unit>0 ? tool_icon_disp_start/unit : 0;
		cur_unit += IS_WHEELDOWN(ev) ? 1 : -1;
		cur_unit = clamp( cur_unit, 0, max(0,total_units-visible_units) );
		tool_icon_disp_start = (uint16)(cur_unit*unit);
		offset.x = 0;
		offset.y = 0;
		tool_icon_disp_end = min( (uint32)tool_icon_disp_start + (uint32)tool_icon_width*tool_icon_height, (uint32)tools.get_count() );
		dirty = true;
		return true;
	}

	// every toolbar (main menubar and popup icon-list windows alike) shows a thin,
	// continuously draggable scrollbar strip outside the icon area when it overflows
	// (see get_scrollbar_rect()); handle clicks/drags on it here
	if(  has_prev_next  &&  (IS_LEFTCLICK(ev)  ||  IS_LEFTDRAG(ev)  ||  is_scrollbar_dragging)  ) {
		const scr_rect track = get_scrollbar_rect();
		bool horizontal;
		sint32 unit, visible_units, total_units;
		get_scroll_metrics( horizontal, unit, visible_units, total_units );

		bool hit_now = false;
		if(  !is_scrollbar_dragging  ) {
			hit_now = track.contains( scr_coord(ev->cx, ev->cy) );
		}
		if(  is_scrollbar_dragging  ||  hit_now  ) {
			is_scrollbar_dragging = true;
			sint32 cur_unit;
			if(  horizontal  ) {
				const scr_coord_val thumb_w = max( (scr_coord_val)8, (scr_coord_val)( (sint64)track.w * visible_units / max(1,total_units) ) );
				const scr_coord_val avail = track.w - thumb_w;
				const scr_coord_val target_x = clamp( (scr_coord_val)(ev->mx - track.x - thumb_w/2), (scr_coord_val)0, max((scr_coord_val)0,avail) );
				cur_unit = (avail>0  &&  total_units>visible_units) ? (sint32)( (sint64)target_x * (total_units-visible_units) / avail ) : 0;
			}
			else {
				const scr_coord_val thumb_h = max( (scr_coord_val)8, (scr_coord_val)( (sint64)track.h * visible_units / max(1,total_units) ) );
				const scr_coord_val avail = track.h - thumb_h;
				const scr_coord_val target_y = clamp( (scr_coord_val)(ev->my - track.y - thumb_h/2), (scr_coord_val)0, max((scr_coord_val)0,avail) );
				cur_unit = (avail>0  &&  total_units>visible_units) ? (sint32)( (sint64)target_y * (total_units-visible_units) / avail ) : 0;
			}
			cur_unit = clamp( cur_unit, 0, max(0,total_units-visible_units) );
			tool_icon_disp_start = (uint16)(cur_unit*unit);
			offset.x = 0;
			offset.y = 0;
			tool_icon_disp_end = min( (uint32)tool_icon_disp_start + (uint32)tool_icon_width*tool_icon_height, (uint32)tools.get_count() );
			if(  !IS_LEFTRELEASE(ev)  &&  ev->button_state != 1  ) {
				is_scrollbar_dragging = false;
			}
			return true;
		}
	}

	if(  has_prev_next  &&  (IS_LEFTDRAG(ev)  ||  is_dragging)  ) {
		if( !is_dragging ) {
			old_offset = offset;
		}
		// currently only drag in x directions
		is_dragging = true;
		offset = old_offset + (tool_icon_height == 1 ? scr_coord(ev->mx - ev->cx, 0) : scr_coord(0, ev->my - ev->cy));
		int xy = tool_icon_width*tool_icon_height;
		if(  tool_icon_height == 1  &&  tool_icon_disp_start + xy >= (int)tools.get_count()  ) {
			// we have to take into account that the height is not a full icon
			tool_icon_disp_start = max(0, (int)tools.get_count() - xy);
			offset.x = -min(-offset.x, env_t::iconsize.w-(get_windowsize().w % env_t::iconsize.w));
		}
		if(  tool_icon_width == 1  &&  tool_icon_disp_start + xy >= (int)tools.get_count()  ) {
			// we have to take into account that the height is not a full icon
			tool_icon_disp_start = max(0, (int)tools.get_count() - xy);
			offset.y = -min( -offset.y, (get_windowsize().h-D_TITLEBAR_HEIGHT) % env_t::iconsize.h);
		}
		if(  tool_icon_disp_start == 0  &&  (offset.x > 0  ||  offset.y > 0)  ) {
			offset.x = 0;
			offset.y = 0;
		}
		if (offset.x > 0) {
			// we must change the old offset, since the mouse starting point changed!
			old_offset.x -= env_t::iconsize.w;
			offset.x -= env_t::iconsize.w;
			tool_icon_disp_start--;
		}
		if (offset.x <= -env_t::iconsize.w) {
			old_offset.x += env_t::iconsize.w;
			offset.x += env_t::iconsize.w;
			tool_icon_disp_start++;
		}
		if (offset.y > 0) {
			// we must change the old offset, since the mouse starting point changed!
			old_offset.y -= env_t::iconsize.h;
			offset.y -= env_t::iconsize.h;
			tool_icon_disp_start--;
		}
		if (offset.y <= -env_t::iconsize.h) {
			old_offset.y += env_t::iconsize.h;
			offset.y += env_t::iconsize.h;
			tool_icon_disp_start++;
		}
		if(  !IS_LEFTRELEASE(ev)  &&  ev->button_state != 1 ) {
			is_dragging = false;
		}
	}

	// offsets sanity check
	while (offset.x < -env_t::iconsize.w) {
		offset.x += env_t::iconsize.w;
	}
	while (offset.x > 0) {
		offset.x -= env_t::iconsize.w;
	}
	while (offset.y < -env_t::iconsize.h) {
		offset.y += env_t::iconsize.h;
	}
	while (offset.y > 0) {
		offset.y -= env_t::iconsize.h;
	}
	if( tool_icon_disp_start > tool_icon_disp_end ) {
		tool_icon_disp_start = 0;
		offset.x = 0;
		offset.y = 0;
	}
	int xy = tool_icon_width*tool_icon_height;
	tool_icon_disp_end = (tool_icon_height == 1) ? min(tool_icon_disp_start+xy+(offset.x!=0), tools.get_count()) : min(tool_icon_disp_start + xy + (offset.y != 0), tools.get_count());

	if(IS_LEFTRELEASE(ev)  ||  IS_RIGHTRELEASE(ev)) {
		if( is_dragging ) {
			is_dragging = false;
			if( abs(old_offset.x - offset.x) > 2   ||  abs(old_offset.y - offset.y) > 2  ||  ev->cx-ev->mx != offset.x  ||  ev->cx - ev->my != offset.y  ) {
				// we did dragg sucesfully before, so no tool selection!
				return true;
			}
		}

		// No dragging => Next check tooltips
		const scr_coord icon_off = get_icon_area_offset();
		const int x = (ev->mx-offset.x-icon_off.x) / env_t::iconsize.w;
		const int y = (ev->my-offset.y-D_TITLEBAR_HEIGHT-icon_off.y) / env_t::iconsize.h;

		const int wz_idx = x+(tool_icon_width*y)+tool_icon_disp_start;
		if( wz_idx>=0  &&  wz_idx < (int)tools.get_count()  ) {
			// change tool
			tool_t *tool = tools[wz_idx].tool;
			if(IS_LEFTRELEASE(ev)) {
				if(  env_t::reselect_closes_tool  &&  tool  &&  tool->is_selected()  &&  !IS_CONTROL_PRESSED(ev)  ) {
					// ->exit triggers tool_selector_t::infowin_event in the closing toolbar,
					// which resets active tool to query tool
					if( tool->exit( welt->get_active_player() ) ) {
						welt->set_tool( tool_t::general_tool[TOOL_QUERY], welt->get_active_player() );
					}
				}
				else {
					welt->set_tool( tool, welt->get_active_player() );
				}
			}
			else {
				// right-click on toolbar icon closes toolbars and dialogues. Resets selectable simple and general tools to the query-tool
				if(  tool  &&  tool->is_selected()  ) {
					// ->exit triggers tool_selector_t::infowin_event in the closing toolbar,
					// which resets active tool to query tool
					if(  tool->exit(welt->get_active_player())  ) {
						welt->set_tool( tool_t::general_tool[TOOL_QUERY], welt->get_active_player() );
					}
				}
			}
			return true;
		}
	}
	// this resets to query-tool, when closing toolsbar - but only for selected general tools in the closing toolbar
	else if(ev->ev_class==INFOWIN &&  ev->ev_code==WIN_CLOSE) {
		FOR(vector_tpl<tool_data_t>, const i, tools) {
			if (i.tool->is_selected() && i.tool->get_id() & GENERAL_TOOL) {
				welt->set_tool( tool_t::general_tool[TOOL_QUERY], welt->get_active_player() );
				break;
			}
		}
	}
	// reset title, language may have changed
	else if(ev->ev_class==INFOWIN  &&  (ev->ev_code==WIN_TOP  ||  ev->ev_code==WIN_OPEN) ) {
		set_name( translator::translate(title) );
	}

	if(IS_WINDOW_CHOOSE_NEXT(ev)) {
		int xy = tool_icon_width*tool_icon_height;
		if(ev->ev_code==NEXT_WINDOW) {
			assert( xy >= tool_icon_width );
			tool_icon_disp_start += xy;
			if(  tool_icon_disp_start + xy > (int)tools.get_count() ) {
				tool_icon_disp_start = tools.get_count() - xy;
			}
			offset = scr_coord( 0, 0 );
		}
		else {
			if(  tool_icon_disp_start > xy  ) {
				tool_icon_disp_start -= xy;
			}
			else {
				tool_icon_disp_start = 0;
			}
			offset = scr_coord( 0, 0 );
		}
		tool_icon_disp_end = min(tool_icon_disp_start+xy, tools.get_count());
		dirty = true;
	}
	return false;
}


void tool_selector_t::draw(scr_coord pos, scr_size sz)
{
	if(  env_t::iconsize.w <= 0  ||  env_t::iconsize.h <= 0  ) {
		// icons disabled (icon_height<=0): skip all layout/draw math below, since
		// it divides by iconsize and would otherwise crash. This runs every frame
		// for the main menubar (toolbar_id==0), regardless of how many tools were
		// ever added, so the guard must be unconditional and come first.
		has_prev_next = false;
		dirty = false;
		unset_dirty();
		return;
	}

	player_t *player = welt->get_active_player();

	if( toolbar_id == 0 ) {
		// checks for main menu (since it can change during changing layout)
		if(env_t::menupos==MENU_TOP || env_t::menupos == MENU_BOTTOM) {
			offset.y = 0;
			allow_break = false;
			// floor, not ceiling: a ceiling-rounded column count includes one column
			// that doesn't fully fit on screen. During a classic pixel-drag that's
			// fine (it's meant to peek in/out via offset.x), but the scrollbar always
			// lands on offset.x==0, so with a ceiling count the icon in that
			// partial last column would render partly or fully off-screen - lost -
			// whenever the scrollbar reached its own (correctly, floor-based) max
			tool_icon_width = max( 1, display_get_width() / env_t::iconsize.w );
			tool_icon_height = 1; // only single row for title bar
			set_windowsize(sz);
			// check for too large values (acter changing width etc.)
			if (  display_get_width() >= (int)tools.get_count() * env_t::iconsize.w  ) {
				tool_icon_disp_start = 0;
				offset.x = 0;
			}
			else {
				scr_coord_val wx = (tools.get_count() - tool_icon_disp_start + 1) * env_t::iconsize.w + offset.x;
				if (wx < display_get_width()) {
					// snap to the true last page: this used to compare
					// tool_icon_disp_end against tool_icon_height, which is always 1
					// for this (single-row) branch, so the condition was never
					// meaningfully false and the snap target used a stale
					// tool_icon_disp_end instead of the current tool_icon_width -
					// disagreeing with the scrollbar's own max-position math
					// (get_scroll_metrics) and making the scrollbar unable to reach
					// the true end when disp_start landed near, but not exactly on,
					// this snap point (e.g. with empty/separator slots near the tail)
					tool_icon_disp_start = (uint16)max( 0, (int)tools.get_count() - (int)tool_icon_width );
					offset.x = display_get_width() - (tools.get_count() - tool_icon_disp_start) * env_t::iconsize.w;
				}
			}
			has_prev_next = (int)tools.get_count() * env_t::iconsize.w > sz.w;
			// keep disp_end consistent with whatever disp_start ended up as above,
			// regardless of which branch set it (reset-to-0, the end-snap, or left
			// untouched from a scrollbar/wheel event handled earlier this frame)
			tool_icon_disp_end = min( (uint32)tool_icon_disp_start + (uint32)tool_icon_width*tool_icon_height, (uint32)tools.get_count() );
		}
		else {
			offset.x = 0;
			allow_break = false;
			tool_icon_width = 1;
			// only single column for title bar; floor (not ceiling) for the same
			// reason as tool_icon_width above - avoids a partial last row landing
			// off-screen once the scrollbar (offset.y==0) reaches its own max
			tool_icon_height = max( 1, (display_get_height() - win_get_statusbar_height()) / env_t::iconsize.h );
			// sz already carries menu_scrollbar_thickness in its width iff the
			// icons actually overflow (see get_main_menu_scrollbar_extra()); using
			// it directly (rather than always reserving the thickness) keeps it
			// consistent with the horizontal branch above and with win_display_flush
			set_windowsize(sz);

			if ( display_get_height() >= (int)tools.get_count() * env_t::iconsize.h  ) {
				tool_icon_disp_start = 0;
				offset.y = 0;
			}
			else {
				scr_coord_val hx = (tools.get_count() - tool_icon_disp_start + 1) * env_t::iconsize.h + offset.y;
				if (hx < display_get_height()) {
					// snap to the true last page (matches get_scroll_metrics's own
					// max-position math, so it agrees with the scrollbar instead of
					// fighting it near the end)
					tool_icon_disp_start = (uint16)max( 0, (int)tools.get_count() - (int)tool_icon_height );
					offset.y = display_get_height() - (tools.get_count() - tool_icon_disp_start) * env_t::iconsize.h;
				}
			}

			has_prev_next = (int)tools.get_count() * env_t::iconsize.h > sz.h;
			// keep disp_end consistent with whatever disp_start ended up as above,
			// regardless of which branch set it (reset-to-0, the end-snap, or left
			// untouched from a scrollbar/wheel event handled earlier this frame)
			tool_icon_disp_end = min( (uint32)tool_icon_disp_start + (uint32)tool_icon_width*tool_icon_height, (uint32)tools.get_count() );
		}
	}

	// (0,0) unless this is the main menubar with a reserved scrollbar strip
	// on its inner side (MENU_BOTTOM/MENU_RIGHT); see get_icon_area_offset()
	const scr_coord icon_off = get_icon_area_offset();

	if(  tool_icon_disp_start != last_tool_icon_disp_start  ) {
		// the visible tools shifted (drag, wheel, scrollbar-drag, or a window-resize
		// auto-adjust) since the last draw: a cell may now show a different tool, and
		// an IMG_EMPTY one is simply skipped below rather than redrawn, so without this
		// the previous tool's icon would be left on screen at that cell. Re-render the
		// whole icon+scrollbar area instead of relying on per-icon dirty marking.
		mark_rect_dirty_wc( pos.x, pos.y+D_TITLEBAR_HEIGHT, pos.x+sz.w, pos.y+sz.h+D_TITLEBAR_HEIGHT );
		dirty = true;
		last_tool_icon_disp_start = tool_icon_disp_start;
	}

	for(  uint i = tool_icon_disp_start;  i < tool_icon_disp_end;  i++  ) {
		const image_id icon_img = tools[i].tool->get_icon(player);
#if COLOUR_DEPTH != 0
		const scr_coord_val additional_xoffset = icon_off.x + ( (i-tool_icon_disp_start)%(tool_icon_width+(offset.x!=0)) )*env_t::iconsize.w;
		const scr_coord_val additional_yoffset = icon_off.y + D_TITLEBAR_HEIGHT+( (i-tool_icon_disp_start)/(tool_icon_width+(offset.x!=0)) )*env_t::iconsize.h;
#else
		const scr_coord_val additional_xoffset = icon_off.x;
		const scr_coord_val additional_yoffset = icon_off.y;
#endif
		const scr_coord draw_pos = pos + offset + scr_coord(additional_xoffset, additional_yoffset);
		const char *param = tools[i].tool->get_default_param();

		// we don't draw in main menu as it is already made in simwin.cc
		// no background if separator starts with "-b" and has an icon defined
		if(  toolbar_id>0  &&  !(strstart((param==NULL)? "" : param, "-b"))  ) {
			if(  skinverwaltung_t::toolbar_background  &&  skinverwaltung_t::toolbar_background->get_image_id(toolbar_id) != IMG_EMPTY  ) {
				const image_id back_img = skinverwaltung_t::toolbar_background->get_image_id(toolbar_id);
				display_fit_img_to_width( back_img, env_t::iconsize.w );
				display_color_img( back_img, draw_pos.x, draw_pos.y, welt->get_active_player_nr(), false, true );
			}
			else {
				display_fillbox_wh_clip_rgb( draw_pos.x, draw_pos.y, env_t::iconsize.w, env_t::iconsize.h, color_idx_to_rgb(MN_GREY2), false );
			}
		}

		// if there's no image we simply skip, button will be transparent showing toolbar background
		if(  icon_img != IMG_EMPTY  ) {
			bool tool_dirty = dirty  ||  (tools[i].tool->is_selected() ^ tools[i].selected);
			display_fit_img_to_width( icon_img, env_t::iconsize.w );
			display_color_img(icon_img, draw_pos.x, draw_pos.y, player->get_player_nr(), false, tool_dirty);
			tools[i].tool->draw_after( draw_pos, tool_dirty);
			// store whether tool was selected
			tools[i].selected = tools[i].tool->is_selected();
		}
	}

	if( is_dragging ) {
		mark_rect_dirty_wc(pos.x, pos.y+D_TITLEBAR_HEIGHT, pos.x+sz.w, pos.y+sz.h+D_TITLEBAR_HEIGHT );
	}
	else if(  dirty  &&  (tool_icon_disp_end-tool_icon_disp_start < tool_icon_width*tool_icon_height)  ) {
		// mark empty space empty
		mark_rect_dirty_wc(pos.x+icon_off.x, pos.y+icon_off.y, pos.x+icon_off.x + tool_icon_width*env_t::iconsize.w, pos.y+icon_off.y + tool_icon_height*env_t::iconsize.h);
	}

	if(  offset.x != 0  &&  tool_icon_disp_start > 0  ) {
		display_color_img(gui_theme_t::arrow_button_left_img[0], pos.x+icon_off.x, pos.y+D_TITLEBAR_HEIGHT+icon_off.y, 0, false, false);
	}
	if(  offset.y != 0  &&  tool_icon_disp_start > 0  ) {
		display_color_img(gui_theme_t::arrow_button_up_img[0], pos.x+icon_off.x, pos.y+D_TITLEBAR_HEIGHT+icon_off.y, 0, false, false);
	}
	if(  tool_icon_height == 1  &&  (tool_icon_disp_start+tool_icon_width < tools.get_count()  ||  (-offset.x) < env_t::iconsize.w*tool_icon_width-get_windowsize().w)  ) {
		display_color_img( gui_theme_t::arrow_button_right_img[0], pos.x+sz.w-D_ARROW_UP_WIDTH, pos.y+D_TITLEBAR_HEIGHT+icon_off.y, 0, false, false );
	}
	if(  tool_icon_width == 1  &&  (tool_icon_disp_start+tool_icon_height < tools.get_count()  ||  (-offset.y) < env_t::iconsize.h*tool_icon_height-get_windowsize().h)  ) {
		// anchored to the icon column's own width, not sz.w, since sz.w may now
		// include the extra scrollbar strip reserved beside a vertical toolbar
		display_color_img(gui_theme_t::arrow_button_down_img[0], pos.x+icon_off.x+tool_icon_width*env_t::iconsize.w-D_ARROW_DOWN_WIDTH, pos.y+D_TITLEBAR_HEIGHT+sz.h-D_ARROW_DOWN_HEIGHT, 0, false, false);
	}

	// scrollbar strip, drawn just outside the icon area (below a single row,
	// beside a column/grid) rather than overlaid on the icons themselves;
	// shown for both the main menubar and popup icon-list windows
	if(  has_prev_next  ) {
		const scr_rect track = get_scrollbar_rect();
		bool horizontal;
		sint32 unit, visible_units, total_units;
		get_scroll_metrics( horizontal, unit, visible_units, total_units );
		const sint32 cur_unit = unit>0 ? tool_icon_disp_start/unit : 0;

		if(  horizontal  ) {
			const scr_coord_val thumb_w = max( (scr_coord_val)8, (scr_coord_val)( (sint64)track.w * visible_units / max(1,total_units) ) );
			const scr_coord_val avail = track.w - thumb_w;
			const scr_coord_val thumb_x = (avail>0  &&  total_units>visible_units) ? clamp( (scr_coord_val)( (sint64)avail * cur_unit / (total_units-visible_units) ), (scr_coord_val)0, avail ) : 0;
			display_fillbox_wh_clip_rgb( pos.x+track.x, pos.y+track.y, track.w, track.h, color_idx_to_rgb(MN_GREY1), true );
			display_fillbox_wh_clip_rgb( pos.x+track.x+thumb_x, pos.y+track.y, thumb_w, track.h, color_idx_to_rgb(MN_GREY4), true );
		}
		else {
			const scr_coord_val thumb_h = max( (scr_coord_val)8, (scr_coord_val)( (sint64)track.h * visible_units / max(1,total_units) ) );
			const scr_coord_val avail = track.h - thumb_h;
			const scr_coord_val thumb_y = (avail>0  &&  total_units>visible_units) ? clamp( (scr_coord_val)( (sint64)avail * cur_unit / (total_units-visible_units) ), (scr_coord_val)0, avail ) : 0;
			display_fillbox_wh_clip_rgb( pos.x+track.x, pos.y+track.y, track.w, track.h, color_idx_to_rgb(MN_GREY1), true );
			display_fillbox_wh_clip_rgb( pos.x+track.x, pos.y+track.y+thumb_y, track.w, thumb_h, color_idx_to_rgb(MN_GREY4), true );
		}
	}

	if(  !is_dragging  ) {
		// tooltips?
		const sint16 mx = get_mouse_x();
		const sint16 my = get_mouse_y();
		if(  is_hit(mx-pos.x, my-pos.y)  ) {
			const sint16 xdiff = (mx - pos.x - icon_off.x) / env_t::iconsize.w;
			const sint16 ydiff = (my - pos.y-D_TITLEBAR_HEIGHT-icon_off.y) / env_t::iconsize.h;
			if(  xdiff>=0  &&  xdiff<tool_icon_width  &&  ydiff>=0  ) {
				const int tipnr = xdiff+(tool_icon_width*ydiff)+tool_icon_disp_start;
				if(  tipnr < (int)tool_icon_disp_end  ) {
					const char* tipstr = tools[tipnr].tool->get_tooltip(welt->get_active_player());
					win_set_tooltip(mx+TOOLTIP_MOUSE_OFFSET_X, my+TOOLTIP_MOUSE_OFFSET_Y, tipstr, tools[tipnr].tool, this);
				}
			}
		}
	}

	dirty = false;
	//as we do not call gui_frame_t::draw, we reset dirty flag explicitly
	unset_dirty();
}



bool tool_selector_t::empty(player_t *player) const
{
	FOR(vector_tpl<tool_data_t>, w, tools) {
		if (w.tool->get_icon(player) != IMG_EMPTY) {
			return false;
		}
	}
	return true;
}
