/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef UTILS_ROUTE_TREE_ROUTING_H
#define UTILS_ROUTE_TREE_ROUTING_H

#include "route_tree_cache.h"
#include <cstdio>
#include <cstdlib>

// Adapter for passenger journey-time routing without walking or overcrowding.
// Other routing modes continue to use the existing targeted search.
namespace route_tree_routing {
struct graph_t
{
	using edge_t = haltestelle_t::connection_t;

	static size_t size()
	{
		return halthandle_t::get_size();
	}

	static halthandle_t halt(uint32 id)
	{
		halthandle_t h;
		h.set_id(id);
		return h;
	}

	static const vector_tpl<haltestelle_t::connection_t> &edges(uint32 id)
	{
		return halt(id)->get_connections(goods_manager_t::INDEX_PAS);
	}

	static uint32 target(const haltestelle_t::connection_t &e)
	{
		return e.halt.is_bound() ? e.halt.get_id() : 0;
	}

	static uint32 weight(const haltestelle_t::connection_t &e)
	{
		return e.weight;
	}

	static bool transfer(uint32 id)
	{
		return halt(id)->is_transfer(goods_manager_t::INDEX_PAS);
	}
};

struct state_t
{
	route_tree::cache_t cache{1024};
	bool checked = false, compatible = false;
	// Optional exhaustive cached/fresh comparison, with the cached result used
	// by the game. No second search or file I/O when this variable is absent.
	FILE *verification = nullptr;
	uint64 calls = 0, hits = 0, fallbacks = 0, mismatches = 0, clears = 0;

	state_t()
	{
		if(const char *path = std::getenv("SIMUTRANS_ROUTE_TREE_VERIFY")) {
			verification = std::fopen(path, "w");
			if(verification) {
				std::fprintf(verification, "calls,hits,fallbacks,mismatches,clears,contexts\n");
			}
		}
	}

	void dump()
	{
		if(verification) {
			std::fprintf(verification, "%llu,%llu,%llu,%llu,%llu,%zu\n", (unsigned long long)calls,
			             (unsigned long long)hits, (unsigned long long)fallbacks, (unsigned long long)mismatches,
			             (unsigned long long)clears, cache.count());
			std::fflush(verification);
		}
	}

	~state_t()
	{
		dump();
		if(verification) {
			std::fclose(verification);
		}
	}
};

inline state_t &state()
{
	static state_t s;
	return s;
}

inline void clear()
{
	state_t &s = state();
	s.cache.clear();
	s.checked = false;
	++s.clears;
}

inline bool compatible()
{
	state_t &s = state();
	if(!s.checked) {
		s.checked = true;
		s.compatible = true;
		// A settings change can precede the next graph commit. Never interpret
		// an old walking graph as the simpler graph supported by this cache.
		for(uint32 id = 1; id<graph_t::size(); ++id) {
			if(!graph_t::halt(id).is_bound()) {
				continue;
			}
			for(const graph_t::edge_t &e : graph_t::edges(id)) {
				if(e.is_foot_path  ||  (e.halt.is_bound()  &&  e.is_transfer!=graph_t::transfer(e.halt.get_id()))) {
					s.compatible = false;
					return false;
				}
			}
		}
	}
	return s.compatible;
}

// Returns false when the bounded search cannot certify an answer; callers must
// then run the targeted search with its own budget and untouched output goods.
inline bool search(const halthandle_t *starts, uint32 count, const vector_tpl<halthandle_t> &targets, uint32 max_transfers,
       uint32 max_hops, ware_t &ware, ware_t &reverse, int &status)
{
	if(!compatible()) {
		return false;
	}
	std::vector<uint32> sources, destinations;
	for(uint32 i = 0; i<count; ++i) {
		sources.push_back(starts[i].get_id());
	}
	for(halthandle_t h : targets) {
		destinations.push_back(h.get_id());
	}
	state_t &s = state();
	route_tree::result_t result = s.cache.query(graph_t{}, sources, destinations, max_transfers, max_hops);
	++s.calls;
	s.hits += result.hit;
	if(s.verification) {
		route_tree::cache_t fresh(0);
		route_tree::result_t reference = fresh.query(graph_t{}, sources, destinations, max_transfers, max_hops);
		if(result.fallback!=reference.fallback  ||
		   (!result.fallback  &&
		    (result.found!=reference.found  ||  result.cost!=reference.cost  ||  result.path!=reference.path))) {
			++s.mismatches;
			result.fallback = true;
		}
	}
	s.fallbacks += result.fallback;
	if((s.calls % 1024)==0) {
		s.dump();
	}
	if(result.fallback) {
		return false;
	}

	ware.clear_transit_halts();
	reverse.clear_transit_halts();
	if(!result.found) {
		ware.set_ziel(halthandle_t());
		reverse.set_ziel(halthandle_t());
		status = haltestelle_t::NO_ROUTE;
		return true;
	}
	vector_tpl<halthandle_t> route;
	for(size_t i = 1; i<result.path.size(); ++i) {
		route.append(graph_t::halt(result.path[i]));
	}
	ware.set_ziel(graph_t::halt(result.path.back()));
	ware.set_transit_halts(route);
	reverse.set_ziel(graph_t::halt(result.path.front()));
	uint32 transfers = graph_t::transfer(result.path.back());
	for(const graph_t::edge_t &e : graph_t::edges(result.path.back())) {
		if(transfers>1) {
			break;
		}
		transfers += e.halt.is_bound()  &&  e.is_transfer;
	}
	if(transfers<=1) {
		vector_tpl<halthandle_t> hop;
		hop.append(graph_t::halt(result.path[result.path.size() - 2]));
		reverse.set_transit_halts(hop);
	}
	status = haltestelle_t::ROUTE_OK;
	return true;
}
} // namespace route_tree_routing
#endif
