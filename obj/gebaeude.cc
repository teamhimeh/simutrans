/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include <string.h>
#include <ctype.h>

#ifdef MULTI_THREAD
#include "../utils/simthread.h"
static pthread_mutex_t sync_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t add_to_city_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif


#include "../bauer/hausbauer.h"
#include "../gui/headquarter_info.h"
#include "../simworld.h"
#include "simobj.h"
#include "../simfab.h"
#include "../display/simimg.h"
#include "../display/simgraph.h"
#include "../simhalt.h"
#include "../gui/simwin.h"
#include "../player/simplay.h"
#include "../simdebug.h"
#include "../simintr.h"
#include "../simskin.h"

#include "../boden/grund.h"
#include "../boden/wege/weg.h"


#include "../descriptor/building_desc.h"
#include "../descriptor/intro_dates.h"

#include "../descriptor/ground_desc.h"

#include "../utils/cbuffer_t.h"
#include "../utils/simrandom.h"

#include "../dataobj/loadsave.h"
#include "../dataobj/translator.h"
#include "../dataobj/settings.h"
#include "../dataobj/environment.h"

#include "../gui/obj_info.h"
#include "../gui/city_info.h"

#include "gebaeude.h"


/**
 * Initializes all variables with safe, usable values
 */
void gebaeude_t::init()
{
	tile = NULL;
	tile2 = NULL;
	anim_time = 0;
	sync = false;
	zeige_baugrube = false;
	is_factory = false;
	season = 0;
	background_animated = false;
	remove_ground = true;
	anim_frame = 0;
//	insta_zeit = 0; // init in set_tile()
	ptr.fab = NULL;
}


gebaeude_t::gebaeude_t() : obj_t()
{
	init();
}


gebaeude_t::gebaeude_t(loadsave_t *file) : obj_t()
{
	init();
	rdwr(file);
	if(file->is_version_less(88, 2)) {
		set_yoff(0);
	}
	if(tile  &&  tile->get_phases()>1) {
		welt->sync_eyecandy.add( this );
		sync = true;
	}
}


gebaeude_t::gebaeude_t(koord3d pos, player_t *player, const building_tile_desc_t *t) :
	obj_t(pos)
{
	set_owner( player );

	init();
	if(t) {
		set_tile(t,true); // this will set init time etc.
		player_t::add_maintenance(get_owner(), tile->get_desc()->get_maintenance(welt), tile->get_desc()->get_finance_waytype() );
	}

	// get correct y offset for bridges
	grund_t *gr=welt->lookup(pos);
	if(gr  &&  gr->get_weg_hang()!=gr->get_grund_hang()) {
		set_yoff( -gr->get_weg_yoff() );
	}
}


/**
 * Destructor. Removes this from the list of sync objects if necessary.
 */
gebaeude_t::~gebaeude_t()
{
	if(welt->is_destroying()) {
		return;
		// avoid book-keeping
	}

	// tile is NULL if no description is found during loading. Such a building was never added to the city.
	if(tile  &&  get_stadt()) {
		ptr.stadt->remove_gebaeude_from_stadt(this);
	}

	if(sync) {
		sync = false;
		welt->sync_eyecandy.remove(this);
	}

	// tiles might be invalid, if no description is found during loading
	if(tile  &&  tile->get_desc()  &&  tile->get_desc()->is_attraction()) {
		welt->remove_attraction(this);
	}

	if(tile) {
		player_t::add_maintenance(get_owner(), -tile->get_desc()->get_maintenance(welt) * get_platform_count(), tile->get_desc()->get_finance_waytype());
	}
}


/**
 * rotates a layout of @p desc by 90 degrees (map rotation)
 */
static uint8 rotate_layout(const building_desc_t *desc, uint8 layout)
{
	if(desc->get_type() == building_desc_t::unknown  ||  desc->get_all_layouts()<=4) {
		layout = ((layout+3) % desc->get_all_layouts() & 3);
	}
	else if(  desc->get_all_layouts()==8  &&  desc->get_type() >= building_desc_t::city_res  ) {
		// eight layout city building
		layout = (layout & 4) + ((layout+3) & 3);
	}
	else if(  layout>=48  &&  desc->get_all_layouts()>48  ) {
		// slope stop layout: rotate90 cycles N->E->S->W->N
		// Layout = slope_group (upper nibble, one of 48/64/80/96) + flat_part (lower nibble).
		// When way is NS (bit0=0) the slope group shifts by ±16 following the rotate90 cycle.
		// When way is EW (bit0=1) the slope group is unchanged (rotating EW->NS keeps the slope).
		// The flat part rotates via the same table used for straight stations.
		static const uint8 layout_rotate[16] = { 1, 8, 5, 10, 3, 12, 7, 14, 9, 0, 13, 2, 11, 4, 15, 6 };
		const uint8 slope_group = layout & 0xF0;
		const uint8 flat_part   = layout & 0x0F;
		const int   flat_count  = min( 48, (int)desc->get_all_layouts() );
		const uint8 new_flat    = layout_rotate[flat_part] % flat_count;
		// for NS-way (bit0=0): swap between N/W group and S/E group (add or subtract 16)
		// for EW-way (bit0=1): slope group is unchanged
		const uint8 new_group = (flat_part & 1)
			? slope_group
			: (uint8)(slope_group + ((slope_group & 0x10) ? 16 : -16));
		layout = new_group + new_flat;
	}
	else if(  layout>=16  ) {
		// diagonal tile layout for stations
		// construct the rotated layout
		const bool is_vertical = (layout & 0x30) == 0x10;
		uint8 bits_to_invert = 0x30; // invert vertical / horizontal
		bits_to_invert |= is_vertical ? 8 : 1; // front/back or way connected direction
		if(  ((layout&6) == 2  ||  (layout&6) == 4)  &&  is_vertical  ) {
			bits_to_invert |= 6; // connection of halt
		}
		layout ^= bits_to_invert;
	}
	else {
		// 8 & 16 tile lyoutout for stations
		static uint8 layout_rotate[16] = { 1, 8, 5, 10, 3, 12, 7, 14, 9, 0, 13, 2, 11, 4, 15, 6 };
		layout = layout_rotate[layout] % desc->get_all_layouts();
	}
	return layout;
}


