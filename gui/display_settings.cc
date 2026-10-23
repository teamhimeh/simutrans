/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "display_settings.h"
#include "../simdebug.h"
#include "../simworld.h"
#include "../display/simimg.h"
#include "../simintr.h"
#include "../simcolor.h"
#include "../boden/wege/strasse.h"
#include "../dataobj/settings.h"
#include "../dataobj/environment.h"
#include "../dataobj/translator.h"
#include "../obj/baum.h"
#include "../obj/zeiger.h"
#include "../display/simgraph.h"
#include "../simmenu.h"
#include "../player/simplay.h"
#include "../utils/simstring.h"
#include "../sys/simsys.h"

#include "gui_theme.h"
#include "themeselector.h"
#include "loadfont_frame.h"
#include "simwin.h"
#include "base_info.h"

// display text label in player colors
void display_text_label(sint16 xpos, sint16 ypos, const char* text, const player_t *player, bool dirty); // grund.cc

enum {
	IDBTN_SCROLL_INVERSE,
	IDBTN_IGNORE_NUMLOCK,
	IDBTN_PEDESTRIANS_AT_STOPS,
	IDBTN_PEDESTRIANS_IN_TOWNS,
	IDBTN_DAY_NIGHT_CHANGE,
	IDBTN_TRANSPARENT_INSTEAD_OF_HIDDEN,
	IDBTN_HIDE_TREES,
	IDBTN_TRANSPARENT_STATION_COVERAGE,
	IDBTN_SHOW_STATION_COVERAGE,
	IDBTN_UNDERGROUND_VIEW,
	IDBTN_SHOW_GRID,
	IDBTN_SHOW_STATION_NAMES_ARROW,
	IDBTN_SHOW_WAITING_BARS,
	IDBTN_SHOW_ALLOWED_PLAYERS,
	IDBTN_SHOW_SLICE_MAP_VIEW,
	IDBTN_HIDE_BUILDINGS,
	IDBTN_SHOW_SCHEDULES_STOP,
	IDBTN_SHOW_THEMEMANAGER,
	IDBTN_SIMPLE_DRAWING,
	IDBTN_CHANGE_FONT,
	IDBTN_INFINITE_SCROLL,
	IDBTN_RIBI_ARROW,
	IDBTN_ONEWAY_RIBI_ONLY,
	IDBTN_SHOW_LINE_COLOR,
	IDBTN_SHOW_CONVOY_LOADINGLEVEL,
	IDBTN_SHOW_WAY_OFFSET_LABEL,
	IDBTN_SHOW_ONLY_OWN_VEHICLE_STATES,
	COLORS_MAX_BUTTONS, 
};

static button_t buttons[COLORS_MAX_BUTTONS];

/**
* class to visualize station names
	IDBTN_SHOW_FACTORY_STORAGE,
*/
class gui_label_stationname_t : public gui_label_t
{
	karte_ptr_t welt;
public:
	gui_label_stationname_t(const char* text) : gui_label_t(text) {}

	void draw(scr_coord offset) OVERRIDE
	{
		scr_coord p = pos + offset;

		const player_t* player = welt->get_active_player();
		const char *text = get_text_pointer();

		display_text_label(p.x, p.y, text, player, true);
	}

	scr_size get_min_size() const OVERRIDE
	{
		return gui_label_t::get_min_size() + scr_size(LINESPACE + D_H_SPACE, 4);
	}
};


gui_settings_t::gui_settings_t()
{
	set_table_layout( 1, 0 );
	// Show thememanager
	buttons[ IDBTN_SHOW_THEMEMANAGER ].init( button_t::roundbox_state | button_t::flexible, "Select a theme for display" );
	add_component( buttons + IDBTN_SHOW_THEMEMANAGER );

	// Change font
	buttons[ IDBTN_CHANGE_FONT ].init( button_t::roundbox_state | button_t::flexible, "Select display font" );
	add_component( buttons + IDBTN_CHANGE_FONT );

	// screen scale number input
	add_table(3, 0);
	set_alignment(ALIGN_LEFT);
	{
		new_component<gui_label_t>("Screen scale: ");

		screen_scale_numinp.init(dr_get_screen_scale(), 25, 400, 25);
		screen_scale_numinp.add_listener(this);
		add_component(&screen_scale_numinp);

		screen_scale_auto.init(button_t::roundbox_state, "Auto");
		screen_scale_auto.add_listener(this);
		add_component(&screen_scale_auto);
	}
	end_table();
	
	// add controls to info container
	add_table(2,5);
	set_alignment(ALIGN_LEFT);

	// position of menu
	new_component<gui_label_t>("Toolbar position:");
	switch (env_t::menupos) {
		case MENU_TOP: toolbar_pos.init(button_t::arrowup, NULL); break;
		case MENU_LEFT: toolbar_pos.init(button_t::arrowleft, NULL); break;
		case MENU_BOTTOM: toolbar_pos.init(button_t::arrowdown, NULL); break;
		case MENU_RIGHT: toolbar_pos.init(button_t::arrowright, NULL); break;
	}
	add_component(&toolbar_pos);

	fullscreen.init( button_t::square_state, "Fullscreen (changed after restart)" );
	fullscreen.pressed = ( dr_get_fullscreen() == FULLSCREEN );
	fullscreen.enable(dr_has_fullscreen());
	add_component( &fullscreen, 2 );

	borderless.init( button_t::square_state, "Borderless (disabled on fullscreen)" );
	borderless.enable ( dr_get_fullscreen() != FULLSCREEN );
	borderless.pressed = ( dr_get_fullscreen() == BORDERLESS );
	add_component( &borderless, 2 );

	reselect_closes_tool.init( button_t::square_state, "Reselect closes tools" );
	reselect_closes_tool.pressed = env_t::reselect_closes_tool;
	add_component( &reselect_closes_tool, 2 );
	
	put_below_others.init( button_t::square_state, "Put new toolbar below others" );
	put_below_others.pressed = env_t::put_new_toolbar_below_others;
	add_component( &put_below_others, 2 );

	// Frame time label
	new_component<gui_label_t>("Frame time:");
	frame_time_value_label.buf().printf(" 9999 ms");
	frame_time_value_label.update();
	add_component( &frame_time_value_label );
	// Idle time label
	new_component<gui_label_t>("Idle:");
	idle_time_value_label.buf().printf(" 9999 ms");
	idle_time_value_label.update();
	add_component( &idle_time_value_label );
	// FPS label
	new_component<gui_label_t>("FPS:");
	fps_value_label.buf().printf(" 99.9 fps");
	fps_value_label.update();
	add_component( &fps_value_label );
	// Simloops label
	new_component<gui_label_t>("Sim:");
	simloops_value_label.buf().printf(" 999.9");
	simloops_value_label.update();
	add_component( &simloops_value_label );
}

