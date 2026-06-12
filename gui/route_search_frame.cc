#include "route_search_frame.h"
#include "../dataobj/schedule.h"
#include "../dataobj/translator.h"
#include "../simplan.h"
#include "components/gui_divider.h"
#include "components/gui_image.h"
#include "minimap.h"
#include "../simconvoi.h"
#include "../simhalt.h"
#include "../simline.h"
#include "../player/simplay.h"
#include "../simware.h"
#include "../simworld.h"
#include <variant>
#include <cstdio>

class gui_traveler_button_t : public button_t, public action_listener_t
{
    linehandle_t line;
public:
    gui_traveler_button_t(linehandle_t line) : button_t()
    {
        this->line = line;
        init(button_t::posbutton, NULL);
        add_listener(this);
    }

    bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE
    {
        player_t *player = world()->get_active_player();
        if(  player == line->get_owner()  ) {
            player->simlinemgmt.show_lineinfo(player, line);
        }
        return true;
    }

    void draw(scr_coord offset) OVERRIDE
    {
        button_t::draw(offset);
        enable(line.is_bound()  &&  line->get_owner() == world()->get_active_player());
    }
};

class gui_halt_button_t : public button_t, public action_listener_t {
    halthandle_t halt;
public:
    gui_halt_button_t(halthandle_t halt) : button_t()
    {
        this->halt = halt;
        init(button_t::posbutton, NULL);
        add_listener(this);
    }

    bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE
    {
        halt->open_info_window();
        return true;
    }
};

route_search_frame_t::route_search_frame_t()
: gui_frame_t( translator::translate("Pax route search") ),
from_halt_label("From halt:"),
dest_halt_label("To halt:"),
from_koord_label("From (x,y):"),
dest_koord_label("To (x,y):"),
result_container(1, 0),
from_koord(koord::invalid),
dest_koord(koord::invalid)
{
    snprintf(from_halt_input_text, lengthof(from_halt_input_text), "");
    snprintf(dest_halt_input_text, lengthof(dest_halt_input_text), "");
    snprintf(from_koord_text, lengthof(from_koord_text), "");
    snprintf(dest_koord_text, lengthof(dest_koord_text), "");

    set_table_layout(1,0);
    {
		viewable_freight_types.append(goods_manager_t::passengers);
		freight_type_c.new_component<gui_scrolled_list_t::const_text_scrollitem_t>( translator::translate("Passagiere"), SYSCOL_TEXT) ;
		viewable_freight_types.append(goods_manager_t::mail);
		freight_type_c.new_component<gui_scrolled_list_t::const_text_scrollitem_t>( translator::translate("Post"), SYSCOL_TEXT) ;
		for(  int i = 0;  i < goods_manager_t::get_max_catg_index();  i++  ) {
			const goods_desc_t *freight_type = goods_manager_t::get_info_catg(i);
			const int index = freight_type->get_catg_index();
			if(  index == goods_manager_t::INDEX_NONE  ||  freight_type->get_catg()==0  ) {
				continue;
			}
			freight_type_c.new_component<gui_scrolled_list_t::const_text_scrollitem_t>(translator::translate(freight_type->get_catg_name()), SYSCOL_TEXT);
			viewable_freight_types.append(freight_type);
		}
		for(  int i=0;  i < goods_manager_t::get_count();  i++  ) {
			const goods_desc_t *ware = goods_manager_t::get_info(i);
			if(  ware->get_catg() == 0  &&  ware->get_index() > 2  ) {
				viewable_freight_types.append(ware);
				freight_type_c.new_component<gui_scrolled_list_t::const_text_scrollitem_t>( translator::translate(ware->get_name()), SYSCOL_TEXT) ;
			}
		}
	}
	freight_type_c.set_selection(0);
    search_ware_index=0;
	freight_type_c.set_focusable( true );
	freight_type_c.add_listener( this );

    // Row 1: halt name inputs
    add_table(4, 1);
    {
        from_halt_input.set_text(from_halt_input_text, lengthof(from_halt_input_text));
        dest_halt_input.set_text(dest_halt_input_text, lengthof(dest_halt_input_text));
        add_component(&from_halt_label);
        add_component(&from_halt_input);
        add_component(&dest_halt_label);
        add_component(&dest_halt_input);
    }
    end_table();

    // Row 2: coordinate inputs (takes precedence over halt name when non-empty)
    add_table(4, 1);
    {
        from_koord_input.set_text(from_koord_text, lengthof(from_koord_text));
        dest_koord_input.set_text(dest_koord_text, lengthof(dest_koord_text));
        add_component(&from_koord_label);
        add_component(&from_koord_input);
        add_component(&dest_koord_label);
        add_component(&dest_koord_input);
    }
    end_table();

    add_table(3, 2);
    {
        search_button.init(button_t::roundbox, "Search");
        search_button.add_listener(this);
        add_component(&search_button);

        reverse_search_button.init(button_t::roundbox, "Swap");
        reverse_search_button.add_listener(this);
        add_component(&reverse_search_button);

        add_component(&freight_type_c);

        bt_show_non_traveled.init(button_t::square_state, "Show Non-Traveled Section");
        bt_show_non_traveled.pressed = true;
        bt_show_non_traveled.add_listener(this);
        add_component(&bt_show_non_traveled);
    }
    end_table();

    new_component<gui_divider_t>();

    result_container.set_table_layout(1,0);
    add_component(&result_container);
    result_container.new_component<gui_label_t>("Enter halt names or tile coordinates (x,y) and press Search.");

    set_resizemode(diagonal_resize);
    reset_min_windowsize();
}