void gebaeude_t::rotate90()
{
	obj_t::rotate90();

	// must or can rotate?
	const building_desc_t* const building_desc = tile->get_desc();
	if (building_desc->get_all_layouts() > 1  ||  building_desc->get_x() * building_desc->get_y() > 1) {
		uint8 layout = tile->get_layout();
		koord new_offset = tile->get_offset();

		layout = rotate_layout(building_desc, layout);
		// have to rotate the tiles :(
		if(  !building_desc->can_rotate()  &&  building_desc->get_all_layouts() == 1  ) {
			if ((welt->get_settings().get_rotation() & 1) == 0) {
				// rotate 180 degree
				new_offset = koord(building_desc->get_x() - 1 - new_offset.x, building_desc->get_y() - 1 - new_offset.y);
			}
			// do nothing here, since we cannot fix it properly
		}
		else {
			// rotate on ...
			new_offset = koord(building_desc->get_y(tile->get_layout()) - 1 - new_offset.y, new_offset.x);
		}

		// such a tile exist?
		if(  building_desc->get_x(layout) > new_offset.x  &&  building_desc->get_y(layout) > new_offset.y  ) {
			const building_tile_desc_t* const new_tile = building_desc->get_tile(layout, new_offset.x, new_offset.y);
			// add new tile: but make them old (no construction)
			uint32 old_insta_zeit = insta_zeit;
			set_tile( new_tile, false );
			insta_zeit = old_insta_zeit;
			if(  building_desc->get_type() != building_desc_t::dock  &&  !tile->has_image()  ) {
				// may have a rotation, that is not recoverable
				if(  !is_factory  &&  new_offset!=koord(0,0)  ) {
					welt->set_nosave_warning();
				}
				if(  is_factory  ) {
					// there are factories with a broken tile
					// => this map rotation cannot be reloaded!
					welt->set_nosave();
				}
			}
		}
		else {
			welt->set_nosave();
		}

		if(  tile2  ) {
			// second platform of a closed diagonal stop: always 1x1, its leg rotates like tile's
			tile2 = building_desc->get_tile( rotate_layout(building_desc, tile2->get_layout()), 0, 0 );
		}
	}
}


/** sets the corresponding pointer to a factory
 */
void gebaeude_t::set_fab(fabrik_t *fd)
{
	// sets the pointer in non-zero
	if(fd) {
		if(!is_factory  &&  ptr.stadt!=NULL) {
			dbg->fatal("gebaeude_t::set_fab()","building already bound to city!");
		}
		is_factory = true;
		ptr.fab = fd;
	}
	else if(is_factory) {
		ptr.fab = NULL;
	}
}


/** sets the corresponding city
 */
void gebaeude_t::set_stadt(stadt_t *s)
{
	if(is_factory  &&  ptr.fab!=NULL) {
		dbg->fatal("gebaeude_t::set_stadt()","building at (%s) already bound to factory!", get_pos().get_str() );
	}
	// sets the pointer in non-zero
	is_factory = false;
	ptr.stadt = s;
}


/* make this building without construction */
void gebaeude_t::add_alter(uint32 a)
{
	insta_zeit -= min(a,insta_zeit);
}


void gebaeude_t::set_tile( const building_tile_desc_t *new_tile, bool start_with_construction )
{
	insta_zeit = welt->get_ticks();

	if(!zeige_baugrube  &&  tile!=NULL) {
		// mark old tile dirty
		mark_images_dirty();
	}

	zeige_baugrube = !new_tile->get_desc()->no_construction_pit()  &&  start_with_construction;
	if(sync) {
		if(  new_tile->get_phases()<=1  &&  !zeige_baugrube  ) {
			// need to stop animation
#ifdef MULTI_THREAD
			pthread_mutex_lock( &sync_mutex );
#endif
			welt->sync_eyecandy.remove(this);
			sync = false;
			anim_frame = 0;
#ifdef MULTI_THREAD
			pthread_mutex_unlock( &sync_mutex );
#endif
		}
	}
	else if(  (new_tile->get_phases()>1  &&  (!is_factory  ||  get_fabrik()->is_currently_producing()) ) ||  zeige_baugrube  ) {
		// needs now animation
#ifdef MULTI_THREAD
		pthread_mutex_lock( &sync_mutex );
#endif
		anim_frame = sim_async_rand( new_tile->get_phases() );
		anim_time = 0;
		welt->sync_eyecandy.add(this);
		sync = true;
#ifdef MULTI_THREAD
		pthread_mutex_unlock( &sync_mutex );
#endif
	}
	tile = new_tile;
	remove_ground = tile->has_image()  &&  !tile->get_desc()->needs_ground();
	set_flag(obj_t::dirty);
}


void gebaeude_t::set_tile2( const building_tile_desc_t *t )
{
	if(  (tile2==NULL) != (t==NULL)  ) {
		// the second platform has its own maintenance
		const building_desc_t *desc = tile->get_desc();
		const sint64 maint = desc->get_maintenance(welt);
		player_t::add_maintenance(get_owner(), t ? maint : -maint, desc->get_finance_waytype());
	}
	if(  tile2  ) {
		// mark old images dirty
		mark_images_dirty();
	}
	tile2 = t;
	set_flag(obj_t::dirty);
}


ribi_t::ribi gebaeude_t::get_diagonal_layout_ribi( uint8 layout )
{
	if(  layout<16  ||  layout>=48  ) {
		return ribi_t::none;
	}
	// see hausbauer_t::build_station_on_diagonal_way() for the bits
	if(  (layout & 0x30)==0x10  ) {
		// vertical
		return (layout & 1) ? ribi_t::southwest : ribi_t::northeast;
	}
	// horizontal
	return (layout & 1) ? ribi_t::northwest : ribi_t::southeast;
}


const building_tile_desc_t *gebaeude_t::get_tile_toward( ribi_t::ribi dir ) const
{
	if(  tile2  &&  (get_diagonal_layout_ribi( tile2->get_layout() ) & dir)  ) {
		return tile2;
	}
	return tile;
}


void gebaeude_t::set_tile_toward( ribi_t::ribi dir, const building_tile_desc_t *t )
{
	if(  tile2  &&  (get_diagonal_layout_ribi( tile2->get_layout() ) & dir)  ) {
		mark_images_dirty();
		tile2 = t;
		set_flag(obj_t::dirty);
	}
	else {
		set_tile( t, false );
	}
}


bool gebaeude_t::is_valid_platform_pair(const grund_t *gr) const
{
	const ribi_t::ribi leg1 = get_diagonal_layout_ribi( tile->get_layout() );
	const ribi_t::ribi leg2 = get_diagonal_layout_ribi( tile2->get_layout() );
	if(  leg1==ribi_t::none  ||  leg2==ribi_t::none  ||  tile->get_desc()!=tile2->get_desc()  ||  !gr->has_two_same_waytype_ways()  ) {
		return false;
	}
	// both legs must be ways of this stop (trams stop on tracks)
	const waytype_t wt = get_waytype();
	if(  gr->get_weg_nr(0)->get_waytype() != (wt==tram_wt ? track_wt : wt)  ) {
		return false;
	}
	const ribi_t::ribi r0 = gr->get_weg_nr(0)->get_ribi_unmasked();
	const ribi_t::ribi r1 = gr->get_weg_nr(1)->get_ribi_unmasked();
	if(  !ribi_t::are_disjoint_bends( r0, r1 )  ) {
		return false;
	}
	return (leg1==r0  &&  leg2==r1)  ||  (leg1==r1  &&  leg2==r0);
}


// a diagonal platform not connected to any other stop tile along its leg (both end bits set)
static bool is_single_platform(const building_tile_desc_t *t)
{
	return (t->get_layout() & 6) == 6;
}