void gui_settings_t::draw(scr_coord offset)
{
	// Update label buffers
	frame_time_value_label.buf().printf(" %d ms", get_frame_time() );
	frame_time_value_label.update();
	idle_time_value_label.buf().printf(" %d ms", world()->get_idle_time() );
	idle_time_value_label.update();

	// fps_label
	uint32 target_fps = world()->is_fast_forward() ? env_t::ff_fps : env_t::fps;
	uint32 loops = world()->get_realFPS();
	PIXVAL color = SYSCOL_TEXT_HIGHLIGHT;
	if(  loops < (target_fps*16*3)/4  ) {
		color = color_idx_to_rgb(( loops <= target_fps*16/2 ) ? COL_RED : COL_YELLOW);
	}
	fps_value_label.set_color(color);
	fps_value_label.buf().printf(" %d fps", loops/16 );
#if MSG_LEVEL >= 3
	if(  env_t::simple_drawing  ) {
		fps_value_label.buf().append( "*" );
	}
#endif
	fps_value_label.update();

	//simloops_label
	loops = world()->get_simloops();
	color = SYSCOL_TEXT_HIGHLIGHT;
	if(  loops <= 30  ) {
		color = color_idx_to_rgb((loops<=20) ? COL_RED : COL_YELLOW);
	}
	simloops_value_label.set_color(color);
	simloops_value_label.buf().printf(" %d%c%d", loops/10, get_fraction_sep(), loops%10 );
	simloops_value_label.update();

	// All components are updated, now draw them...
	gui_aligned_container_t::draw(offset);
}


bool gui_settings_t::action_triggered(gui_action_creator_t *comp, value_t)
{
	if (comp == &screen_scale_numinp) {
		const sint16 new_value = screen_scale_numinp.get_value();
		env_t::display_scale_percent = new_value;
		dr_set_screen_scale(new_value);
	}
	else if (comp == &screen_scale_auto) {
		dr_set_screen_scale(-1);
		const sint16 screen_scale = dr_get_screen_scale();
		env_t::display_scale_percent = screen_scale;
		screen_scale_numinp.set_value(screen_scale);
	}

	return true;
}


map_settings_t::map_settings_t()
{
	set_table_layout( 2, 0 );

	// Show grid checkbox
	buttons[ IDBTN_SHOW_GRID ].init( button_t::square_state, "show grid" );
	add_component( buttons + IDBTN_SHOW_GRID, 2 );

	// Underground view checkbox
	buttons[ IDBTN_UNDERGROUND_VIEW ].init( button_t::square_state, "underground mode" );
	add_component( buttons + IDBTN_UNDERGROUND_VIEW, 2 );

	// Show slice map view checkbox
	buttons[ IDBTN_SHOW_SLICE_MAP_VIEW ].init( button_t::square_state, "sliced underground mode" );
	add_component( buttons + IDBTN_SHOW_SLICE_MAP_VIEW );

	// underground slice edit
	inp_underground_level.set_value( grund_t::underground_mode == grund_t::ugm_level ? grund_t::underground_level : world()->get_zeiger()->get_pos().z );
	inp_underground_level.set_limits( world()->get_groundwater() - 10, 32 );
	inp_underground_level.add_listener( this );
	add_component( &inp_underground_level );

	// Day/night change checkbox
	buttons[ IDBTN_DAY_NIGHT_CHANGE ].init( button_t::square_state, "8WORLD_CHOOSE" );
	add_component( buttons + IDBTN_DAY_NIGHT_CHANGE, 2 );

	// Brightness label
	new_component<gui_label_t>( "1LIGHT_CHOOSE" );

	// brightness edit
	brightness.set_value( env_t::daynight_level );
	brightness.set_limits( 0, 9 );
	brightness.add_listener( this );
	add_component( &brightness );

	// Scroll inverse checkbox
	buttons[ IDBTN_SCROLL_INVERSE ].init( button_t::square_state, "4LIGHT_CHOOSE" );
	add_component( buttons + IDBTN_SCROLL_INVERSE, 2 );

	// Scroll infinite checkbox
	buttons[IDBTN_INFINITE_SCROLL].init(button_t::square_state, "Infinite mouse scrolling");
	buttons[IDBTN_INFINITE_SCROLL].set_tooltip("Infinite scrolling using mouse");
	add_component(buttons + IDBTN_INFINITE_SCROLL, 2);

	// Numpad key
	buttons[ IDBTN_IGNORE_NUMLOCK ].init( button_t::square_state, "Num pad keys always move map" );
	buttons[ IDBTN_IGNORE_NUMLOCK ].pressed = env_t::numpad_always_moves_map;
	add_component( buttons + IDBTN_IGNORE_NUMLOCK, 2 );

	// Scroll speed label
	new_component<gui_label_t>( "3LIGHT_CHOOSE" );

	// Scroll speed edit
	scrollspeed.set_value( abs( env_t::scroll_multi ) );
	scrollspeed.set_limits( 1, 9 );
	scrollspeed.add_listener( this );
	add_component( &scrollspeed );

#ifdef DEBUG
	// Toggle simple drawing for debugging
	buttons[IDBTN_SIMPLE_DRAWING].init(button_t::square_state, "Simple drawing");
	add_component(buttons+IDBTN_SIMPLE_DRAWING, 2);
#endif

	// Set date format
	new_component<gui_label_t>( "Date format" );
	time_setting.set_focusable( false );
	uint8 old_show_month = env_t::show_month;
	sint32 current_tick = world()->get_ticks();
	for( env_t::show_month = 0; env_t::show_month<8; env_t::show_month++ ) {
		tstrncpy( time_str[env_t::show_month], tick_to_string( current_tick ), 64 );
		time_setting.new_component<gui_scrolled_list_t::const_text_scrollitem_t>( time_str[env_t::show_month], SYSCOL_TEXT );
	}
	env_t::show_month = old_show_month;
	time_setting.set_selection( old_show_month );
	add_component( &time_setting );
	time_setting.add_listener( this );
}