route_search_frame_t::~route_search_frame_t()
{
    minimap_t::get_instance()->set_selected_cnv( convoihandle_t(), true );
    result_container.remove_all();
}

bool route_search_frame_t::action_triggered(gui_action_creator_t* comp, value_t) {
    if(  comp==&search_button  ) {
        search_route();
    } else if(  comp==&reverse_search_button  ) {
        swap_halt_inputs();
        search_route();
    } else if (  comp == &freight_type_c  ) {
        const goods_desc_t *ware = viewable_freight_types[freight_type_c.get_selection()];
        search_ware_index = ware->get_index();
	} else if (  comp == &bt_show_non_traveled  ) {
        bt_show_non_traveled.pressed ^= 1;
        if (  from_halt.is_bound() && dest_halt.is_bound()  ) search_route();
    }
    return true;
}

halthandle_t find_halt(const char* name) {
    FOR(vector_tpl<halthandle_t>, const h, haltestelle_t::get_alle_haltestellen()) {
        if(  strcmp(h->get_name(), name)==0  ) {
            return h;
        }
    }
    return halthandle_t();
}

haltestelle_t::connection_t find_connection(halthandle_t from, halthandle_t to, const uint8 ware_index) {
    FOR(vector_tpl<haltestelle_t::connection_t>, const c, from->get_connections(ware_index)) {
        if(  c.halt.is_bound()  &&  c.halt==to  ) {
            return c;
        }
    }
    return haltestelle_t::connection_t();
}