bool gebaeude_t::is_platform_shown(const building_tile_desc_t *t) const
{
	if(  !tile2  ) {
		return true;
	}
	// a single platform is hidden next to a connected one; two single platforms are both shown
	const building_tile_desc_t *other = (t==tile) ? tile2 : tile;
	return !is_single_platform(t)  ||  is_single_platform(other);
}


sync_result gebaeude_t::sync_step(uint32 delta_t)
{
	if(  zeige_baugrube  ) {
		// still under construction?
		if(  welt->get_ticks() - insta_zeit > 5000  ) {
			set_flag( obj_t::dirty );
			mark_image_dirty( get_image(), 0 );
			zeige_baugrube = false;
			if(  tile->get_phases() <= 1  ) {
				sync = false;
				return SYNC_REMOVE;
			}
		}
	}
	else {
		if(  !is_factory  ||  get_fabrik()->is_currently_producing()  ) {
			// normal animated building
			anim_time += delta_t;
			if(  anim_time > tile->get_desc()->get_animation_time()  ) {
				anim_time -= tile->get_desc()->get_animation_time();

				// old positions need redraw
				if(  background_animated  ) {
					set_flag( obj_t::dirty );
					mark_images_dirty();
				}
				else {
					// try foreground
					image_id image = tile->get_foreground( anim_frame, season );
					mark_image_dirty( image, 0 );
					if(  tile2  ) {
						mark_image_dirty( tile2->get_foreground( anim_frame, season ), 0 );
					}
				}

				anim_frame++;
				if(  anim_frame >= tile->get_phases()  ) {
					anim_frame = 0;
				}

				if(  !background_animated  ) {
					// next phase must be marked dirty too ...
					image_id image = tile->get_foreground( anim_frame, season );
					mark_image_dirty( image, 0 );
					if(  tile2  ) {
						mark_image_dirty( tile2->get_foreground( anim_frame, season ), 0 );
					}
				}
			}
		}
	}
	return SYNC_OK;
}


void gebaeude_t::calc_image()
{
	grund_t *gr = welt->lookup( get_pos() );
	// need no ground?
	if(  remove_ground  &&  gr->get_typ() == grund_t::fundament  ) {
		gr->set_image( IMG_EMPTY );
	}

	static uint8 effective_season[][5] = { {0,0,0,0,0}, {0,0,0,0,1}, {0,0,0,0,1}, {0,1,2,3,2}, {0,1,2,3,4} };  // season image lookup from [number of images] and [actual season/snow]

	if(  (gr->ist_tunnel()  &&  !gr->ist_karten_boden())  ||  tile->get_seasons() < 2  ) {
		season = 0;
	}
	else if(  get_pos().z - (get_yoff() / TILE_HEIGHT_STEP) >= welt->get_snowline()  ||  welt->get_climate( get_pos().get_2d() ) == arctic_climate  ) {
		// snowy winter graphics
		season = effective_season[tile->get_seasons() - 1][4];
	}
	else if(  get_pos().z - (get_yoff() / TILE_HEIGHT_STEP) >= welt->get_snowline() - 1  &&  welt->get_season() == 0  ) {
		// snowline crossing in summer
		// so at least some weeks spring/autumn
		season = effective_season[tile->get_seasons() - 1][welt->get_last_month() <= 5 ? 3 : 1];
	}
	else {
		season = effective_season[tile->get_seasons() - 1][welt->get_season()];
	}

	background_animated = tile->is_background_animated( season );
}


image_id gebaeude_t::get_image() const
{
	if(env_t::hide_buildings!=0  &&  tile->has_image()) {
		// opaque houses
		if (is_city_building()) {
			if (env_t::hide_with_transparency) {
				// transparent mode: building shape is drawn by get_outline_image(); no basement here
				return IMG_EMPTY;
			}
			if (skinverwaltung_t::construction_site->get_count() == 1) {
				// only one kind of construction site?
				return skinverwaltung_t::construction_site->get_image_id(0);
			}
			// 6 is special building, 7-9 is res com ind (our type)
			return skinverwaltung_t::construction_site->get_count() > 7 ? skinverwaltung_t::construction_site->get_image_id(tile->get_desc()->get_type() - building_desc_t::city_res + 7) : skinverwaltung_t::construction_site->get_image_id(0);
		}
		else if(  env_t::hide_buildings == env_t::ALL_HIDDEN_BUILDING  &&  tile->get_desc()->get_type() < building_desc_t::others  ) {
			// hide with transparency or tile without information
			if(env_t::hide_with_transparency) {
				if(tile->get_desc()->get_type() == building_desc_t::factory  &&  ptr.fab->get_desc()->get_placement() == factory_desc_t::Water) {
					// no ground tiles for water things
					return IMG_EMPTY;
				}
				return skinverwaltung_t::fussweg->get_image_id(0);
			}
			// only one kind of construction site
			if (skinverwaltung_t::construction_site->get_count() == 1) {
				return skinverwaltung_t::construction_site->get_image_id(0);
			}
			// 6 is special building, 7-9 is res com ind (handled above) => we only go up to 6
			uint16 kind = tile->get_desc()->get_type() > 6 ? 6 : tile->get_desc()->get_type();
			return skinverwaltung_t::construction_site->get_image_id( kind );
		}
	}

	if(  zeige_baugrube  )  {
		return skinverwaltung_t::construction_site->get_image_id(0);
	}
	else if(  !is_platform_shown(tile)  ) {
		return IMG_EMPTY;
	}
	else {
		return tile->get_background( anim_frame, 0, season );
	}
}


image_id gebaeude_t::get_outline_image() const
{
	const bool is_city_info_opened = win_get_magic((ptrdiff_t)this->get_stadt()) != NULL;
	const bool is_in_highlighted_city = is_building_of_city() && this->get_stadt() == city_info_t::get_highlighted_city();

	if (((env_t::hide_buildings != 0 && env_t::hide_with_transparency) || (is_in_highlighted_city && is_city_info_opened)) && !zeige_baugrube)
	{
		// opaque houses
		return is_platform_shown(tile) ? tile->get_background( anim_frame, 0, season ) : IMG_EMPTY;
	}
	return IMG_EMPTY;
}