bool map_settings_t::action_triggered( gui_action_creator_t *comp, value_t v )
{
	// Brightness edit
	if( &brightness == comp ) {
		env_t::daynight_level = (sint8)v.i;
	}
	// Scroll speed edit
	else if( &scrollspeed == comp ) {
		env_t::scroll_multi = (sint16)(buttons[ IDBTN_SCROLL_INVERSE ].pressed ? -v.i : v.i);
	}
	// underground slice edit
	else if( comp == &inp_underground_level ) {
		if( grund_t::underground_mode == grund_t::ugm_level ) {
			grund_t::underground_level = inp_underground_level.get_value();

			// calc new images
			world()->update_underground();
		}
	}
	else if( comp == &time_setting ) {
		env_t::show_month = v.i;
		return true;
	}
	return true;
}

transparency_settings_t::transparency_settings_t()
{
	set_table_layout( 2, 0 );

	// Transparent instead of hidden checkbox
	buttons[ IDBTN_TRANSPARENT_INSTEAD_OF_HIDDEN ].init( button_t::square_state, "hide transparent" );
	add_component( buttons + IDBTN_TRANSPARENT_INSTEAD_OF_HIDDEN, 2 );

	// Hide trees checkbox
	buttons[ IDBTN_HIDE_TREES ].init( button_t::square_state, "hide trees" );
	add_component( buttons + IDBTN_HIDE_TREES, 2 );

	// Hide buildings
	hide_buildings.set_focusable( false );
	hide_buildings.new_component<gui_scrolled_list_t::const_text_scrollitem_t>( translator::translate( "no buildings hidden" ), SYSCOL_TEXT );
	hide_buildings.new_component<gui_scrolled_list_t::const_text_scrollitem_t>( translator::translate( "hide city building" ), SYSCOL_TEXT );
	hide_buildings.new_component<gui_scrolled_list_t::const_text_scrollitem_t>( translator::translate( "hide all building" ), SYSCOL_TEXT );
	hide_buildings.set_selection( env_t::hide_buildings );
	add_component( &hide_buildings, 2 );
	hide_buildings.add_listener( this );

	buttons[ IDBTN_HIDE_BUILDINGS ].set_tooltip( "hide objects under cursor" );
	buttons[ IDBTN_HIDE_BUILDINGS ].init( button_t::square_state, "Smart hide objects" );
	add_component( buttons + IDBTN_HIDE_BUILDINGS );

	// Smart hide objects edit
	cursor_hide_range.set_value( env_t::cursor_hide_range );
	cursor_hide_range.set_limits( 0, 10 );
	cursor_hide_range.add_listener( this );
	add_component( &cursor_hide_range );

	new_component<gui_label_t>( "Industry overlay" )->set_tooltip( translator::translate( "Display bars above factory to show the status" ) );
	factory_tooltip.set_focusable( false );
	factory_tooltip.new_component<gui_scrolled_list_t::const_text_scrollitem_t>( translator::translate( "Do not show" ), SYSCOL_TEXT );
	factory_tooltip.new_component<gui_scrolled_list_t::const_text_scrollitem_t>( translator::translate( "On mouseover" ), SYSCOL_TEXT );
	factory_tooltip.new_component<gui_scrolled_list_t::const_text_scrollitem_t>( translator::translate( "Served by me" ), SYSCOL_TEXT );
	factory_tooltip.new_component<gui_scrolled_list_t::const_text_scrollitem_t>( translator::translate( "Show always" ), SYSCOL_TEXT );
	factory_tooltip.set_selection( env_t::show_factory_storage_bar );
	add_component( &factory_tooltip );
	factory_tooltip.add_listener( this );

	new_component<gui_label_t>( "Clip below elevated ways" );
	clip_below_setting.set_focusable( false );
	clip_below_setting.new_component<gui_scrolled_list_t::const_text_scrollitem_t>( translator::translate( "no cut" ), SYSCOL_TEXT );
	clip_below_setting.new_component<gui_scrolled_list_t::const_text_scrollitem_t>( translator::translate( "all cut" ), SYSCOL_TEXT );
	clip_below_setting.new_component<gui_scrolled_list_t::const_text_scrollitem_t>( translator::translate( "pak dependence" ), SYSCOL_TEXT );
	clip_below_setting.set_selection( env_t::clip_below );
	add_component( &clip_below_setting );
	clip_below_setting.add_listener( this );
}

bool transparency_settings_t::action_triggered( gui_action_creator_t *comp, value_t v )
{
	// Smart hide objects edit
	if( &cursor_hide_range == comp ) {
		env_t::cursor_hide_range = cursor_hide_range.get_value();
	}
	// Hide building
	if( &hide_buildings == comp ) {
		env_t::hide_buildings = (uint8)v.i;
		world()->set_dirty();
	}
	if( comp == &factory_tooltip ) {
		env_t::show_factory_storage_bar = (uint8)v.i;
		world()->set_dirty();

		return true;
	}
	if( comp == &clip_below_setting ) {
		env_t::clip_below = (sint8)v.i;
		world()->set_dirty();
		return true;
	}
	return true;
}


void transparency_settings_t::draw( scr_coord offset )
{
	hide_buildings.set_selection( env_t::hide_buildings );
	clip_below_setting.set_selection( env_t::clip_below );

	gui_aligned_container_t::draw(offset);
}


