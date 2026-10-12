/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef UTILS_ROUTE_TREE_CACHE_H
#define UTILS_ROUTE_TREE_CACHE_H

#include "../simtypes.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <list>
#include <memory>
#include <unordered_map>
#include <vector>

namespace route_tree {
using node_id = uint32;
static constexpr uint32 infinity = std::numeric_limits<uint32>::max();

struct result_t
{
	bool found = false, fallback = false, hit = false, ready = false;
	uint32 cost = infinity;
	uint64 pops = 0, edges = 0;
	std::vector<node_id> path; // includes the origin and the selected destination
};

// Graph supplies edge_t, size(), edges(id), target(edge), weight(edge), and transfer(id).
// Only nonnegative weights, no walking edges, no overcrowding are supported.
// Node zero is invalid. The caller must clear the cache before changing the
// graph (including transfer flags and handle reuse). Keys include the complete
// source set and limits; destinations never change the expansion order.
// A bounded search may decline to answer, but must never report NO_ROUTE merely
// because it exhausted its budget or encountered the transfer-depth boundary.
class cache_t
{
	struct label_t
	{
		uint32 distance = infinity, previous = 0;
		uint16 depth = 0;
		bool origin = false;
	};

	struct item_t
	{
		uint32 distance;
		node_id id;
	};

	struct later_t
	{
		bool operator()(const item_t &a, const item_t &b) const
		{
			return a.distance!=b.distance ? a.distance>b.distance : a.id>b.id;
		}
	};

	struct context_t
	{
		std::vector<label_t> labels;
		std::vector<item_t> queue;
		uint32 allocations = 0;
		bool limited = false;
		uint32 certified_distance = 0;

		void reset(size_t size, const std::vector<node_id> &origins)
		{
			labels.assign(size, label_t{});
			queue.clear();
			allocations = 0;
			limited = false;
			certified_distance = 0;
			for(node_id id : origins) {
				labels[id] = {1, 0, 0, true};
				push({1, id});
				++allocations;
			}
		}

		void push(item_t n)
		{
			queue.push_back(n);
			std::push_heap(queue.begin(), queue.end(), later_t{});
		}

		item_t pop()
		{
			std::pop_heap(queue.begin(), queue.end(), later_t{});
			item_t n = queue.back();
			queue.pop_back();
			return n;
		}

		size_t bytes() const
		{
			return labels.capacity() * sizeof(label_t) + queue.capacity() * sizeof(item_t);
		}
	};

	struct hash_t
	{
		size_t operator()(const std::vector<node_id> &v) const
		{
			size_t h = 0;
			for(node_id x : v) {
				h = (h * 1099511628211ULL) ^ x;
			}
			return h;
		}
	};

	using entry_t = std::pair<std::vector<node_id>, std::unique_ptr<context_t>>;
	using list_t = std::list<entry_t>;
	size_t capacity;
	list_t lru;
	std::unordered_map<std::vector<node_id>, typename list_t::iterator, hash_t> index;
	context_t scratch;
	size_t live_bytes = 0, peak_bytes = 0;

public:
	explicit cache_t(size_t n) : capacity(n)
	{}

	void clear()
	{
		index.clear();
		lru.clear();
		scratch = context_t{};
		live_bytes = 0;
	}

	size_t bytes() const
	{
		return live_bytes;
	}

	size_t peak() const
	{
		return peak_bytes;
	}

	size_t count() const
	{
		return lru.size();
	}