/* gives outline colour and plots background tile if needed for transparent view */
FLAGGED_PIXVAL gebaeude_t::get_outline_colour() const
{
	uint8 colours[] = { COL_BLACK, COL_YELLOW, COL_YELLOW, COL_PURPLE, COL_RED, COL_GREEN };
	FLAGGED_PIXVAL disp_colour = 0;

	const bool is_city_info_opened = win_get_magic((ptrdiff_t)this->get_stadt()) != NULL;
	const bool is_in_highlighted_city = is_building_of_city() && this->get_stadt() == city_info_t::get_highlighted_city();

	if(env_t::hide_buildings!=env_t::NOT_HIDE) {
		if(is_city_building()) {
			disp_colour = color_idx_to_rgb(colours[0]) | TRANSPARENT50_FLAG | OUTLINE_FLAG;
		}
		else if (env_t::hide_buildings == env_t::ALL_HIDDEN_BUILDING && tile->get_desc()->get_type() < building_desc_t::others) {
			// special building
			disp_colour = color_idx_to_rgb(colours[tile->get_desc()->get_type()]) | TRANSPARENT50_FLAG | OUTLINE_FLAG;
		}
	}
	else if(  is_in_highlighted_city  &&  is_city_info_opened  ) {
		disp_colour = color_idx_to_rgb(colours[0]) | TRANSPARENT75_FLAG | OUTLINE_FLAG;
	}

	return disp_colour;
}


image_id gebaeude_t::get_image(int nr) const
{
	const bool is_city_info_opened = win_get_magic((ptrdiff_t)this->get_stadt()) != NULL;
	const bool is_in_highlighted_city = is_building_of_city() && this->get_stadt() == city_info_t::get_highlighted_city();

	if(zeige_baugrube || env_t::hide_buildings || (is_in_highlighted_city  &&  is_city_info_opened)) {
		return IMG_EMPTY;
	}
	else {
		return is_platform_shown(tile) ? tile->get_background( anim_frame, nr, season ) : IMG_EMPTY;
	}
}


image_id gebaeude_t::get_front_image() const
{
	if(zeige_baugrube) {
		return IMG_EMPTY;
	}

	const bool is_city_info_opened = win_get_magic((ptrdiff_t)this->get_stadt()) != NULL;
	const bool is_in_highlighted_city = is_building_of_city() && this->get_stadt() == city_info_t::get_highlighted_city();

	if (env_t::hide_buildings != 0   &&  (is_building_of_city()  ||  (env_t::hide_buildings == env_t::ALL_HIDDEN_BUILDING  &&  tile->get_desc()->get_type() < building_desc_t::others))) {
		return IMG_EMPTY;
	} else if (  is_in_highlighted_city  &&  is_city_info_opened  ) {
		return IMG_EMPTY;
	}
	else {
		// Show depots, station buildings etc.
		return is_platform_shown(tile) ? tile->get_foreground( anim_frame, season ) : IMG_EMPTY;
	}
}


// on a closed diagonal tile the platform on the north-west leg is behind the south-east one
static bool is_back_leg_tile(const building_tile_desc_t *t)
{
	return gebaeude_t::get_diagonal_layout_ribi( t->get_layout() ) == ribi_t::northwest;
}


// draws one image of the second platform like obj_t::display() / display_after() would
static void display_tile2_image(image_id image, int xpos, int ypos, sint8 owner_nr, bool highlight, bool is_dirty  CLIP_NUM_DEF)
{
	if(  owner_nr != PLAYER_UNOWNED  ) {
		if(  obj_t::show_owner  ) {
			display_blend( image, xpos, ypos, owner_nr, color_idx_to_rgb(world()->get_player(owner_nr)->get_player_color1()+2) | OUTLINE_FLAG | TRANSPARENT75_FLAG, 0, is_dirty  CLIP_NUM_PAR);
		}
		else if(  highlight  ) {
			display_blend( image, xpos, ypos, owner_nr, color_idx_to_rgb(COL_RED) | OUTLINE_FLAG | TRANSPARENT75_FLAG, 0, is_dirty  CLIP_NUM_PAR);
		}
		else {
			display_color( image, xpos, ypos, owner_nr, true, is_dirty  CLIP_NUM_PAR);
		}
	}
	else if(  highlight  ) {
		display_blend( image, xpos, ypos, owner_nr, color_idx_to_rgb(COL_RED) | OUTLINE_FLAG | TRANSPARENT75_FLAG, 0, is_dirty  CLIP_NUM_PAR);
	}
	else {
		display_normal( image, xpos, ypos, 0, true, is_dirty  CLIP_NUM_PAR);
	}
}


void gebaeude_t::display_tile2(int xpos, int ypos  CLIP_NUM_DEF) const
{
	if(  zeige_baugrube  ||  !is_platform_shown(tile2)  ) {
		// the construction site is drawn once (by tile)
		return;
	}
	const int raster_width = get_current_tile_raster_width();
	const bool is_dirty = get_flag(obj_t::dirty);
	xpos += tile_raster_scale_x( get_xoff(), raster_width );
	ypos += tile_raster_scale_y( get_yoff(), raster_width );
	// like get_image(nr): the upper images are not shown with hidden buildings
	for(  int j=0;  j==0  ||  !env_t::hide_buildings;  j++  ) {
		const image_id image = tile2->get_background( anim_frame, j, season );
		if(  image==IMG_EMPTY  ) {
			break;
		}
		display_tile2_image( image, xpos, ypos - j*raster_width, get_owner_nr(), get_flag(obj_t::highlight)  &&  j==0, is_dirty  CLIP_NUM_PAR );
	}
}


void gebaeude_t::display_tile2_after(int xpos, int ypos  CLIP_NUM_DEF) const
{
	if(  zeige_baugrube  ||  !is_platform_shown(tile2)  ) {
		return;
	}
	const image_id image = tile2->get_foreground( anim_frame, season );
	if(  image != IMG_EMPTY  ) {
		const int raster_width = get_current_tile_raster_width();
		xpos += tile_raster_scale_x( get_xoff(), raster_width );
		ypos += tile_raster_scale_y( get_yoff(), raster_width );
		display_tile2_image( image, xpos, ypos, get_owner_nr(), get_flag(obj_t::highlight), get_flag(obj_t::dirty)  CLIP_NUM_PAR );
	}
}


void gebaeude_t::display(int xpos, int ypos  CLIP_NUM_DEF) const
{
	if(  tile2  &&  is_back_leg_tile(tile2)  ) {
		display_tile2( xpos, ypos  CLIP_NUM_PAR );
		obj_t::display( xpos, ypos  CLIP_NUM_PAR );
	}
	else {
		obj_t::display( xpos, ypos  CLIP_NUM_PAR );
		if(  tile2  ) {
			display_tile2( xpos, ypos  CLIP_NUM_PAR );
		}
	}
}


#ifdef MULTI_THREAD
void gebaeude_t::display_after(int xpos, int ypos, const sint8 clip_num) const
#define DISPLAY_AFTER_PAR clip_num
#else
void gebaeude_t::display_after(int xpos, int ypos, bool is_global) const
#define DISPLAY_AFTER_PAR is_global
#endif
{
	if(  tile2  &&  is_back_leg_tile(tile2)  ) {
		display_tile2_after( xpos, ypos  CLIP_NUM_PAR );
		obj_t::display_after( xpos, ypos, DISPLAY_AFTER_PAR );
	}
	else {
		obj_t::display_after( xpos, ypos, DISPLAY_AFTER_PAR );
		if(  tile2  ) {
			display_tile2_after( xpos, ypos  CLIP_NUM_PAR );
		}
	}
}
#undef DISPLAY_AFTER_PAR


