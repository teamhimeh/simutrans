/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef BAUER_TUNNELBAUER_H
#define BAUER_TUNNELBAUER_H


#include "../simtypes.h"
#include "../dataobj/koord.h"
#include "../dataobj/koord3d.h"
#include "../tpl/stringhashtable_tpl.h"

class karte_ptr_t;
class player_t;
class tunnel_desc_t;
class way_desc_t;
class tool_selector_t;
class grund_t;
class weg_t;

/**
 * Baut Tunnel. Tunnel sollten nicht direkt instanziiert werden
 * sondern immer vom tunnel_builder_t erzeugt werden.
 *
 * Es gibt keine Instanz - nur statische Methoden.
 */
class tunnel_builder_t {
private:
	static karte_ptr_t welt;

	static bool build_tunnel(player_t *player, koord3d pos, koord3d end, koord zv, const tunnel_desc_t *desc, overtaking_mode_t overtaking_mode, uint8 street_flag, sint8 vehicle_offset);
	static void build_tunnel_portal(player_t *player, koord3d end, koord zv, const tunnel_desc_t *desc, const way_desc_t *way_desc, sint64 &cost, bool connect_inside, overtaking_mode_t overtaking_mode, uint8 street_flag, bool beginning, sint8 vehicle_offset);

	tunnel_builder_t() {} // private -> no instance please

public:
	static koord3d find_end_pos(player_t *player, koord3d pos, koord zv, const tunnel_desc_t *desc, bool full_tunnel=true, const char** msg=NULL);

	static void register_desc(tunnel_desc_t *desc);

	static const tunnel_desc_t *get_desc(const char *);

	/// @returns the full descriptor table (for iterating all registered tunnels)
	static const stringhashtable_tpl<tunnel_desc_t *>& get_desc_table();

	static const tunnel_desc_t *get_tunnel_desc(const waytype_t wtyp, const sint32 min_speed,const uint16 time);

	static void fill_menu(tool_selector_t *tool_selector, const waytype_t wtyp, sint16 sound_ok);
	/**
	 * Returns a list with available tunnel types.
	 */
	static const vector_tpl<const tunnel_desc_t *>& get_available_tunnels(const waytype_t wtyp);

	static const char *build( player_t *player, koord pos, const tunnel_desc_t *desc, bool full_tunnel, overtaking_mode_t overtaking_mode, uint8 street_flag, sint8 vehicle_offset );

	static const char *remove(player_t *player, koord3d pos, waytype_t wegtyp, bool all);

	/**
	 * The way on an underground or elevated tile @p gr that must survive the removal of the way
	 * of @p wegtyp: a different waytype on the opposite diagonal bend (closed diagonal).
	 * Tram tracks have no tunnels of their own, so they are removed with the way they run on.
	 * @return NULL if the tile can be removed with the way of @p wegtyp
	 */
	static weg_t *get_way_kept_on_removal(const grund_t *gr, waytype_t wegtyp);

	/**
	 * Removes the way of @p wegtyp (keeping the directions @p rem) from a tile shared with a
	 * way of @p kept_wt, keeping the tile and the other way. If the tunnel_t of a tunnel tile
	 * belongs to @p wegtyp and the way goes completely, it is replaced by a tunnel of the
	 * remaining waytype, which then takes over its maintenance.
	 * @return false if the way could not be removed completely (see grund_t::remove_everything_from_way())
	 */
	static bool remove_way_keep_tile(player_t *player, grund_t *gr, waytype_t wegtyp, waytype_t kept_wt, ribi_t::ribi rem);
};

#endif