void route_search_frame_t::append_connection_row(haltestelle_t::connection_t connection, halthandle_t connection_from_halt) {
    if(  connection.weight==0  ) {
        result_container.new_component<gui_label_t>("No connection found!");
        return;
    }
    result_container.add_table(4, 1);

    // construct weight text
    char text[16];
    uint32 weight;
    ware_t dummy_ware = ware_t();
    dummy_ware.menge = 100;
    dummy_ware.index = search_ware_index;
    if(  world()->get_settings().get_time_based_routing_enabled(dummy_ware.get_desc()->get_catg_index())  ) {
        // When TBGR is enabled for pax, weight is in ticks. We have to convert it to the OTRP departure time unit.
        weight = (uint64)connection.weight * world()->get_settings().get_spacing_shift_divisor() / world()->ticks_per_world_month;
    } else {
        // weight is in route cost.
        weight = connection.weight;
    }
    snprintf(text, 16, "<%d>", weight);
    auto label_with_buf = result_container.new_component<gui_label_buf_t>();
    label_with_buf->buf().append(text);

    if(  connection.is_foot_path  ) {
        // foot-path connection: no vehicle or line, just show walk indicator
        result_container.new_component<gui_empty_t>();
        result_container.new_component<gui_empty_t>();
        result_container.new_component<gui_label_t>("(walk)");
        result_container.end_table();
        return;
    }

    linehandle_t result_line = std::holds_alternative<linehandle_t>(connection.best_weight_traveler) ?
        std::get<linehandle_t>(connection.best_weight_traveler) : linehandle_t();
    result_container.new_component<gui_traveler_button_t>(result_line);
    convoihandle_t cnv = result_line.is_bound()?(  result_line->count_convoys()>0 ? result_line->get_convoy(0) : convoihandle_t()  ) : (std::holds_alternative<convoihandle_t>(connection.best_weight_traveler) ? std::get<convoihandle_t>(connection.best_weight_traveler) : convoihandle_t());

    if (  cnv.is_bound()  ) {
        auto original_sched = cnv->get_schedule();

        schedule_t* spliced_schedule = original_sched->copy();
        spliced_schedule->remove_all();
        
        bool recording = false;
        bool is_end_halt = false;

        int start_idx = -1;
        int end_idx = -1;
        uint8 count = original_sched->get_count();

        for (uint8 i = 0; i < count; i++) {
            halthandle_t halt = haltestelle_t::get_stoppable_halt(original_sched->at(i).pos, cnv->get_owner(), original_sched->get_waytype());
            if (halt == connection_from_halt) {
                if (start_idx == -1) {
                    start_idx = i;
                } else if (end_idx != -1) {
                    uint8 current_dist = (end_idx - start_idx + count) % count;
                    uint8 new_dist = (end_idx - i + count) % count;
                    if (new_dist < current_dist) start_idx = i;
                }
            }

            if (halt == connection.halt) {
                if (end_idx == -1) {
                    end_idx = i;
                } else if (start_idx != -1) {
                    uint8 current_dist = (end_idx - start_idx + count) % count;
                    uint8 new_dist = (i - start_idx + count) % count;
                    if (new_dist < current_dist) end_idx = i;
                }
            }
        }

        if (start_idx != -1 && end_idx != -1) {
            int i = start_idx;
            while (true) {
                grund_t* gr = world()->lookup(original_sched->at(i).pos);
                if (gr) {
                    spliced_schedule->append(gr);
                }

                if (i == end_idx) break;
            
                i = (i + 1) % count;
                
                if (i == start_idx) break;
            }
        } else {
            result_container.end_table();
            return;
        }

        halthandle_t halt_start = haltestelle_t::get_stoppable_halt(original_sched->at(start_idx).pos, cnv->get_owner(), original_sched->get_waytype());
        halthandle_t halt_end = haltestelle_t::get_stoppable_halt(original_sched->at(end_idx).pos, cnv->get_owner(), original_sched->get_waytype());

        if(  halt_start != from_halt && halt_start != dest_halt  ) minimap_t::get_instance()->add_transfer_halt(halt_start);
        if(  halt_end != from_halt && halt_end != dest_halt  ) minimap_t::get_instance()->add_transfer_halt(halt_end);

        spliced_schedule->add_return_way();

        spliced_schedule->set_minimap_route_search_found(true);
        cnv->get_schedule()->set_minimap_route_search_found(true);

        if (original_sched->get_entries() == spliced_schedule->get_entries()) {
            minimap_t::get_instance()->set_selected_route( spliced_schedule, cnv->get_owner(), true, false );
        } else {
            if (  bt_show_non_traveled.pressed  ) minimap_t::get_instance()->set_selected_route( cnv->get_schedule(), cnv->get_owner(), false, false );
            minimap_t::get_instance()->set_selected_route( spliced_schedule, cnv->get_owner(), true, false );
        }
    }

    // Set icon
    const waytype_t waytype = std::visit([&](const auto& t) {
        return t->get_schedule()->get_waytype();
    }, connection.best_weight_traveler);
    gui_image_t *im = NULL;
	switch(waytype) {
		case road_wt: im = result_container.new_component<gui_image_t>(skinverwaltung_t::bushaltsymbol->get_image_id(0)); break;
		case track_wt: im = result_container.new_component<gui_image_t>(skinverwaltung_t::zughaltsymbol->get_image_id(0)); break;
        case tram_wt: im = result_container.new_component<gui_image_t>(skinverwaltung_t::tramhaltsymbol->get_image_id(0)); break;
		case water_wt: im = result_container.new_component<gui_image_t>(skinverwaltung_t::schiffshaltsymbol->get_image_id(0)); break;
		case air_wt: im = result_container.new_component<gui_image_t>(skinverwaltung_t::airhaltsymbol->get_image_id(0)); break;
		case monorail_wt: im = result_container.new_component<gui_image_t>(skinverwaltung_t::monorailhaltsymbol->get_image_id(0)); break;
		case maglev_wt: im = result_container.new_component<gui_image_t>(skinverwaltung_t::maglevhaltsymbol->get_image_id(0)); break;
		case narrowgauge_wt: im = result_container.new_component<gui_image_t>(skinverwaltung_t::narrowgaugehaltsymbol->get_image_id(0)); break;
		default:             result_container.new_component<gui_empty_t>(); break;
	}
	if (im) {
		im->enable_offset_removal(true);
	}

    // Set traveler name
    const char* best_weight_traveler_name = std::visit([&](const auto& t) {
        return t->get_name();
    }, connection.best_weight_traveler);
    const player_t* player = std::visit([&](const auto& t) {
        return t->get_owner();
    }, connection.best_weight_traveler);
    const uint8 color_idx = player->get_player_color1();
    result_container.new_component<gui_label_t>(best_weight_traveler_name, color_idx_to_rgb(color_idx));

    result_container.end_table();
}