/**
 * calculate the passenger level as function of the city size (if there)
 */
int gebaeude_t::get_passagier_level() const
{
	koord dim = tile->get_desc()->get_size();
	sint32 pax = tile->get_desc()->get_level();
	if(  !is_factory  &&  ptr.stadt != NULL  ) {
		// belongs to a city ...
		return ((pax + 6) >> 2) * (welt->get_settings().get_passenger_factor()*welt->get_settings().max_passenger_factor_float() + welt->get_settings().get_passenger_factor_float()) / (16*welt->get_settings().max_passenger_factor_float());
	}
	return pax*dim.x*dim.y;
}


int gebaeude_t::get_mail_level() const
{
	koord dim = tile->get_desc()->get_size();
	sint32 mail = tile->get_desc()->get_mail_level();
	if(  !is_factory  &&  ptr.stadt != NULL  ) {
		return ((mail + 5) >> 2) * (welt->get_settings().get_passenger_factor()*welt->get_settings().max_passenger_factor_float() + welt->get_settings().get_passenger_factor_float()) / (16*welt->get_settings().max_passenger_factor_float());
	}
	return mail*dim.x*dim.y;
}


/**
 * @return eigener Name oder Name der Fabrik falls Teil einer Fabrik
 */
const char *gebaeude_t::get_name() const
{
	if(is_factory  &&  ptr.fab) {
		return ptr.fab->get_name();
	}
	switch(tile->get_desc()->get_type()) {
				case building_desc_t::attraction_city:   return "Besonderes Gebaeude";
				case building_desc_t::attraction_land:   return "Sehenswuerdigkeit";
				case building_desc_t::monument:           return "Denkmal";
				case building_desc_t::townhall:           return "Rathaus";
				default: break;
	}
	return "Gebaeude";
}


/**
 * waytype associated with this object
 */
waytype_t gebaeude_t::get_waytype() const
{
	const building_desc_t *desc = tile->get_desc();
	waytype_t wt = invalid_wt;

	const building_desc_t::btype type = tile->get_desc()->get_type();
	if (type == building_desc_t::depot  ||  type == building_desc_t::generic_stop  ||  type == building_desc_t::generic_extension) {
		wt = (waytype_t)desc->get_extra();
	}
	return wt;
}


bool gebaeude_t::is_townhall() const
{
	return tile->get_desc()->is_townhall();
}


bool gebaeude_t::is_monument() const
{
	return tile->get_desc()->get_type() == building_desc_t::monument;
}


bool gebaeude_t::is_headquarter() const
{
	return tile->get_desc()->is_headquarters();
}


bool gebaeude_t::is_city_building() const
{
	return tile->get_desc()->is_city_building();
}


uint32 gebaeude_t::get_tile_list( vector_tpl<grund_t *> &list ) const
{
	koord size = get_tile()->get_desc()->get_size( get_tile()->get_layout() );
	const koord3d pos0 = get_pos() - get_tile()->get_offset(); // get origin
	koord k;

	list.clear();
	// add all tiles
	for( k.y = 0; k.y < size.y; k.y++ ) {
		for( k.x = 0; k.x < size.x; k.x++ ) {
			if( grund_t* gr = welt->lookup( pos0+k ) ) {
				if( gebaeude_t* const add_gb = gr->find<gebaeude_t>() ) {
					if( is_same_building( add_gb ) ) {
						list.append( gr );
					}
				}
			}
		}
	}
	return list.get_count();
}

// checking the broken buildings
// if building is broken, it will be removed
bool gebaeude_t::is_broken_building() const
{
	vector_tpl<grund_t*> grs;
	if( get_tile_list(grs) < 2 ) {
		return false;
	}
	const koord size = get_tile()->get_desc()->get_size( get_tile()->get_layout() );
	const koord3d pos0 = get_pos() - get_tile()->get_offset();
	const uint8 layout_int = (get_tile()->get_layout());
	// Check about the four corners of the building.
	grund_t* gr;
	gebaeude_t* gb;
	gr = welt->lookup( pos0 );
	if( gr ) {
		if( get_tile()->get_desc()->get_tile(layout_int*size.x*size.y)->get_background(0,0,0) != IMG_EMPTY ) {
			gb = gr->find<gebaeude_t>();
			if( !gb || !is_same_building(gb)  ) {
				return true;
			}
		}
	}
	gr = welt->lookup( pos0+koord3d (size.x-1,0,0) );
	if( gr ) {
		if( get_tile()->get_desc()->get_tile(layout_int*size.x*size.y+size.x-1)->get_background(0,0,0) != IMG_EMPTY ) {
			gb = gr->find<gebaeude_t>();
			if( !gb || !is_same_building(gb)  ) {
				return true;
			}
		}
	}
	gr = welt->lookup( pos0+koord3d (0,size.y-1,0) );
	if( gr ) {
		if( get_tile()->get_desc()->get_tile(layout_int*size.x*size.y+size.y*(size.x-1))->get_background(0,0,0) != IMG_EMPTY ) {
			gb = gr->find<gebaeude_t>();
			if( !gb || !is_same_building(gb)  ) {
				return true;
			}
		}
	}
	gr = welt->lookup( pos0+koord3d(size.x-1,size.y-1,0) );
	if( gr ) {
		if( get_tile()->get_desc()->get_tile((layout_int+1)*size.x*size.y-1)->get_background(0,0,0) != IMG_EMPTY ) {
			gb = gr->find<gebaeude_t>();
			if( !gb || !is_same_building(gb)  ) {
				return true;
			}
		}
	}
	return false;
}



void gebaeude_t::show_info()
{
	if(get_fabrik()) {
		ptr.fab->open_info_window();
		return;
	}

	const uint32 old_count = win_get_open_count();
	const bool special = is_headquarter() || is_townhall();

	if(is_headquarter()) {
		create_win( new headquarter_info_t(get_owner()), w_info, magic_headquarter+get_owner()->get_player_nr() );
	}
	else if (is_townhall()) {
		ptr.stadt->open_info_window();
	}

	if(!tile->get_desc()->no_info_window()) {
		if(!special  ||  (env_t::townhall_info  &&  old_count==win_get_open_count()) ) {
			// iterate over all places to check if there is already an open window
			gebaeude_t * first_tile = NULL;
			static vector_tpl<grund_t *> gb_tiles;
			get_tile_list( gb_tiles );
			FOR( vector_tpl<grund_t*>, gr, gb_tiles ) {
				// no need for check, we jsut did before ...
				gebaeude_t* gb = gr->find<gebaeude_t>();
				if( win_get_magic( (ptrdiff_t)gb ) ) {
					// already open
					return;
				}
				if( first_tile==0 ) {
					first_tile = gb;
				}
			}
			// open info window for the first tile of our building (not relying on presence of (0,0) tile)
			first_tile->obj_t::show_info();
		}
	}
}