station_settings_t::station_settings_t()
{
	set_table_layout( 2, 0 );

	// Transparent station coverage
	buttons[ IDBTN_TRANSPARENT_STATION_COVERAGE ].init( button_t::square_state, "transparent station coverage" );
	buttons[ IDBTN_TRANSPARENT_STATION_COVERAGE ].pressed = env_t::use_transparency_station_coverage;
	add_component( buttons + IDBTN_TRANSPARENT_STATION_COVERAGE, 2 );

	// Show station coverage
	buttons[ IDBTN_SHOW_STATION_COVERAGE ].init( button_t::square_state, "show station coverage" );
	add_component( buttons + IDBTN_SHOW_STATION_COVERAGE, 2 );

	// Show station names arrow
	add_table( 2, 1 );
	{
		buttons[ IDBTN_SHOW_STATION_NAMES_ARROW ].set_typ( button_t::arrowright );
		buttons[ IDBTN_SHOW_STATION_NAMES_ARROW ].set_tooltip( "show station names" );
		add_component( buttons + IDBTN_SHOW_STATION_NAMES_ARROW );
		new_component<gui_label_stationname_t>( "show station names" );
	}
	end_table();
	new_component<gui_empty_t>();

	// Show waiting bars checkbox
	buttons[ IDBTN_SHOW_WAITING_BARS ].init( button_t::square_state, "show waiting bars" );
	buttons[ IDBTN_SHOW_WAITING_BARS ].pressed = env_t::show_names & env_t::SHOW_WAITING_BARS;
	add_component( buttons + IDBTN_SHOW_WAITING_BARS, 2 );

	// Show allowed players checkbox
	buttons[ IDBTN_SHOW_ALLOWED_PLAYERS ].init( button_t::square_state, "show allowed players" );
	buttons[ IDBTN_SHOW_ALLOWED_PLAYERS ].pressed = env_t::show_names & env_t::SHOW_ALLOWED_PLAYERS;
	add_component( buttons + IDBTN_SHOW_ALLOWED_PLAYERS, 2 );
}

traffic_settings_t::traffic_settings_t()
{
	set_table_layout( 2, 0 );

	// Pedestrians in towns checkbox
	buttons[IDBTN_PEDESTRIANS_IN_TOWNS].init(button_t::square_state, "6LIGHT_CHOOSE");
	buttons[IDBTN_PEDESTRIANS_IN_TOWNS].pressed = world()->get_settings().get_random_pedestrians();
	add_component(buttons+IDBTN_PEDESTRIANS_IN_TOWNS, 2);

	// Pedestrians at stops checkbox
	buttons[IDBTN_PEDESTRIANS_AT_STOPS].init(button_t::square_state, "5LIGHT_CHOOSE");
	buttons[IDBTN_PEDESTRIANS_AT_STOPS].pressed = world()->get_settings().get_show_pax();
	add_component(buttons+IDBTN_PEDESTRIANS_AT_STOPS, 2);

	// Traffic density label
	new_component<gui_label_t>("6WORLD_CHOOSE");

	// Traffic density edit
	traffic_density.set_value(world()->get_settings().get_traffic_level());
	traffic_density.set_limits( 0, 16 );
	traffic_density.add_listener(this);
	add_component(&traffic_density);

	// Convoy tooltip
	convoy_tooltip.set_focusable(false);
	convoy_tooltip.new_component<gui_scrolled_list_t::const_text_scrollitem_t>(translator::translate("convoi error tooltips"), SYSCOL_TEXT);
	convoy_tooltip.new_component<gui_scrolled_list_t::const_text_scrollitem_t>(translator::translate("convoi mouseover tooltips"), SYSCOL_TEXT);
	convoy_tooltip.new_component<gui_scrolled_list_t::const_text_scrollitem_t>(translator::translate("all convoi tooltips"), SYSCOL_TEXT);
	convoy_tooltip.new_component<gui_scrolled_list_t::const_text_scrollitem_t>(translator::translate("line name tooltips"), SYSCOL_TEXT);
	convoy_tooltip.new_component<gui_scrolled_list_t::const_text_scrollitem_t>(translator::translate("line name and states tooltips"), SYSCOL_TEXT);
	convoy_tooltip.set_selection(env_t::show_vehicle_states);
	add_component(&convoy_tooltip, 2);
	convoy_tooltip.add_listener(this);

	buttons[IDBTN_SHOW_ONLY_OWN_VEHICLE_STATES].init(button_t::square_state, "show only own vehicle states");
	add_component(buttons+IDBTN_SHOW_ONLY_OWN_VEHICLE_STATES,2);

	buttons[IDBTN_SHOW_LINE_COLOR].init(button_t::square_state, "Show line colors");
	add_component(buttons+IDBTN_SHOW_LINE_COLOR,2);
	buttons[IDBTN_SHOW_CONVOY_LOADINGLEVEL].init(button_t::square_state, "Show convoy loading level");
	add_component(buttons+IDBTN_SHOW_CONVOY_LOADINGLEVEL,2);

	// Convoy follow mode
	new_component<gui_label_t>("Convoi following mode");

	follow_mode.set_focusable(false);
	follow_mode.new_component<gui_scrolled_list_t::const_text_scrollitem_t>(translator::translate("None"), SYSCOL_TEXT);
	follow_mode.new_component<gui_scrolled_list_t::const_text_scrollitem_t>(translator::translate("underground mode"), SYSCOL_TEXT);
	follow_mode.new_component<gui_scrolled_list_t::const_text_scrollitem_t>(translator::translate("sliced underground mode"), SYSCOL_TEXT);
	follow_mode.set_selection(env_t::follow_convoi_underground);
	add_component(&follow_mode);
	follow_mode.add_listener(this);

	// Show schedule's stop checkbox
	buttons[IDBTN_SHOW_SCHEDULES_STOP].init( button_t::square_state ,  "Highlite schedule" );
	add_component(buttons+IDBTN_SHOW_SCHEDULES_STOP, 2);

	// convoi booking message options
	money_booking.set_focusable( false );
	money_booking.new_component<gui_scrolled_list_t::const_text_scrollitem_t>(translator::translate("Show all revenue messages"), SYSCOL_TEXT );
	money_booking.new_component<gui_scrolled_list_t::const_text_scrollitem_t>(translator::translate("Show only player's revenue"), SYSCOL_TEXT );
	money_booking.new_component<gui_scrolled_list_t::const_text_scrollitem_t>(translator::translate("Show no revenue messages"), SYSCOL_TEXT );
	money_booking.set_selection( env_t::show_money_message );
	add_component(&money_booking, 2);
	money_booking.add_listener(this);
	
	// ribi arrow
	buttons[IDBTN_RIBI_ARROW].init(button_t::square_state, "show connected directions");
	buttons[IDBTN_RIBI_ARROW].pressed = strasse_t::show_masked_ribi;
	add_component(buttons+IDBTN_RIBI_ARROW, 2);
	buttons[IDBTN_ONEWAY_RIBI_ONLY].init(button_t::square_state, "show directions only for oneway roads");
	buttons[IDBTN_ONEWAY_RIBI_ONLY].pressed = env_t::show_oneway_ribi_only;
	add_component(buttons+IDBTN_ONEWAY_RIBI_ONLY, 2);

	// Show way offset label checkbox
	buttons[ IDBTN_SHOW_WAY_OFFSET_LABEL ].init( button_t::square_state, "show way offset label" );
	buttons[ IDBTN_SHOW_WAY_OFFSET_LABEL ].pressed = env_t::show_way_offset_label;
	add_component( buttons + IDBTN_SHOW_WAY_OFFSET_LABEL, 2 );
}