void route_search_frame_t::append_halt_row(halthandle_t halt) {
    result_container.add_table(2, 1);
    result_container.new_component<gui_halt_button_t>(halt);
    result_container.new_component<gui_label_t>(halt->get_name());
    result_container.end_table();
}

void route_search_frame_t::append_pos_row(koord pos) {
    result_container.add_table(2, 1);
    result_container.new_component<gui_empty_t>(); // alignment placeholder (halt_row has a button here)
    char buf[32];
    snprintf(buf, sizeof(buf), "(%d, %d)", pos.x, pos.y);
    auto *lbl = result_container.new_component<gui_label_buf_t>();
    lbl->buf().append(buf);
    result_container.end_table();
}

// static
koord route_search_frame_t::parse_koord(const char* text)
{
    if(!text || text[0] == '\0') return koord::invalid;
    int x = -1, y = -1;
    if(sscanf(text, "%d,%d", &x, &y) == 2 && x >= 0 && y >= 0) {
        return koord(x, y);
    }
    return koord::invalid;
}

void route_search_frame_t::search_route() {
	// reset selection
    minimap_t::get_instance()->set_selected_cnv( convoihandle_t(), true );
    minimap_t::get_instance()->set_selected_route( nullptr, nullptr, true );
    result_container.remove_all();

    // Parse coordinate inputs; they take precedence over halt-name inputs when non-empty.
    from_koord = parse_koord(from_koord_input.get_text());
    dest_koord = parse_koord(dest_koord_input.get_text());

    // Helper: collect all enabled halts covering a tile position.
    auto collect_halts = [&](koord pos, vector_tpl<halthandle_t>& out) {
        const planquadrat_t *plan = world()->access(pos);
        if(plan) {
            for(uint h = 0; h < plan->get_haltlist_count(); h++) {
                halthandle_t h2 = plan->get_haltlist()[h];
                if(h2.is_bound() && h2->is_enabled(search_ware_index)) {
                    out.append_unique(h2);
                }
            }
        }
    };

    // Resolve from-position and collect all candidate start halts.
    static vector_tpl<halthandle_t> start_halts(16);
    start_halts.clear();
    koord from_pos;
    if((from_koord != koord::invalid)) {
        from_pos = from_koord;
        collect_halts(from_pos, start_halts);
        if(start_halts.empty()) {
            result_container.new_component<gui_label_t>("No halt found at From coordinate.");
            return;
        }
    }
    else {
        from_halt = find_halt(from_halt_input.get_text());
        if(!from_halt.is_bound()) {
            result_container.new_component<gui_label_t>("From halt not found.");
            return;
        }
        from_pos = from_halt->get_init_pos();
        start_halts.append(from_halt);
    }

    // Resolve destination position.
    // When dest_koord is given: leave dummy_ware.ziel unbound so search_route evaluates ALL halts
    // near dest_koord and picks the one with the lowest total route cost (transit + walking).
    // When halt-name is given: fix the destination as before (backward-compatible).
    koord dest_pos_for_search;
    bool fix_dest = false;
    if((dest_koord != koord::invalid)) {
        dest_pos_for_search = dest_koord;
        // Validate that at least one enabled halt exists near dest_koord.
        static vector_tpl<halthandle_t> dest_halts_tmp(8);
        dest_halts_tmp.clear();
        collect_halts(dest_koord, dest_halts_tmp);
        if(dest_halts_tmp.empty()) {
            result_container.new_component<gui_label_t>("No halt found at To coordinate.");
            return;
        }
        dest_halt = dest_halts_tmp[0]; // preliminary; updated after search
    }
    else {
        dest_halt = find_halt(dest_halt_input.get_text());
        if(!dest_halt.is_bound()) {
            result_container.new_component<gui_label_t>("To halt not found.");
            return;
        }
        dest_pos_for_search = dest_halt->get_init_pos();
        fix_dest = true;
    }

    ware_t dummy_ware = ware_t();
    dummy_ware.menge = 0;
    dummy_ware.index = search_ware_index;
    dummy_ware.set_zielpos(dest_pos_for_search);
    if(fix_dest) {
        // Halt-name mode: pin the destination so only this halt is considered.
        dummy_ware.set_ziel(dest_halt);
    }
    // Koord mode: ziel stays unbound — search_route discovers the best destination halt.

    // Use return_ware so we can learn the actual start halt chosen by the routing algorithm.
    // When multiple start halts exist (koord mode), routing picks the one with the lowest
    // total cost (walking to halt + transit), not simply the nearest one.
    ware_t return_ware;
    haltestelle_t::search_route(start_halts.begin(), (uint16)start_halts.get_count(), false, dummy_ware,
                                &return_ware, from_pos);

    // Update from_halt / dest_halt to what routing actually chose.
    if(return_ware.get_ziel().is_bound()) {
        from_halt = return_ware.get_ziel();
    }
    if(!fix_dest && dummy_ware.get_ziel().is_bound()) {
        dest_halt = dummy_ware.get_ziel();
    }

    minimap_t::get_instance()->set_from_dest_halt(from_halt, dest_halt);

    if(  !dummy_ware.get_ziel().is_bound()  ) {
        result_container.new_component<gui_label_t>("No route found!");
        return;
    }

    const uint8 ware_catg_idx = dummy_ware.get_desc()->get_catg_index();
    const bool tbgr = world()->get_settings().get_time_based_routing_enabled(ware_catg_idx);
    const bool add_walk = world()->get_settings().is_transit_by_foot()
                          && world()->get_settings().is_walk_cost_to_halt();
    const uint32 walk_factor = tbgr ? world()->get_settings().get_foot_path_time_ticks()
                                    : world()->get_settings().get_foot_path_weight();

    // Pre-compute walking costs so they can be both displayed and counted in the total.
    const uint32 origin_walk_raw = (add_walk && (from_koord != koord::invalid))
        ? koord_distance(from_pos, from_halt->get_init_pos()) * walk_factor : 0u;
    const uint32 dest_walk_raw   = (add_walk && (dest_koord != koord::invalid))
        ? koord_distance(dest_halt->get_init_pos(), dest_pos_for_search) * walk_factor : 0u;

    // Helper: emit a walking-leg connection row using the existing foot-path display code.
    // append_connection_row handles is_foot_path=true without needing a real convoy/line.
    auto append_walk_leg = [&](uint32 raw_weight) {
        haltestelle_t::connection_t wc;
        wc.is_foot_path = true;
        wc.weight = raw_weight;  // same units as transit connection weights
        append_connection_row(wc, halthandle_t());
    };

    uint64 total_raw = origin_walk_raw;

    // Origin leg: position → start halt (only when walking and koord given).
    if(origin_walk_raw > 0) {
        append_pos_row(from_pos);
        append_walk_leg(origin_walk_raw);
    }

    append_halt_row(from_halt);
    halthandle_t transit_from = from_halt;
    FOR(vector_tpl<halthandle_t>, const h, dummy_ware.get_transit_halts()) {
        auto connection = find_connection(transit_from, h, ware_catg_idx);
        total_raw += connection.weight;
        append_connection_row(connection, transit_from);
        append_halt_row(h);
        transit_from = h;
    }

    // Destination leg: dest halt → position (only when walking and koord given).
    if(dest_walk_raw > 0) {
        append_walk_leg(dest_walk_raw);
        append_pos_row(dest_pos_for_search);
    }
    total_raw += dest_walk_raw;

    // Total journey cost row.
    result_container.new_component<gui_divider_t>();
    result_container.add_table(2, 1);
    {
        result_container.new_component<gui_label_t>("Total:");

        // Apply the same unit conversion used by append_connection_row.
        uint64 display_total;
        if(tbgr) {
            display_total = total_raw * world()->get_settings().get_spacing_shift_divisor()
                            / world()->ticks_per_world_month;
        } else {
            display_total = total_raw;
        }
        char buf[32];
        snprintf(buf, sizeof(buf), "<%llu>", (unsigned long long)display_total);
        auto *lbl = result_container.new_component<gui_label_buf_t>();
        lbl->buf().append(buf);
    }
    result_container.end_table();

    reset_min_windowsize();
}

void route_search_frame_t::swap_halt_inputs() {
    char temp[256];
    strcpy(temp, from_halt_input_text);
    strcpy(from_halt_input_text, dest_halt_input_text);
    strcpy(dest_halt_input_text, temp);
    from_halt_input.set_text(from_halt_input_text, lengthof(from_halt_input_text));
    dest_halt_input.set_text(dest_halt_input_text, lengthof(dest_halt_input_text));

    char temp2[64];
    strcpy(temp2, from_koord_text);
    strcpy(from_koord_text, dest_koord_text);
    strcpy(dest_koord_text, temp2);
    from_koord_input.set_text(from_koord_text, lengthof(from_koord_text));
    dest_koord_input.set_text(dest_koord_text, lengthof(dest_koord_text));
}