// returns true, if there is a halt serving at least one tile
bool gebaeude_t::is_within_players_network(const player_t* player) const
{
	if(  get_fabrik()  ) {
		return ptr.fab->is_within_players_network( player );
	}
	if(  is_townhall()  ) {
		ptr.stadt->is_within_players_network( player );
	}

	// normal building: iterate over all tiles
	vector_tpl<grund_t*> gb_tiles;
	get_tile_list( gb_tiles );
	FOR( vector_tpl<grund_t*>, gr, gb_tiles ) {
		// no need for check, we jsut did before ...
		if( const planquadrat_t* plan = welt->access( gr->get_pos().get_2d()) ) {
			if( plan->get_haltlist_count() > 0 ) {
				const halthandle_t* const halt_list = plan->get_haltlist();
				for( int h = 0; h < plan->get_haltlist_count(); h++ ) {
					if( halt_list[h].is_bound()  &&  (halt_list[h]->get_pax_enabled()  || halt_list[h]->get_mail_enabled())  &&  halt_list[h]->has_available_network( player ) ) {
						return true;
					}
				}
			}
		}
	}
	return false;
}



bool gebaeude_t::is_same_building(const gebaeude_t* other) const
{
	if (other) {
		const building_tile_desc_t* otile = other->get_tile();
		// same descriptor and house tile
		if (get_tile()->get_desc() == otile->get_desc()  &&  get_tile()->get_layout() == otile->get_layout()
			// same position of (0,0) tile
			&&  (get_pos() - get_tile()->get_offset()  == other->get_pos() - otile->get_offset())) {
			return true;
		}
	}
	return false;
}


gebaeude_t* gebaeude_t::get_first_tile()
{
	koord size = get_tile()->get_desc()->get_size( get_tile()->get_layout() );
	const koord3d pos0 = get_pos() - get_tile()->get_offset(); // get origin
	koord k;

	// add all tiles
	for( k.y = 0; k.y < size.y; k.y++ ) {
		for( k.x = 0; k.x < size.x; k.x++ ) {
			if( grund_t* gr = welt->lookup( pos0+k ) ) {
				if( gebaeude_t* const add_gb = obj_cast<gebaeude_t>(gr->first_obj()) ) {
					if( is_same_building( add_gb ) ) {
						return add_gb;
					}
				}
			}
		}
	}
	return this;
}


void gebaeude_t::info(cbuffer_t & buf) const
{
	obj_t::info(buf);

	if(is_factory  &&  ptr.fab != NULL) {
		buf.append((char *)0);
	}
	else if(zeige_baugrube) {
		buf.append(translator::translate("Baustelle"));
		buf.append("\n");
	}
	else {
		const char *desc = tile->get_desc()->get_name();
		if(desc != NULL) {
			const char *trans_desc = translator::translate(desc);
			if(trans_desc==desc) {
				// no description here
				switch(tile->get_desc()->get_type()) {
					case building_desc_t::city_res:
						trans_desc = translator::translate("residential house");
						break;
					case building_desc_t::city_ind:
						trans_desc = translator::translate("industrial building");
						break;
					case building_desc_t::city_com:
						trans_desc = translator::translate("shops and stores");
						break;
					default:
						// use file name
						break;
				}
				buf.append(trans_desc);
			}
			else {
				// since the format changed, we remove all but double newlines
				char *text = new char[strlen(trans_desc)+1];
				char *dest = text;
				const char *src = trans_desc;
				while(  *src!=0  ) {
					*dest = *src;
					if(src[0]=='\n') {
						if(src[1]=='\n') {
							src ++;
							dest++;
							*dest = '\n';
						}
						else {
							*dest = ' ';
						}
					}
					src ++;
					dest ++;
				}
				// remove double line breaks at the end
				*dest = 0;
				while( dest>text  &&  *--dest=='\n'  ) {
					*dest = 0;
				}

				buf.append(text);
				delete [] text;
			}
		}
		buf.append( "\n\n" );

		// belongs to which city?
		if(  !is_factory  &&  ptr.stadt != NULL  ) {
			buf.printf(translator::translate("Town: %s\n"), ptr.stadt->get_name());
		}

		if(  !get_tile()->get_desc()->is_transport_building()  ) {
			buf.printf("%s: %d\n", translator::translate("Passagierrate"), get_passagier_level());
			buf.printf("%s: %d\n", translator::translate("Postrate"),      get_mail_level());
		}
		else {
			// stop length of the platform on this tile (ships do not use halt length)
			const grund_t *gr = welt->lookup(get_pos());
			for(  uint8 i=0;  gr  &&  gr->get_halt().is_bound()  &&  i<2;  i++  ) {
				const weg_t *weg = gr->get_weg_nr(i);
				if(  weg  &&  weg->get_waytype()!=water_wt  ) {
					append_platform_length_string_if_needed(buf, weg);
					buf.append("\n");
				}
			}
		}

		building_desc_t const& h = *tile->get_desc();
		buf.printf("%s%u", translator::translate("\nBauzeit von"), h.get_intro_year_month() / 12);
		if (h.get_retire_year_month() != DEFAULT_RETIRE_DATE * 12) {
			buf.printf("%s%u", translator::translate("\nBauzeit bis"), h.get_retire_year_month() / 12);
		}

		buf.append("\n");
		if(get_owner()==NULL) {
			const sint32 v = (sint32)( -welt->get_settings().cst_multiply_remove_haus * (tile->get_desc()->get_level() + 1) / 100 );
			buf.printf("\n%s: %d$\n", translator::translate("Wert"), v);
		}

		if (char const* const maker = tile->get_desc()->get_copyright()) {
			buf.append("\n");
			buf.printf(translator::translate("Constructed by %s"), maker);
		}
#ifdef DEBUG
		buf.append( "\n\nrotation " );
		buf.append( tile->get_layout(), 0 );
		if(  !env_t::networkmode  ) {
			// Calling stadt_t::orient_city_building here in a network game causes a crash.
			buf.append( " best layout " );
			buf.append( stadt_t::orient_city_building( get_pos().get_2d() - tile->get_offset(), tile->get_desc(), koord(3,3) ), 0 );
		}
		buf.append( "\n" );
#endif
	}
}