bool traffic_settings_t::action_triggered( gui_action_creator_t *comp, value_t v )
{
	// Traffic density edit
	if( &traffic_density == comp ) {
		if( !env_t::networkmode || world()->get_active_player_nr() == PUBLIC_PLAYER_NR ) {
			static char level[ 16 ];
			sprintf( level, "%li", v.i );
			tool_t::simple_tool[ TOOL_TRAFFIC_LEVEL & 0xFFF ]->set_default_param( level );
			world()->set_tool( tool_t::simple_tool[ TOOL_TRAFFIC_LEVEL & 0xFFF ], world()->get_active_player() );
		}
		else {
			traffic_density.set_value( world()->get_settings().get_traffic_level() );
		}
	}
	// Convoy tooltip
	if( &convoy_tooltip == comp ) {
		env_t::show_vehicle_states = (uint8)v.i;
	}

	if( &follow_mode == comp ) {
		env_t::follow_convoi_underground = (uint8)v.i;
	}

	if( &money_booking == comp ) {
		env_t::show_money_message = (sint8)v.i;
	}
	return true;
}



// Fixed minimum column widths keep every operation aligned. Long descriptions
// can be read in full through the tooltip instead of widening the whole dialog.
class shortcut_cell_t : public gui_label_t
{
	int width_units;
	bool stretch;
public:
	shortcut_cell_t(int units, bool flexible = false) : width_units(units), stretch(flexible) {}
	scr_size get_min_size() const OVERRIDE { return scr_size(D_BUTTON_WIDTH * width_units / 2, D_LABEL_HEIGHT); }
	scr_size get_max_size() const OVERRIDE { return scr_size(stretch ? scr_size::inf.w : get_min_size().w, D_LABEL_HEIGHT); }
};

class shortcut_conflict_window_t : public base_infowin_t, private action_listener_t
{
	button_t acknowledge;
public:
	shortcut_conflict_window_t(const char *message) : base_infowin_t(translator::translate("Key binding conflict"))
	{
		buf.append(message);
		acknowledge.init(button_t::roundbox | button_t::flexible, "Ok");
		acknowledge.add_listener(this);
		add_component(&acknowledge);
		recalc_size();
	}
	bool action_triggered(gui_action_creator_t *, value_t) OVERRIDE
	{
		destroy_win(this);
		return true;
	}
	bool infowin_event(const event_t *ev) OVERRIDE
	{
		// Consume repeated keys while the warning is open.
		if (ev->ev_class == EVENT_KEYBOARD) {
			if (ev->ev_code == SIM_KEY_ESCAPE || ev->ev_code == 13) { destroy_win(this); }
			return true;
		}
		if (ev->ev_class == EVENT_STRING) { return true; }
		return base_infowin_t::infowin_event(ev);
	}
};

class shortcut_row_t
{
public:
	std::string name, key;
	shortcut_cell_t description, binding;
	button_t change, clear;
	shortcut_row_t(uint32 index, player_t *player) : description(5, true), binding(3)
	{
		tool_t *tool = tool_t::get_shortcut_tool(index);
		const char *tooltip = tool->get_tooltip(player);
		name = tooltip && *tooltip ? tooltip : tool_t::get_shortcut_name(index);
		if (strncmp(tool_t::get_shortcut_name(index), "toolbar[", 8) == 0) {
			name += " (" + std::string(translator::translate("Toolbar")) + ")";
		}
		description.set_text_pointer(name.c_str(), false);
		description.set_tooltip(name.c_str());
		change.init(button_t::roundbox, "Change keybind");
		clear.init(button_t::roundbox, "Clear key");
	}
};

shortcut_settings_t::shortcut_settings_t() : rows_scroll(&rows_table), capturing_row(-1)
{
	set_table_layout(1, 0);
	reset.init(button_t::roundbox | button_t::flexible, "Restore default keys");
	reset.add_listener(this);
	add_component(&reset);
	new_component<gui_label_t>("Key changes are saved automatically to menuconf.tab.");
	add_component(&status);
	rows_table.set_table_layout(4, 0);
	rows_table.set_margin(scr_size(0, 0), scr_size(0, 0));
	rows_table.new_component<shortcut_cell_t>(5, true)->set_text("Shortcut action", false);
	rows_table.new_component<shortcut_cell_t>(3)->set_text("Current key", false);
	rows_table.new_component<shortcut_cell_t>(2)->set_text("Change keybind", false);
	rows_table.new_component<shortcut_cell_t>(2)->set_text("Clear key", false);
	for (uint32 i = 0; i < tool_t::get_shortcut_count(); i++) {
		rows.emplace_back(new shortcut_row_t(i, world()->get_active_player()));
		shortcut_row_t &row = *rows.back();
		row.change.add_listener(this);
		row.clear.add_listener(this);
		rows_table.add_component(&row.description);
		rows_table.add_component(&row.binding);
		rows_table.add_component(&row.change);
		rows_table.add_component(&row.clear);
	}
	rows_scroll.set_min_width(rows_table.get_min_size().w + D_SCROLLBAR_WIDTH);
	rows_scroll.set_scroll_amount_y(D_BUTTON_HEIGHT + D_V_SPACE);
	add_component(&rows_scroll);
	refresh();
}

shortcut_settings_t::~shortcut_settings_t() {}

void shortcut_settings_t::refresh()
{
	for (uint32 i = 0; i < rows.size(); i++) {
		shortcut_row_t &row = *rows[i];
		tool_t *tool = tool_t::get_shortcut_tool(i);
		row.key = tool_t::get_shortcut_key_name(tool->command_key, tool->command_flags);
		row.binding.set_text_pointer(row.key.c_str(), false);
		row.binding.set_tooltip(row.key.c_str());
		row.clear.enable(tool->command_key != 0 && tool->command_key != 0xFFFF);
	}
}

