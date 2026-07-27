/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "route_cache.h"
#include "../simworld.h"
#include <unordered_set>


namespace {
	struct koord3d_hash_t {
		size_t operator()(const koord3d &k) const {
			return (size_t)(uint16)k.x ^ ((size_t)(uint16)k.y << 16) ^ ((size_t)(uint8)k.z << 24);
		}
	};
}


route_t* route_cache_t::find(linehandle_t line, uint8 schedule_entry, koord3d start, koord3d ziel,
                             sint32 max_speed_kmh, uint16 convoy_length, bool need_electric)
{
	line_map_t::iterator line_it = map.find(line);
	if (line_it == map.end()) return nullptr;
	entry_map_t::iterator entry_it = line_it->second.find(schedule_entry);
	if (entry_it == line_it->second.end()) return nullptr;
	route_map_t::iterator it = entry_it->second.find({start, ziel, max_speed_kmh, convoy_length, need_electric});
	if (it == entry_it->second.end()) return nullptr;
	// Expiration check (unsigned subtraction handles wraparound)
	karte_ptr_t welt;
	if (welt->get_ticks() - it->second.added_ticks > welt->ticks_per_world_month) {
		entry_it->second.erase(it);
		return nullptr;
	}
	return &it->second.route;
}


void route_cache_t::add(linehandle_t line, uint8 schedule_entry, koord3d start, koord3d ziel,
                        sint32 max_speed_kmh, uint16 convoy_length, bool need_electric,
                        const route_t &r)
{
	karte_ptr_t welt;
	const key_t key = {start, ziel, max_speed_kmh, convoy_length, need_electric};
	route_map_t &route_map = map[line][schedule_entry];
	route_map[key] = {r, welt->get_ticks()};
	if (route_map.size() > MAX_ROUTES_PER_ENTRY) {
		// Evict the oldest entry
		route_map_t::iterator oldest = route_map.end();
		uint32 oldest_ticks = UINT32_MAX;
		for (route_map_t::iterator it = route_map.begin(); it != route_map.end(); ++it) {
			if (it->second.added_ticks < oldest_ticks) {
				oldest_ticks = it->second.added_ticks;
				oldest = it;
			}
		}
		if (oldest != route_map.end()) {
			route_map.erase(oldest);
		}
	}
}


void route_cache_t::remove(linehandle_t line, uint8 schedule_entry, koord3d start, koord3d ziel,
                           sint32 max_speed_kmh, uint16 convoy_length, bool need_electric)
{
	line_map_t::iterator line_it = map.find(line);
	if (line_it != map.end()) {
		entry_map_t::iterator entry_it = line_it->second.find(schedule_entry);
		if (entry_it != line_it->second.end()) {
			entry_it->second.erase({start, ziel, max_speed_kmh, convoy_length, need_electric});
		}
	}
}


void route_cache_t::remove_line(linehandle_t line)
{
	map.erase(line);
}


void route_cache_t::get_route_tiles_for_line(linehandle_t line, vector_tpl<koord3d> &tiles) const
{
	line_map_t::const_iterator line_it = map.find(line);
	if (line_it == map.end()) return;
	karte_ptr_t welt;
	std::unordered_set<koord3d, koord3d_hash_t> seen;
	for (entry_map_t::const_iterator entry_it = line_it->second.begin(); entry_it != line_it->second.end(); ++entry_it) {
		for (route_map_t::const_iterator it = entry_it->second.begin(); it != entry_it->second.end(); ++it) {
			if (welt->get_ticks() - it->second.added_ticks > welt->ticks_per_world_month) {
				// expired; skip (do not erase here, this is a read-only query)
				continue;
			}
			const route_t &r = it->second.route;
			for (uint32 i = 0; i < r.get_count(); i++) {
				if (seen.insert(r.at(i)).second) {
					tiles.append(r.at(i));
				}
			}
		}
	}
}
