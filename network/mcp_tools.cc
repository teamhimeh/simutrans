/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

/**
 * MCP tool implementations.
 *
 * run_squirrel accepts arbitrary Squirrel script code and executes it inside a
 * dedicated script VM that has the full Simutrans scripting API (ai_base)
 * available. capture_screen returns the current display as MCP image content.
 *
 * A fresh VM is created for each run_squirrel call and destroyed afterwards,
 * so no state (global variables, defined functions, etc.) leaks between calls.
 */

#include "mcp_tools.h"
#include "mcp_server.h"

#include <stdio.h>
#include <string.h>
#include <string>

#include "../script/script.h"
#include "../script/script_loader.h"
#include "../dataobj/environment.h"
#include "../dataobj/koord3d.h"
#include "../dataobj/ribi.h"
#include "../display/simgraph.h"
#include "../utils/cbuffer_t.h"
#include "../utils/plainstring.h"
#include "../simconst.h"
#include "../simworld.h"
#include "../boden/grund.h"
#include "../obj/simobj.h"
#include "../player/simplay.h"


// ---------------------------------------------------------------------------
// Minimal JSON helpers (output only)
// ---------------------------------------------------------------------------

static std::string jesc(const std::string &s) { return mcp_server_t::json_escape(s); }
static std::string jstr(const std::string &s)  { return "\"" + jesc(s) + "\""; }
static std::string jstr(const char *s)          { return s ? jstr(std::string(s)) : "null"; }

static std::string text_content(const std::string &text)
{
	return "{\"content\":[{\"type\":\"text\",\"text\":" + jstr(text) + "}]}";
}