void gebaeude_t::rdwr(loadsave_t *file)
{
	// do not save factory buildings => factory will reconstruct them
	assert(!is_factory);
	xml_tag_t d( file, "gebaeude_t" );

	obj_t::rdwr(file);

	char buf[128];
	short idx;

	if(file->is_saving()) {
		const char *s = tile->get_desc()->get_name();
		file->rdwr_str(s);
		idx = tile->get_index();
	}
	else {
		file->rdwr_str(buf, lengthof(buf));
	}
	file->rdwr_short(idx);
	file->rdwr_long(insta_zeit);

	if(file->is_loading()) {
		tile = hausbauer_t::find_tile(buf, idx);
		if(tile==NULL) {
			// try with compatibility list first
			tile = hausbauer_t::find_tile(translator::compatibility_name(buf), idx);
			if(tile==NULL) {
				DBG_MESSAGE("gebaeude_t::rdwr()","neither %s nor %s, tile %i not found, try other replacement",translator::compatibility_name(buf),buf,idx);
			}
			else {
				DBG_MESSAGE("gebaeude_t::rdwr()","%s replaced by %s, tile %i",buf,translator::compatibility_name(buf),idx);
			}
		}
		if(tile==NULL) {
			// first check for special buildings
			if(strstr(buf,"TrainStop")!=NULL) {
				tile = hausbauer_t::find_tile("TrainStop", idx);
			} else if(strstr(buf,"BusStop")!=NULL) {
				tile = hausbauer_t::find_tile("BusStop", idx);
			} else if(strstr(buf,"ShipStop")!=NULL) {
				tile = hausbauer_t::find_tile("ShipStop", idx);
			} else if(strstr(buf,"PostOffice")!=NULL) {
				tile = hausbauer_t::find_tile("PostOffice", idx);
			} else if(strstr(buf,"StationBlg")!=NULL) {
				tile = hausbauer_t::find_tile("StationBlg", idx);
			}
			else {
				// try to find a fitting building
				int level=atoi(buf);
				building_desc_t::btype type = building_desc_t::unknown;

				if(level>0) {
					// May be an old 64er, so we can try some
					if(strncmp(buf+3,"WOHN",4)==0) {
						type = building_desc_t::city_res;
					} else if(strncmp(buf+3,"FAB",3)==0) {
						type = building_desc_t::city_ind;
					}
					else {
						type = building_desc_t::city_com;
					}
					level --;
				}
				else if(buf[3]=='_') {
					/* should have the form of RES/IND/COM_xx_level
					 * xx is usually a number by can be anything without underscores
					 */
					level = atoi(strrchr( buf, '_' )+1);
					if(level>0) {
						switch(toupper(buf[0])) {
							case 'R': type = building_desc_t::city_res; break;
							case 'I': type = building_desc_t::city_ind; break;
							case 'C': type = building_desc_t::city_com; break;
						}
					}
					level --;
				}
				// we try to replace citybuildings with their matching counterparts
				// if none are matching, we try again without climates and timeline!
				switch(type) {
					case building_desc_t::city_res:
						{
							const building_desc_t *bdsc = hausbauer_t::get_residential( level, welt->get_timeline_year_month(), welt->get_climate( get_pos().get_2d() ), 0, koord((sint16)1,(sint16)1), koord((sint16)1,(sint16)1) );
							if(bdsc==NULL) {
								bdsc = hausbauer_t::get_residential(level,0, MAX_CLIMATES, 0, koord((sint16)1,(sint16)1), koord((sint16)1,(sint16)1) );
							}
							if( bdsc) {
								dbg->message("gebaeude_t::rwdr", "replace unknown building %s with residence level %i by %s",buf,level,bdsc->get_name());
								tile = bdsc->get_tile(0);
							}
						}
						break;

					case building_desc_t::city_com:
						{
							// for replacement, ignore cluster and size
							const building_desc_t *bdsc = hausbauer_t::get_commercial( level, welt->get_timeline_year_month(), welt->get_climate( get_pos().get_2d() ), 0, koord((sint16)1,(sint16)1), koord((sint16)1,(sint16)1) );
							if(bdsc==NULL) {
								bdsc = hausbauer_t::get_commercial(level, 0, MAX_CLIMATES, 0, koord((sint16)1,(sint16)1), koord((sint16)1,(sint16)1) );
							}
							if(bdsc) {
								dbg->message("gebaeude_t::rwdr", "replace unknown building %s with commercial level %i by %s",buf,level,bdsc->get_name());
								tile = bdsc->get_tile(0);
							}
						}
						break;

					case building_desc_t::city_ind:
						{
							const building_desc_t *bdsc = hausbauer_t::get_industrial( level, welt->get_timeline_year_month(), welt->get_climate( get_pos().get_2d() ), 0, koord((sint16)1,(sint16)1), koord((sint16)1,(sint16)1) );
							if(bdsc==NULL) {
								bdsc = hausbauer_t::get_industrial(level, 0, MAX_CLIMATES, 0, koord((sint16)1,(sint16)1), koord((sint16)1,(sint16)1) );
								if(bdsc==NULL) {
									bdsc = hausbauer_t::get_residential(level, 0, MAX_CLIMATES, 0, koord((sint16)1,(sint16)1), koord((sint16)1,(sint16)1) );
								}
							}
							if (bdsc) {
								dbg->message("gebaeude_t::rwdr", "replace unknown building %s with industry level %i by %s",buf,level,bdsc->get_name());
								tile = bdsc->get_tile(0);
							}
						}
						break;

					default:
						dbg->warning("gebaeude_t::rwdr", "description %s for building at %d,%d not found (will be removed)!", buf, get_pos().x, get_pos().y);
						welt->add_missing_paks( buf, karte_t::MISSING_BUILDING );
				}
			}
		}

		// here we should have a valid tile pointer or nothing ...

		/* avoid double construction of monuments:
		 * remove them from selection lists
		 */
		if (tile  &&  tile->get_desc()->get_type() == building_desc_t::monument) {
			hausbauer_t::monument_erected(tile->get_desc());
		}
		if (tile) {
			remove_ground = tile->has_image()  &&  !tile->get_desc()->needs_ground();
		}
	}

	if(file->is_version_less(99, 6)) {
		// ignore the sync flag
		uint8 dummy=sync;
		file->rdwr_byte(dummy);
	}

	// restore city pointer here
	if(  file->is_version_atleast(99, 14)  ) {
		sint32 city_index = -1;
		if(  file->is_saving()  &&  ptr.stadt!=NULL  ) {
			if(  welt->get_cities().is_contained( ptr.stadt )  ) {
				city_index = welt->get_cities().index_of( ptr.stadt );
			}
			// else: city was removed; save city_index as -1 without touching ptr.stadt
		}
		file->rdwr_long(city_index);
		if(  file->is_loading()  &&  (tile==NULL  ||  tile->get_desc()==NULL  ||  tile->get_desc()->is_connected_with_town())  ) {
			if (city_index != -1) {
				ptr.stadt = welt->get_cities()[city_index];
			}
			else {
				ptr.stadt = NULL;
			}
		}
	}

	// second platform of a stop on a closed diagonal tile (same desc as tile, -1 = none)
	if(  file->get_OTRP_version() >= 63  ) {
		sint16 idx2 = -1;
		if(  file->is_saving()  &&  tile2  ) {
			idx2 = tile2->get_index();
		}
		file->rdwr_short(idx2);
		if(  file->is_loading()  ) {
			tile2 = NULL;
			// the desc may have been replaced while loading, so take it from tile and check the index
			const building_desc_t *desc = tile ? tile->get_desc() : NULL;
			if(  idx2 >= 0  &&  desc  &&  desc->get_all_layouts() >= 48
			     &&  idx2 < (sint32)desc->get_all_layouts() * desc->get_x() * desc->get_y()  ) {
				tile2 = desc->get_tile(idx2);
				if(  get_diagonal_layout_ribi( tile2->get_layout() )==ribi_t::none  ) {
					tile2 = NULL;
				}
			}
		}
	}

	if(file->is_loading()) {
		anim_frame = 0;
		anim_time = 0;
		sync = false;

		// rebuild tourist attraction list
		if(tile && tile->get_desc()->is_attraction()) {
			welt->add_attraction( this );
		}
	}
}


