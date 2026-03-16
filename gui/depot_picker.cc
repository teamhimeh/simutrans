/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "depot_picker.h"
#include "gui_theme.h"

#include "../player/simplay.h"
#include "../vehicle/simvehicle.h"
#include "../simdepot.h"
#include "../simskin.h"
#include "../simworld.h"
#include "../simconvoi.h"
#include "../dataobj/translator.h"
#include "../descriptor/skin_desc.h"
#include "../utils/cbuffer_t.h"


// ── depot_picker_item_t ──────────────────────────────────────────────────────

depot_picker_item_t::depot_picker_item_t(depot_t *d, convoihandle_t cnv, bool teleport)
	: depot(d), cnv(cnv), teleport(teleport)
{
	set_table_layout(3, 1);

	gotopos.set_typ(button_t::posbutton_automatic);
	gotopos.set_targetpos3d(depot->get_pos());
	add_component(&gotopos);

	switch (d->get_waytype()) {
		case maglev_wt:      waytype_symbol.set_image(skinverwaltung_t::maglevhaltsymbol->get_image_id(0), true);      break;
		case monorail_wt:    waytype_symbol.set_image(skinverwaltung_t::monorailhaltsymbol->get_image_id(0), true);    break;
		case track_wt:       waytype_symbol.set_image(skinverwaltung_t::zughaltsymbol->get_image_id(0), true);         break;
		case tram_wt:        waytype_symbol.set_image(skinverwaltung_t::tramhaltsymbol->get_image_id(0), true);        break;
		case narrowgauge_wt: waytype_symbol.set_image(skinverwaltung_t::narrowgaugehaltsymbol->get_image_id(0), true); break;
		case road_wt:        waytype_symbol.set_image(skinverwaltung_t::autohaltsymbol->get_image_id(0), true);        break;
		case water_wt:       waytype_symbol.set_image(skinverwaltung_t::schiffshaltsymbol->get_image_id(0), true);     break;
		case air_wt:         waytype_symbol.set_image(skinverwaltung_t::airhaltsymbol->get_image_id(0), true);         break;
		default: break;
	}
	add_component(&waytype_symbol);
	add_component(&label);
	update_label();
}


void depot_picker_item_t::update_label()
{
	cbuffer_t &buf = label.buf();
	buf.append(translator::translate(depot->get_name()));
	buf.printf(" %s", depot->get_pos().get_2d().get_fullstr());
	label.update();
}


void depot_picker_item_t::draw(scr_coord pos)
{
	update_label();
	gui_aligned_container_t::draw(pos);
}


bool depot_picker_item_t::is_valid() const
{
	return depot_t::get_depot_list().is_contained(depot);
}


bool depot_picker_item_t::infowin_event(const event_t *ev)
{
	bool swallowed = gui_aligned_container_t::infowin_event(ev);
	if (!swallowed && IS_LEFTRELEASE(ev) && cnv.is_bound()) {
		koord3d pos = depot->get_pos();
		char buf[64];
		sprintf(buf, "%d,%d,%d", pos.x, pos.y, pos.z);
		// 'D' = route to specific depot, 'Y' = teleport to specific depot
		cnv->call_convoi_tool(teleport ? 'Y' : 'D', buf);
		destroy_win(magic_depot_picker);
		swallowed = true;
	}
	return swallowed;
}


bool depot_picker_item_t::compare(const gui_component_t *aa, const gui_component_t *bb)
{
	const depot_picker_item_t *fa = dynamic_cast<const depot_picker_item_t*>(aa);
	const depot_picker_item_t *fb = dynamic_cast<const depot_picker_item_t*>(bb);
	assert(fa && fb);
	int cmp = koord_distance(fa->depot->get_pos(), koord(0, 0))
	        - koord_distance(fb->depot->get_pos(), koord(0, 0));
	if (cmp == 0) cmp = fa->depot->get_pos().x - fb->depot->get_pos().x;
	return cmp < 0;
}


// ── depot_picker_t ───────────────────────────────────────────────────────────

depot_picker_t::depot_picker_t(convoihandle_t cnv, bool teleport)
	: gui_frame_t(translator::translate(teleport ? "Teleport to depot" : "Go to depot"), cnv->get_owner()),
	  scrolly(gui_scrolled_list_t::windowskin, depot_picker_item_t::compare),
	  cnv(cnv), teleport(teleport)
{
	set_table_layout(1, 0);
	scrolly.set_maximize(true);
	add_component(&scrolly);
	fill_list();
	set_resizemode(diagonal_resize);
	reset_min_windowsize();
}


void depot_picker_t::fill_list()
{
	scrolly.clear_elements();
	if (!cnv.is_bound()) return;

	waytype_t wt    = cnv->front()->get_waytype();
	player_t *owner = cnv->get_owner();

	FOR(slist_tpl<depot_t*>, const depot, depot_t::get_depot_list()) {
		if (depot->get_owner() == owner && depot->get_waytype() == wt) {
			scrolly.new_component<depot_picker_item_t>(depot, cnv, teleport);
		}
	}
	scrolly.sort(0);
	scrolly.set_size(scrolly.get_size());
}