void shortcut_settings_t::reload()
{
 cancel_capture();
 const bool ok = tool_t::reload_shortcuts();
 refresh();
 status.set_text(ok ? "" : "Could not read keybindings or prepare their backup.", false);
}

void shortcut_settings_t::draw(scr_coord offset)
{
	status.set_size(scr_size(get_size().w, status.get_size().h));
	gui_aligned_container_t::draw(offset);
}

void shortcut_settings_t::cancel_capture()
{
	if (capturing_row < 0) { return; }
	rows[capturing_row]->change.pressed = false;
	capturing_row = -1;
	status.set_text("", false);
}

bool shortcut_settings_t::action_triggered(gui_action_creator_t *comp, value_t)
{
	cancel_capture();
	if (comp == &reset) {
		const bool ok = tool_t::reset_shortcuts();
		refresh();
		status.set_text(ok ? "Key settings saved." : "Could not save key settings.", false);
		return true;
	}
	for (uint32 i = 0; i < rows.size(); i++) {
		shortcut_row_t &row = *rows[i];
		if (comp == &row.change) {
			capturing_row = (sint32)i;
			row.change.pressed = true;
			status_text = row.name + ": " + translator::translate("Press a key. Esc cancels.");
			status.set_text_pointer(status_text.c_str(), false);
			return true;
		}
		if (comp == &row.clear) {
			const bool ok = tool_t::set_shortcut(i, 0, 0);
			refresh();
			status.set_text(ok ? "Key settings saved." : "Could not save key settings.", false);
			return true;
		}
	}
	return false;
}

bool shortcut_settings_t::capture_event(const event_t *ev)
{
	if (capturing_row < 0) { return false; }
	if (ev->ev_class == EVENT_STRING) { return true; }
	if (ev->ev_class != EVENT_KEYBOARD) { return false; }
	if (ev->ev_code == SIM_KEY_ESCAPE) { cancel_capture(); return true; }
	const uint16 key = (uint16)ev->ev_code;
	const uint8 modifiers = ev->ev_key_mod & (SIM_MOD_SHIFT | SIM_MOD_CTRL);
	const sint32 conflict = tool_t::find_shortcut_conflict(capturing_row, key, modifiers);
	if (!tool_t::is_shortcut_key_allowed(key, modifiers)) {
		status.set_text("This key is reserved by the game.", false);
	}
	else if (conflict >= 0) {
		const std::string message = std::string(translator::translate("Conflicting key:")) + " "
			+ tool_t::get_shortcut_key_name(key, modifiers) + "\n\n"
			+ translator::translate("Key already assigned:") + " " + rows[conflict]->name + "\n\n"
			+ translator::translate("The binding was not changed. Choose another key, or clear the existing binding first.");
		cancel_capture();
		create_win(new shortcut_conflict_window_t(message.c_str()), w_info, magic_none);
	}
	else {
		const bool ok = tool_t::set_shortcut(capturing_row, key, modifiers);
		cancel_capture();
		refresh();
		status.set_text(ok ? "Key settings saved." : "Could not save key settings.", false);
	}
	return true;
}

color_gui_t::color_gui_t() :
	gui_frame_t( translator::translate( "Helligk. u. Farben" ) ),
	scrolly_gui(&gui_settings),
	scrolly_map(&map_settings),
	scrolly_transparency(&transparency_settings),
	scrolly_station(&station_settings),
	scrolly_traffic(&traffic_settings)
{
	set_table_layout( 1, 0 );

	scrolly_gui.set_scroll_amount_y(D_BUTTON_HEIGHT/2);
	scrolly_map.set_scroll_amount_y(D_BUTTON_HEIGHT/2);
	scrolly_transparency.set_scroll_amount_y(D_BUTTON_HEIGHT/2);
	scrolly_station.set_scroll_amount_y(D_BUTTON_HEIGHT/2);
	scrolly_traffic.set_scroll_amount_y(D_BUTTON_HEIGHT/2);

	tabs.add_tab(&scrolly_gui, translator::translate("GUI settings"));
	tabs.add_tab(&scrolly_map, translator::translate("map view"));
	tabs.add_tab(&scrolly_transparency, translator::translate("transparencies"));
	tabs.add_tab(&scrolly_station, translator::translate("station labels"));
	tabs.add_tab(&scrolly_traffic, translator::translate("traffic settings"));
	tabs.add_tab(&shortcut_settings, translator::translate("Keybindings"));
	tabs.add_listener(this);
	add_component(&tabs);

	for( int i = 0; i < COLORS_MAX_BUTTONS; i++ ) {
		buttons[ i ].add_listener( this );
	}
	gui_settings.toolbar_pos.add_listener( this );
	gui_settings.fullscreen.add_listener( this );
	gui_settings.borderless.add_listener( this );
	gui_settings.reselect_closes_tool.add_listener(this);
	gui_settings.put_below_others.add_listener(this);

	set_resizemode(diagonal_resize);
	set_min_windowsize( scr_size(max(D_DEFAULT_WIDTH, shortcut_settings.get_min_size().w + 2 * D_MARGIN_LEFT),get_min_windowsize().h+map_settings.get_size().h) );
	set_windowsize( scr_size(max(D_DEFAULT_WIDTH, shortcut_settings.get_min_size().w + 2 * D_MARGIN_LEFT),get_min_windowsize().h+map_settings.get_size().h) );
	resize( scr_coord( 0, 0 ) );
}