	template <class Graph>
	result_t query(const Graph &g, std::vector<node_id> origins, const std::vector<node_id> &targets, uint32 max_transfers,
	      uint32 max_hops)
	{
		result_t result;
		if(origins.empty()  ||  targets.empty()) {
			return result;
		}
		if(max_transfers>=UINT16_MAX) {
			result.fallback = true;
			return result;
		}
		std::sort(origins.begin(), origins.end());
		origins.erase(std::unique(origins.begin(), origins.end()), origins.end());
		for(node_id id : origins) {
			if(!id  ||  id>=g.size()) {
				result.fallback = true;
				return result;
			}
		}
		for(node_id id : targets) {
			if(!id  ||  id>=g.size()) {
				result.fallback = true;
				return result;
			}
		}
		if(origins.size()>max_hops) {
			result.fallback = true;
			return result;
		}
		std::vector<node_id> key = origins;
		key.push_back(g.size());
		key.push_back(max_transfers);
		key.push_back(max_hops);
		context_t *ctx;
		size_t old_bytes = 0;
		if(!capacity) {
			old_bytes = scratch.bytes();
			scratch.reset(g.size(), origins);
			ctx = &scratch;
		} else {
			auto found = index.find(key);
			if(found!=index.end()) {
				lru.splice(lru.begin(), lru, found->second);
				ctx = lru.front().second.get();
				result.hit = true;
				old_bytes = ctx->bytes();
			} else {
				std::unique_ptr<context_t> context;
				if(lru.size()==capacity) {
					live_bytes -= lru.back().second->bytes();
					context = std::move(lru.back().second);
					index.erase(lru.back().first);
					lru.pop_back();
				} else {
					context = std::make_unique<context_t>();
				}
				context->reset(g.size(), origins);
				ctx = context.get();
				lru.emplace_front(key, std::move(context));
				index.emplace(std::move(key), lru.begin());
			}
		}
		// Keep using certified destinations after the context reaches its bound.
		// Unknown destinations fall back: a fresh targeted search may still succeed.
		auto search = [&]() {
			auto best_target = [&]() {
				node_id id = 0;
				uint32 best = infinity;
				for(node_id t : targets) {
					if(ctx->labels[t].distance<best  ||
					   (ctx->labels[t].distance==best  &&  best!=infinity  &&  t<id)) {
						id = t;
						best = ctx->labels[t].distance;
					}
				}
				return id;
			};
			node_id best = best_target();
			if(ctx->limited  &&  (!best  ||  ctx->labels[best].distance>=ctx->certified_distance)) {
				result.fallback = true;
				return;
			}
			while(!ctx->limited  &&  !ctx->queue.empty()) {
				const item_t front = ctx->queue.front();
				// Finish the entire equal-distance layer, including zero-weight edges,
				// so the selected destination/path is independent of previous queries.
				if(best  &&  ctx->labels[best].distance<front.distance) {
					break;
				}
				const item_t item = ctx->pop();
				++result.pops;
				const label_t label = ctx->labels[item.id];
				if(label.distance!=item.distance) {
					continue;
				}
				ctx->certified_distance = item.distance;
				if(label.depth>max_transfers) {
					ctx->limited = true;
					break;
				}
				for(const typename Graph::edge_t &edge : g.edges(item.id)) {
					++result.edges;
					const node_id to = g.target(edge);
					const uint32 weight = g.weight(edge);
					if(!to  ||  to>=ctx->labels.size()  ||  weight>infinity - label.distance) {
						continue;
					}
					label_t &next = ctx->labels[to];
					if(next.origin  ||  label.distance + weight>=next.distance) {
						continue;
					}
					// Like the existing search, bound queue insertions, not every edge
					// or terminal label. Non-transfer destinations need no queue entry.
					const bool enqueue = g.transfer(to);
					if(enqueue  &&  ctx->allocations>=max_hops) {
						ctx->limited = true;
						break;
					}
					if(enqueue) {
						++ctx->allocations;
					}
					next = {label.distance + weight, item.id, uint16(label.depth + 1), false};
					if(enqueue) {
						ctx->push({next.distance, to});
					}
				}
				best = best_target();
			}
			if(ctx->limited  &&  (!best  ||  ctx->labels[best].distance>=ctx->certified_distance)) {
				result.fallback = true;
				return;
			}
			if(!best) {
				return;
			}
			result.found = true;
			result.cost = ctx->labels[best].distance;
			for(node_id id = best; id; id = ctx->labels[id].previous) {
				result.path.push_back(id);
				if(result.path.size()>ctx->labels.size()) {
					result.found = false;
					result.fallback = true;
					result.path.clear();
					return;
				}
			}
			std::reverse(result.path.begin(), result.path.end());
			result.ready = result.hit  &&  result.pops==0;
		};
		search();
		if(capacity) {
			live_bytes += ctx->bytes() - old_bytes;
		} else {
			live_bytes = ctx->bytes();
		}
		peak_bytes = std::max(peak_bytes, live_bytes);
		return result;
	}
};
} // namespace route_tree
#endif