void gebaeude_t::finish_rd(const uint8 /*loaded_OTRP_version*/)
{
	if(  tile2  ) {
		// only this tile can be checked here (finish_rd() runs in parallel for the map)
		const grund_t *gr = welt->lookup( get_pos() );
		if(  !gr  ||  !is_valid_platform_pair(gr)  ) {
			dbg->warning( "gebaeude_t::finish_rd()", "second platform at (%s) does not match the ways, removed", get_pos().get_str() );
			tile2 = NULL;
		}
	}
	player_t::add_maintenance(get_owner(), tile->get_desc()->get_maintenance(welt) * get_platform_count(), tile->get_desc()->get_finance_waytype());

	// citybuilding, but no town?
	if(  tile->get_offset()==koord(0,0)  ) {
		if(  tile->get_desc()->is_connected_with_town()  ) {
			stadt_t *city = (ptr.stadt==NULL) ? welt->find_nearest_city( get_pos().get_2d() ) : ptr.stadt;
			if(city) {
#ifdef MULTI_THREAD
				pthread_mutex_lock( &add_to_city_mutex );
#endif
				city->add_gebaeude_to_stadt(this, true);
#ifdef MULTI_THREAD
				pthread_mutex_unlock( &add_to_city_mutex );
#endif
			}
		}
		else if(  !is_factory  ) {
			ptr.stadt = NULL;
		}
	}
}


void gebaeude_t::cleanup(player_t *player)
{
//	DBG_MESSAGE("gebaeude_t::entferne()","gb %i");
	// remove costs
	sint64 cost = welt->get_settings().cst_multiply_remove_haus;

	// tearing down halts is always single costs only
	if (!tile->get_desc()->is_transport_building()) {
		cost *= tile->get_desc()->get_level() + 1;
	}

	player_t::book_construction_costs(player, cost, get_pos().get_2d(), tile->get_desc()->get_finance_waytype());
	
	const grund_t * this_gr = welt->lookup( get_pos() );
	if(tile->get_desc()->get_all_layouts()==1  ||  is_city_building()  ||  !this_gr) {
		mark_images_dirty();
		return;
	}

	// may need to update next buildings, in the case of start, middle, end buildings
	// realign surrounding buildings...

	// Which end-cap bit to restore on a neighbour reached from direction dir,
	// given the neighbour's layout type (straight=0, vertical diag=1, horizontal diag=2).
	const ribi_t::ribi bit_4_turn_on_dir[3] = {
		ribi_t::southeast, // straight
		ribi_t::northwest, // vertical diagonal
		ribi_t::southwest  // horizontal diagonal
	};

	// Walk every way on this tile. For each connected direction, find the neighbouring
	// stop via get_neighbour() and restore its end-cap bit.
	// get_neighbour() resolves the correct height for slopes and bridges via get_vmove(),
	// handling single slopes, double slopes, and all combinations correctly.
	for(  int w = 0;  w < 2;  w++  ) {
		const weg_t* weg = this_gr->get_weg_nr( w );
		if(  !weg  ) { continue; }
		const ribi_t::ribi way_ribi = weg->get_ribi_unmasked();
		for(  uint8 i = 0;  i < 4;  i++  ) {
			const ribi_t::ribi dir = ribi_t::nesw[i];
			if(  !(way_ribi & dir)  ) { continue; }

			grund_t* gr = nullptr;
			this_gr->get_neighbour( gr, weg->get_waytype(), dir );
			if(  !gr  ) {
				// Same-height tile with a halt (stop extension without direct way link).
				const planquadrat_t* pl = welt->access( get_pos().get_2d() + koord(dir) );
				if(  pl  ) {
					grund_t* same_z = pl->get_boden_in_hoehe( get_pos().z );
					if(  same_z  &&  same_z->get_halt().is_bound()  ) {
						gr = same_z;
					}
				}
			}

			gebaeude_t* gb = gr ? gr->find<gebaeude_t>() : NULL;
			if(  !gb  ||  gb->get_tile()->get_desc()->get_all_layouts() <= 4u  ) {
				continue;
			}
			// on a closed diagonal neighbour: the platform on the leg facing us
			const ribi_t::ribi back = ribi_t::backward( dir );
			const building_tile_desc_t *nb_tile = gb->get_tile_toward( back );
			const koord xy = nb_tile->get_offset();
			uint8 layoutbase = nb_tile->get_layout();
			// Slope stop neighbours use the straight-way rule (index 0) for bit selection.
			const uint32 nb_idx = (layoutbase >= 48 && nb_tile->get_desc()->get_all_layouts() > 48)
			                    ? 0u : (layoutbase & 0x30u) >> 4;
			const bool bit_4_turn_on = (dir & bit_4_turn_on_dir[nb_idx]) > 0;
			layoutbase |= bit_4_turn_on ? 4u : 2u; // restore end-cap bit on neighbour
			gb->set_tile_toward( back, nb_tile->get_desc()->get_tile(layoutbase, xy.x, xy.y) );
		}
	}
	mark_images_dirty();
}


void gebaeude_t::mark_images_dirty() const
{
	// remove all traces from the screen
	image_id img;
	if(  zeige_baugrube  ||
			(!env_t::hide_with_transparency  &&
				env_t::hide_buildings>(is_building_of_city() ? env_t::NOT_HIDE : env_t::SOME_HIDDEN_BUILDING))) {
		img = skinverwaltung_t::construction_site->get_image_id(0);
	}
	else {
		img = tile->get_background( anim_frame, 0, season ) ;
	}
	for(  int i=0;  img!=IMG_EMPTY;  img=get_image(++i)  ) {
		mark_image_dirty( img, -(i*get_tile_raster_width()) );
	}
	if(  tile2  ) {
		// a hidden platform (see is_platform_shown()) may just have been shown
		for(  int i=1;  (img=tile->get_background( anim_frame, i, season ))!=IMG_EMPTY;  i++  ) {
			mark_image_dirty( img, -(i*get_tile_raster_width()) );
		}
		mark_image_dirty( tile->get_foreground( anim_frame, season ), 0 );
	}
	if(  tile2  ) {
		for(  int i=0;  (img=tile2->get_background( anim_frame, i, season ))!=IMG_EMPTY;  i++  ) {
			mark_image_dirty( img, -(i*get_tile_raster_width()) );
		}
		mark_image_dirty( tile2->get_foreground( anim_frame, season ), 0 );
	}
}