bool color_gui_t::action_triggered( gui_action_creator_t *comp, value_t p)
{
	if (comp == &tabs) {
		shortcut_settings.cancel_capture();
		if (tabs.get_active_tab_index() == 5) { shortcut_settings.reload(); }
		return true;
	}
	if(  comp == &gui_settings.toolbar_pos  ) {
		env_t::menupos++;
		env_t::menupos &= 3;
		switch (env_t::menupos) {
			case MENU_TOP: gui_settings.toolbar_pos.set_typ(button_t::arrowup); break;
			case MENU_LEFT: gui_settings.toolbar_pos.set_typ(button_t::arrowleft); break;
			case MENU_BOTTOM: gui_settings.toolbar_pos.set_typ(button_t::arrowdown); break;
			case MENU_RIGHT: gui_settings.toolbar_pos.set_typ(button_t::arrowright); break;
		}
		welt->set_dirty();

		// move all windows
		event_t* ev = new event_t();
		ev->ev_class = EVENT_SYSTEM;
		ev->ev_code = SYSTEM_RELOAD_WINDOWS;
		queue_event(ev);

		return true;
	}

	if(  comp == &gui_settings.fullscreen  ) {
		env_t::fullscreen = p.i==0 && dr_get_fullscreen()!=2 ? dr_get_fullscreen() : (sint16)FULLSCREEN;
		gui_settings.fullscreen.pressed = !gui_settings.fullscreen.pressed;
		gui_settings.borderless.pressed = false;
		return true;
	}

	if(  comp == &gui_settings.borderless  ) {
		env_t::fullscreen = dr_toggle_borderless();
		gui_settings.borderless.pressed = dr_get_fullscreen();
		gui_settings.fullscreen.pressed = false;
		return true;
	}

	if(  comp == &gui_settings.reselect_closes_tool  ) {
		env_t::reselect_closes_tool = !env_t::reselect_closes_tool;
		gui_settings.reselect_closes_tool.pressed = env_t::reselect_closes_tool;
		return true;
	}
	
	if(  comp == &gui_settings.put_below_others  ) {
		env_t::put_new_toolbar_below_others = !env_t::put_new_toolbar_below_others;
		gui_settings.put_below_others.pressed = env_t::put_new_toolbar_below_others;
		return true;
	}

	int i;
	for(  i=0;  i<COLORS_MAX_BUTTONS  &&  comp!=buttons+i;  i++  ) { }

	switch( i )
	{
	case IDBTN_IGNORE_NUMLOCK:
		env_t::numpad_always_moves_map = !env_t::numpad_always_moves_map;
		buttons[IDBTN_IGNORE_NUMLOCK].pressed = env_t::numpad_always_moves_map;
		break;
	case IDBTN_SCROLL_INVERSE:
		env_t::scroll_multi = -env_t::scroll_multi;
		break;
	case IDBTN_INFINITE_SCROLL:
		env_t::scroll_infinite ^= 1;
		break;
	case IDBTN_PEDESTRIANS_AT_STOPS:
		if( !env_t::networkmode || welt->get_active_player_nr() == PUBLIC_PLAYER_NR ) {
			welt->set_tool( tool_t::simple_tool[ TOOL_TOOGLE_PAX & 0xFFF ], welt->get_active_player() );
		}
		break;
	case IDBTN_PEDESTRIANS_IN_TOWNS:
		if( !env_t::networkmode || welt->get_active_player_nr() == PUBLIC_PLAYER_NR ) {
			welt->set_tool( tool_t::simple_tool[ TOOL_TOOGLE_PEDESTRIANS & 0xFFF ], welt->get_active_player() );
		}
		break;
	case IDBTN_HIDE_TREES:
		env_t::hide_trees = !env_t::hide_trees;
		baum_t::recalc_outline_color();
		break;
	case IDBTN_DAY_NIGHT_CHANGE:
		env_t::night_shift = !env_t::night_shift;
		break;
	case IDBTN_TRANSPARENT_INSTEAD_OF_HIDDEN:
		env_t::hide_with_transparency = !env_t::hide_with_transparency;
		baum_t::recalc_outline_color();
		break;
	case IDBTN_TRANSPARENT_STATION_COVERAGE:
		env_t::use_transparency_station_coverage = !env_t::use_transparency_station_coverage;
		break;
	case IDBTN_SHOW_STATION_COVERAGE:
		env_t::station_coverage_show = env_t::station_coverage_show == 0 ? 0xFF : 0;
		break;
	case IDBTN_UNDERGROUND_VIEW:
		// see simtool.cc::tool_show_underground_t::init
		grund_t::set_underground_mode( buttons[ IDBTN_UNDERGROUND_VIEW ].pressed ? grund_t::ugm_none : grund_t::ugm_all, map_settings.inp_underground_level.get_value() );
		// calc new images
		welt->update_underground();
		// renew toolbar
		tool_t::update_toolbars();
		break;
	case IDBTN_SHOW_GRID:
		grund_t::toggle_grid();
		break;
	case IDBTN_SHOW_STATION_NAMES_ARROW:
		if( env_t::show_names & env_t::SHOW_NAME ) {
			if( env_t::show_names & env_t::SHOW_NAME_TYPE3 ) {
				env_t::show_names &= env_t::bars_settings;
			}
			else if(  env_t::show_names & env_t::SHOW_NAME_TYPE2  ) {
				env_t::show_names &= ~env_t::SHOW_NAME_TYPE2;
				env_t::show_names |= env_t::SHOW_NAME_TYPE3;
			} else {
				env_t::show_names |= env_t::SHOW_NAME_TYPE2;
			}
		}
		else {
			env_t::show_names &= env_t::bars_settings;
			env_t::show_names |= env_t::SHOW_NAME;
		}
		break;
	case IDBTN_SHOW_WAITING_BARS:
		env_t::show_names ^= env_t::SHOW_WAITING_BARS;
		break;
	case IDBTN_SHOW_ALLOWED_PLAYERS:
		env_t::show_names ^= env_t::SHOW_ALLOWED_PLAYERS;
		break;
	case IDBTN_SHOW_WAY_OFFSET_LABEL:
		env_t::show_way_offset_label = !env_t::show_way_offset_label;
		buttons[ IDBTN_SHOW_WAY_OFFSET_LABEL ].pressed = env_t::show_way_offset_label;
		welt->set_dirty();
		break;
	case IDBTN_SHOW_SLICE_MAP_VIEW:
		// see simtool.cc::tool_show_underground_t::init
		grund_t::set_underground_mode( buttons[ IDBTN_SHOW_SLICE_MAP_VIEW ].pressed ? grund_t::ugm_none : grund_t::ugm_level, map_settings.inp_underground_level.get_value() );

		// calc new images
		welt->update_underground();

		// renew toolbar
		tool_t::update_toolbars();
		break;
	case IDBTN_HIDE_BUILDINGS:
		// see simtool.cc::tool_hide_under_cursor_t::init
		env_t::hide_under_cursor = !env_t::hide_under_cursor  &&  env_t::cursor_hide_range > 0;

		// renew toolbar
		tool_t::update_toolbars();
		break;
	case IDBTN_SHOW_SCHEDULES_STOP:
		env_t::visualize_schedule = !env_t::visualize_schedule;
		break;
	case IDBTN_SHOW_THEMEMANAGER:
		create_win( new themeselector_t(), w_info, magic_themes );
		break;
	case IDBTN_SIMPLE_DRAWING:
		env_t::simple_drawing = !env_t::simple_drawing;
		break;
	case IDBTN_CHANGE_FONT:
		create_win( new loadfont_frame_t(), w_info, magic_font );
		break;
	case IDBTN_RIBI_ARROW:
		strasse_t::show_masked_ribi ^= 1;
		break;
	case IDBTN_ONEWAY_RIBI_ONLY:
		env_t::show_oneway_ribi_only ^= 1;
		break;
	case IDBTN_SHOW_LINE_COLOR:
		env_t::show_line_colors ^= 1;
		break;
	case IDBTN_SHOW_CONVOY_LOADINGLEVEL:
		env_t::show_convoy_loadinglevel ^= 1;
		break;
	case IDBTN_SHOW_ONLY_OWN_VEHICLE_STATES:
		env_t::show_only_own_vehicle_states ^= 1;
		break;
	default:
		assert( 0 );
	}

	welt->set_dirty();

	return true;
}