static std::string base64_encode(const std::string &data)
{
	static const char chars[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string encoded;
	encoded.reserve(((data.size() + 2) / 3) * 4);

	for (size_t i = 0; i < data.size(); i += 3) {
		const uint32 a = (uint8)data[i];
		const uint32 b = i + 1 < data.size() ? (uint8)data[i + 1] : 0;
		const uint32 c = i + 2 < data.size() ? (uint8)data[i + 2] : 0;
		const uint32 value = (a << 16) | (b << 8) | c;

		encoded += chars[(value >> 18) & 0x3F];
		encoded += chars[(value >> 12) & 0x3F];
		encoded += i + 1 < data.size() ? chars[(value >> 6) & 0x3F] : '=';
		encoded += i + 2 < data.size() ? chars[value & 0x3F] : '=';
	}
	return encoded;
}


// ---------------------------------------------------------------------------
// Tool implementation
// ---------------------------------------------------------------------------

// Start a squirrel script execution.
// Returns result JSON on immediate completion.
// On suspension (network mode command_x), sets *out_vm to the live VM and returns "".
// On error, returns error JSON.
// Caller owns *out_vm and must eventually delete it.
static std::string tool_run_squirrel(const std::string &code, int player_nr,
                                     script_vm_t **out_vm)
{
	// Create a fresh VM for each call so no state leaks between invocations.
	cbuffer_t ai_path;
	ai_path.printf("%sai/", env_t::data_dir);

	script_vm_t *vm = script_loader_t::start_vm("ai_base.nut", "script-mcp.log",
	                                              ai_path, /*is_scenario=*/false,
	                                              /*enable_io=*/false);
	if (!vm) {
		return "{\"error\":\"failed to initialize Squirrel VM\"}";
	}
	vm->set_my_player((uint8)player_nr);
	vm->pause_on_error = false;

	// Wrap the user code in a named function so we can call it with
	// call_function<plainstring>(QUEUE, ...) and capture the return value.
	std::string wrapped =
		"function __mcp_fn__() {\n"
		+ code +
		"\n}";

	// Step 1: define __mcp_fn__ in the VM
	const char *err = vm->eval_string(wrapped.c_str());
	if (err && strcmp(err, "suspended") != 0) {
		std::string result_json = "{\"error\":" + jstr(err) + "}";
		delete vm;
		return result_json;
	}

	// Step 2: call __mcp_fn__ via QUEUE mode (allows command_x suspend in network mode)
	plainstring result;
	err = vm->call_function(script_vm_t::QUEUE, "__mcp_fn__", result);

	if (script_vm_t::is_call_suspended(err)) {
		// Script suspended waiting for a network command — hand VM to caller
		*out_vm = vm;
		return "";
	}

	std::string result_json;
	if (err) {
		result_json = "{\"error\":" + jstr(err) + "}";
	}
	else {
		result_json = "{\"result\":" + jstr(result.c_str()) + "}";
	}
	delete vm;
	return result_json;
}

// Reads an integer arg from a JSON args object. Returns false if the key is
// absent or JSON null, leaving *out untouched.
static bool json_get_int(const std::string &args_json, const std::string &key, int *out)
{
	std::string raw = mcp_server_t::json_get_raw(args_json, key);
	if (raw.empty() || raw == "null") {
		return false;
	}
	*out = atoi(raw.c_str());
	return true;
}

static const char *obj_type_name(obj_t::typ t)
{
	switch (t) {
		case obj_t::baum:                return "tree";
		case obj_t::gebaeude:            return "building";
		case obj_t::signal:              return "signal";
		case obj_t::bruecke:             return "bridge";
		case obj_t::tunnel:              return "tunnel";
		case obj_t::bahndepot:           return "rail_depot";
		case obj_t::strassendepot:       return "road_depot";
		case obj_t::schiffdepot:         return "ship_depot";
		case obj_t::leitung:             return "powerline";
		case obj_t::pumpe:               return "pump";
		case obj_t::senke:               return "sink";
		case obj_t::roadsign:            return "roadsign";
		case obj_t::pillar:              return "pillar";
		case obj_t::airdepot:            return "air_depot";
		case obj_t::monoraildepot:       return "monorail_depot";
		case obj_t::tramdepot:           return "tram_depot";
		case obj_t::maglevdepot:         return "maglev_depot";
		case obj_t::wayobj:              return "wayobj";
		case obj_t::way:                 return "way";
		case obj_t::label:               return "label";
		case obj_t::field:               return "field";
		case obj_t::crossing:            return "crossing";
		case obj_t::groundobj:           return "groundobj";
		case obj_t::narrowgaugedepot:    return "narrowgauge_depot";
		case obj_t::pedestrian:          return "pedestrian";
		case obj_t::road_user:           return "citycar";
		case obj_t::road_vehicle:        return "road_vehicle";
		case obj_t::rail_vehicle:        return "rail_vehicle";
		case obj_t::monorail_vehicle:    return "monorail_vehicle";
		case obj_t::maglev_vehicle:      return "maglev_vehicle";
		case obj_t::narrowgauge_vehicle: return "narrowgauge_vehicle";
		case obj_t::water_vehicle:       return "water_vehicle";
		case obj_t::air_vehicle:         return "air_vehicle";
		case obj_t::movingobj:           return "movingobj";
		default:                         return "other";
	}
}

static const char *ground_type_name(grund_t::typ t)
{
	switch (t) {
		case grund_t::boden:         return "boden";
		case grund_t::wasser:        return "wasser";
		case grund_t::fundament:     return "fundament";
		case grund_t::tunnelboden:   return "tunnelboden";
		case grund_t::brueckenboden: return "brueckenboden";
		case grund_t::monorailboden: return "monorailboden";
		default:                     return "unknown";
	}
}

static void append_slope_json(cbuffer_t &buf, slope_t::type sl)
{
	buf.printf(
		"{\"raw\":%d,\"flat\":%s,\"buildable_way\":%s,\"single\":%s,"
		"\"corners\":{\"sw\":%d,\"se\":%d,\"ne\":%d,\"nw\":%d}}",
		(int)sl,
		sl == slope_t::flat ? "true" : "false",
		slope_t::is_way(sl) ? "true" : "false",
		slope_t::is_single(sl) ? "true" : "false",
		(int)corner_sw(sl), (int)corner_se(sl), (int)corner_ne(sl), (int)corner_nw(sl));
}

// Reports ground type (flat/bridge/tunnel/elevated), slope, and the objects
// (with owners) present on a tile. If "z" is omitted, the surface tile is
// used; if given, that exact height is looked up (no silent fallback to the
// surface tile when nothing exists there).
static std::string tool_get_tile_info(karte_t *welt, const std::string &args_json)
{
	if (!welt) {
		return text_content("{\"error\":\"world not ready\"}");
	}

	int x = 0, y = 0, z = 0;
	bool has_x = json_get_int(args_json, "x", &x);
	bool has_y = json_get_int(args_json, "y", &y);
	bool has_z = json_get_int(args_json, "z", &z);
	if (!has_x || !has_y) {
		return text_content("{\"error\":\"x and y are required\"}");
	}

	koord k((sint16)x, (sint16)y);
	if (!welt->is_within_limits(k)) {
		return text_content("{\"error\":\"coordinate out of range\"}");
	}

	grund_t *gr = has_z ? welt->lookup(koord3d(k, (sint8)z)) : welt->lookup_kartenboden(k);
	if (!gr) {
		return text_content(has_z
			? "{\"error\":\"no ground tile at given height\",\"found\":false}"
			: "{\"error\":\"no ground tile\",\"found\":false}");
	}

	const koord3d pos = gr->get_pos();
	const bool is_elevated = gr->get_typ() == grund_t::monorailboden;
	const bool is_ground   = gr->ist_karten_boden();
	const bool is_tunnel   = gr->ist_tunnel();

	cbuffer_t buf;
	buf.printf("{\"found\":true,\"x\":%d,\"y\":%d,\"z\":%d,", pos.x, pos.y, pos.z);
	buf.printf("\"ground_type\":%s,", jstr(ground_type_name(gr->get_typ())).c_str());
	buf.printf("\"is_water\":%s,",       gr->is_water()   ? "true" : "false");
	buf.printf("\"is_ground\":%s,",      is_ground        ? "true" : "false");
	buf.printf("\"is_bridge\":%s,",      gr->ist_bruecke() ? "true" : "false");
	buf.printf("\"is_tunnel\":%s,",      is_tunnel        ? "true" : "false");
	buf.printf("\"is_elevated\":%s,",    is_elevated      ? "true" : "false");
	buf.printf("\"is_underground\":%s,", (is_tunnel && !is_ground) ? "true" : "false");

	buf.printf("\"slope\":");
	append_slope_json(buf, gr->get_grund_hang());
	buf.printf(",\"way_slope\":");
	append_slope_json(buf, gr->get_weg_hang());
	buf.printf(",");

	buf.printf("\"objects\":[");
	const uint8 top = gr->get_top();
	for (uint8 i = 0; i < top; i++) {
		obj_t *obj = gr->obj_bei(i);
		if (!obj) {
			continue;
		}
		if (i > 0) {
			buf.printf(",");
		}
		player_t *owner = obj->get_owner();
		buf.printf("{\"index\":%d,\"type\":%d,\"type_name\":%s,\"name\":%s,\"owner\":",
			(int)i, (int)obj->get_typ(),
			jstr(obj_type_name(obj->get_typ())).c_str(),
			jstr(obj->get_name()).c_str());
		if (owner) {
			buf.printf("%d", (int)owner->get_player_nr());
		}
		else {
			buf.printf("null");
		}
		buf.printf("}");
	}
	buf.printf("]}");

	return text_content((const char *)buf);
}

static std::string tool_capture_screen()
{
	std::string png_data;
	const scr_rect screen_area(0, 0, display_get_width(), display_get_height());
	if (!display_snapshot_png(screen_area, png_data)) {
		return text_content("{\"error\":\"failed to capture the Simutrans screen\"}");
	}

	return "{\"content\":[{\"type\":\"image\",\"data\":"
		+ jstr(base64_encode(png_data))
		+ ",\"mimeType\":\"image/png\"}]}";
}


// ---------------------------------------------------------------------------
// Tool registry
// ---------------------------------------------------------------------------

struct tool_def_t {
	const char *name;
	const char *description;
	const char *input_schema_json;
};

static const tool_def_t TOOL_DEFS[] = {
	{
		"run_squirrel",
		"Execute Squirrel script code with the full Simutrans scripting API "
		"(world, player, halt, line, convoy, factory, command_x, etc.) available. "
		"The script should return a value; it is returned as a string result. "
		"Each call runs in a fresh VM; no state persists between calls.",
		"{\"type\":\"object\","
		 "\"properties\":{"
		   "\"code\":{\"type\":\"string\","
		              "\"description\":\"Squirrel script code to execute. "
		              "Use 'return' to pass a value back.\"},"
		   "\"player_nr\":{\"type\":\"integer\","
		                   "\"description\":\"Player context index (0-15), default 1\"}"
		 "},"
		 "\"required\":[\"code\"]}"
	},
	{
		"capture_screen",
		"Capture the current Simutrans window as a PNG image. "
		"Use this to inspect the actual on-screen game view and GUI state.",
		"{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}"
	},
	{
		"get_tile_info",
		"Inspect a single map tile: ground type (flat ground/bridge/tunnel/elevated way), "
		"slope of the ground and of the way on it (with buildability flags), and the list "
		"of objects present (ways, buildings, signs, vehicles, ...) with their owning player. "
		"x,y select the 2D map position. z is optional: if omitted, the surface (kartenboden) "
		"tile is used; if given, that exact height is looked up and an error is returned if "
		"there is no ground there (it does NOT silently fall back to the surface tile), which "
		"lets you distinguish e.g. an elevated way tile from the ground below it.",
		"{\"type\":\"object\","
		 "\"properties\":{"
		   "\"x\":{\"type\":\"integer\",\"description\":\"map x coordinate\"},"
		   "\"y\":{\"type\":\"integer\",\"description\":\"map y coordinate\"},"
		   "\"z\":{\"type\":\"integer\",\"description\":\"optional exact tile height; omit for the surface tile\"}"
		 "},"
		 "\"required\":[\"x\",\"y\"]}"
	}
};
static const int NUM_TOOLS = sizeof(TOOL_DEFS) / sizeof(TOOL_DEFS[0]);


// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

std::string mcp_tools::tools_list_json()
{
	std::string arr = "[";
	for (int i = 0; i < NUM_TOOLS; i++) {
		if (i > 0) arr += ',';
		arr += std::string("{")
			+ "\"name\":"        + jstr(TOOL_DEFS[i].name)        + ","
			+ "\"description\":" + jstr(TOOL_DEFS[i].description) + ","
			+ "\"inputSchema\":"  + TOOL_DEFS[i].input_schema_json
			+ "}";
	}
	return arr + "]";
}


std::string mcp_tools::tools_call(const std::string &name,
                                   const std::string &args_json,
                                   karte_t           *welt,
                                   script_vm_t      **out_pending_vm)
{
	if (name == "run_squirrel") {
		std::string code     = mcp_server_t::json_get_string(args_json, "code");
		std::string player_s = mcp_server_t::json_get_raw(args_json, "player_nr");
		int player_nr = player_s.empty() ? 1 : atoi(player_s.c_str());
		if (player_nr < 0 || player_nr > MAX_PLAYER_COUNT-1) player_nr = 1;
		script_vm_t *pending_vm = nullptr;
		std::string result = tool_run_squirrel(code, player_nr, &pending_vm);
		if (out_pending_vm) {
			*out_pending_vm = pending_vm;
		}
		else if (pending_vm) {
			// Caller doesn't support async — treat as error
			delete pending_vm;
			return text_content("{\"error\":\"script suspended but caller does not support async\"}");
		}
		return text_content(result);
	}

	if (name == "capture_screen") {
		return tool_capture_screen();
	}

	if (name == "get_tile_info") {
		return tool_get_tile_info(welt, args_json);
	}

	return text_content("{\"error\":\"unknown tool: " + jesc(name) + "\"}");
}