void color_gui_t::draw(scr_coord pos, scr_size size)
{
	// Update button states that was changed with keyboard ...
	buttons[IDBTN_PEDESTRIANS_AT_STOPS].pressed = welt->get_settings().get_show_pax();
	buttons[IDBTN_PEDESTRIANS_IN_TOWNS].pressed = welt->get_settings().get_random_pedestrians();
	buttons[IDBTN_HIDE_TREES].pressed = env_t::hide_trees;
	buttons[IDBTN_HIDE_BUILDINGS].pressed = env_t::hide_under_cursor;
	buttons[IDBTN_SHOW_STATION_COVERAGE].pressed = env_t::station_coverage_show;
	buttons[IDBTN_UNDERGROUND_VIEW].pressed = grund_t::underground_mode == grund_t::ugm_all;
	buttons[IDBTN_SHOW_GRID].pressed = grund_t::show_grid;
	buttons[IDBTN_SHOW_WAITING_BARS].pressed = (env_t::show_names & env_t::SHOW_WAITING_BARS)!=0;
	buttons[IDBTN_SHOW_ALLOWED_PLAYERS].pressed = (env_t::show_names & env_t::SHOW_ALLOWED_PLAYERS)!=0;
	buttons[IDBTN_SHOW_SLICE_MAP_VIEW].pressed = grund_t::underground_mode == grund_t::ugm_level;
	buttons[IDBTN_SHOW_SCHEDULES_STOP].pressed = env_t::visualize_schedule;
	buttons[IDBTN_SIMPLE_DRAWING].pressed = env_t::simple_drawing;
	buttons[IDBTN_SIMPLE_DRAWING].enable(welt->is_paused());
	buttons[IDBTN_SCROLL_INVERSE].pressed = env_t::scroll_multi < 0;
	buttons[IDBTN_INFINITE_SCROLL].pressed = env_t::scroll_infinite;
	buttons[IDBTN_DAY_NIGHT_CHANGE].pressed = env_t::night_shift;
	buttons[IDBTN_SHOW_SLICE_MAP_VIEW].pressed = grund_t::underground_mode == grund_t::ugm_level;
	buttons[IDBTN_UNDERGROUND_VIEW].pressed = grund_t::underground_mode == grund_t::ugm_all;
	buttons[IDBTN_TRANSPARENT_STATION_COVERAGE].pressed = env_t::use_transparency_station_coverage;
	buttons[IDBTN_TRANSPARENT_INSTEAD_OF_HIDDEN].pressed = env_t::hide_with_transparency;
	buttons[IDBTN_SHOW_LINE_COLOR].pressed = env_t::show_line_colors;
	buttons[IDBTN_SHOW_LINE_COLOR].enable(env_t::show_vehicle_states==env_t::LINE_NAME_TOOLTIPS||env_t::show_vehicle_states==env_t::LINE_NAME_AND_STATES_TOOLTIPS);
	buttons[IDBTN_SHOW_CONVOY_LOADINGLEVEL].pressed = env_t::show_convoy_loadinglevel;
	buttons[IDBTN_SHOW_CONVOY_LOADINGLEVEL].enable(env_t::show_vehicle_states==env_t::LINE_NAME_TOOLTIPS||env_t::show_vehicle_states==env_t::LINE_NAME_AND_STATES_TOOLTIPS);
	buttons[IDBTN_SHOW_ONLY_OWN_VEHICLE_STATES].pressed = env_t::show_only_own_vehicle_states;
	buttons[IDBTN_SHOW_ONLY_OWN_VEHICLE_STATES].enable();
	buttons[IDBTN_RIBI_ARROW].pressed = strasse_t::show_masked_ribi;
	buttons[IDBTN_RIBI_ARROW].enable(skinverwaltung_t::ribi_arrow!=NULL);
	buttons[IDBTN_ONEWAY_RIBI_ONLY].pressed = env_t::show_oneway_ribi_only;
	buttons[IDBTN_ONEWAY_RIBI_ONLY].enable(skinverwaltung_t::ribi_arrow!=NULL);

	// All components are updated, now draw them...
	gui_frame_t::draw(pos, size);
}


bool color_gui_t::infowin_event(const event_t *ev)
{
	if (ev->ev_class == INFOWIN && (ev->ev_code == WIN_UNTOP || ev->ev_code == WIN_CLOSE)) {
		shortcut_settings.cancel_capture();
	}
	if (tabs.get_active_tab_index() == 5 && shortcut_settings.capture_event(ev)) {
		set_dirty();
		return true;
	}
	if (ev->ev_class == INFOWIN && ev->ev_code == WIN_OPEN && tabs.get_active_tab_index() == 5) { shortcut_settings.reload(); }
	const bool handled = gui_frame_t::infowin_event(ev);
	if (handled && tabs.get_active_tab_index() == 5) { set_dirty(); }
	return handled;
}

void color_gui_t::rdwr(loadsave_t *f)
{
	tabs.rdwr(f);
	scrolly_gui.rdwr(f);
	scrolly_map.rdwr(f);
	scrolly_transparency.rdwr(f);
	scrolly_station.rdwr(f);
	scrolly_traffic.rdwr(f);
}
