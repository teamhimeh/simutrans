#include <string>
#include "../../dataobj/tabfile.h"
#include "../roadsign_desc.h"
#include "obj_node.h"
#include "text_writer.h"
#include "imagelist_writer.h"
#include "roadsign_writer.h"
#include "get_waytype.h"
#include "skin_writer.h"

using std::string;

void roadsign_writer_t::write_obj(FILE* fp, obj_node_t& parent, tabfileobj_t& obj)
{
	obj_node_t node(this, 16, &parent);

	uint32                 const price       = obj.get_int("cost",        500) * 100;
	uint16                 const min_speed   = obj.get_int("min_speed",     0);
	sint8                  const offset_left = obj.get_int("offset_left",  14);
	uint8                  const wtyp        = get_waytype(obj.get("waytype"));
	roadsign_desc_t::types const flags     =
		(obj.get_int("single_way",         0) > 0 ? roadsign_desc_t::ONE_WAY               : roadsign_desc_t::NONE) |
		(obj.get_int("free_route",         0) > 0 ? roadsign_desc_t::CHOOSE_SIGN           : roadsign_desc_t::NONE) |
		(obj.get_int("is_private",         0) > 0 ? roadsign_desc_t::PRIVATE_ROAD          : roadsign_desc_t::NONE) |
		(obj.get_int("is_signal",          0) > 0 ? roadsign_desc_t::SIGN_SIGNAL           : roadsign_desc_t::NONE) |
		(obj.get_int("is_presignal",       0) > 0 ? roadsign_desc_t::SIGN_PRE_SIGNAL       : roadsign_desc_t::NONE) |
		(obj.get_int("no_foreground",      0) > 0 ? roadsign_desc_t::ONLY_BACKIMAGE        : roadsign_desc_t::NONE) |
		(obj.get_int("is_longblocksignal", 0) > 0 ? roadsign_desc_t::SIGN_LONGBLOCK_SIGNAL : roadsign_desc_t::NONE) |
		(obj.get_int("end_of_choose",      0) > 0 ? roadsign_desc_t::END_OF_CHOOSE_AREA    : roadsign_desc_t::NONE);

	string str = obj.get("image[0][n][0]");
	uint8 image_type;
	if(  !str.empty()  ) {
		image_type = roadsign_desc_t::USE_DIAGONAL;
	} else {
		str = obj.get("frontimage[0]");
		if(  str.empty()  &&  (flags&roadsign_desc_t::ONLY_BACKIMAGE)==0  ) {
			image_type = roadsign_desc_t::SET_LAYER_AUTOMATIC;
		} else {
			image_type = roadsign_desc_t::USE_FRONTIMAGE;
		}
	}
	// Hajo: write version data
	node.write_uint16(fp, 0x8005,         0); // version 5
	node.write_uint16(fp, min_speed,      2);
	node.write_uint32(fp, price,          4);
	node.write_uint8 (fp, flags,          8);
	node.write_uint8 (fp, offset_left,    9);
	node.write_uint8 (fp, wtyp,          10);
	node.write_uint8 (fp, image_type,    11);

	uint16 intro_date = obj.get_int("intro_year", DEFAULT_INTRO_DATE) * 12;
	intro_date += obj.get_int("intro_month", 1) - 1;
	node.write_uint16(fp, intro_date, 12);

	uint16 retire_date = obj.get_int("retire_year", DEFAULT_RETIRE_DATE) * 12;
	retire_date += obj.get_int("retire_month", 1) - 1;
	node.write_uint16(fp, retire_date, 14);

	write_head(fp, node, obj);

	// add the images
	slist_tpl<string> keys, front_keys;

	if(  image_type==roadsign_desc_t::USE_DIAGONAL  ) {
		// private sign and traffic signal cannot use this mode.
		if(  flags&roadsign_desc_t::PRIVATE_ROAD  ||  (flags&roadsign_desc_t::SIGN_SIGNAL  &&  wtyp==get_waytype("road"))  ) {
			dbg->fatal("roadsign_writer_t::write_obj", "private sign and traffic signal cannot use diagonal images.");
		}
		// examine whether there are images for winter.
		uint8 num_of_seasons = 1;
		str = obj.get("image[0][n][1]");
		if(  !str.empty()  ) num_of_seasons = 2;
		uint8 num_of_status = 1;
		if(  flags&roadsign_desc_t::SIGN_PRE_SIGNAL  ) {
			num_of_status = 3;
		} else if(  flags&roadsign_desc_t::SIGN_SIGNAL  ) {
			num_of_status = 2;
		}
		const char* const normal_ribi_codes[4] = {"n", "e",  "s",  "w"};
		const char* const slope_ribi_codes[4] = {"3", "6", "9", "12"};
		const char* const slope_head[4] = {"up","down","up2","down2"};
		const char* const diagonal_ribi_codes[8] = {"sw", "ws", "nw", "wn", "ne", "en", "se", "es"};
		char buf[40];
		for(uint8 season=0; season<num_of_seasons; season++) {
			for(uint8 status=0; status<num_of_status; status++) {
				// append flat images.
				for(uint8 i=0; i<4; i++) {
					sprintf(buf,"image[%d][%s][%d]", status, normal_ribi_codes[i], season);
					str = obj.get(buf);
					keys.append(str);
					sprintf(buf,"frontimage[%d][%s][%d]", status, normal_ribi_codes[i], season);
					str = obj.get(buf);
					front_keys.append(str);
				}
				// append slope images.
				for(uint8 h=0; h<4; h++) {
					for(uint8 i=0; i<4; i++) {
						sprintf(buf,"image%s[%d][%s][%d]", slope_head[h], status, slope_ribi_codes[i], season);
						str = obj.get(buf);
						keys.append(str);
						sprintf(buf,"frontimage%s[%d][%s][%d]", slope_head[h], status, slope_ribi_codes[i], season);
						str = obj.get(buf);
						front_keys.append(str);
					}
				}
				// append diagonal images.
				for(uint8 i=0; i<8; i++) {
					sprintf(buf,"diagonal[%d][%s][%d]", status, diagonal_ribi_codes[i], season);
					str = obj.get(buf);
					keys.append(str);
					sprintf(buf,"frontdiagonal[%d][%s][%d]", status, diagonal_ribi_codes[i], season);
					str = obj.get(buf);
					front_keys.append(str);
				}
			}
		}
	} else {
		for (int i = 0; i < 24; i++) {
			char buf[40];
			sprintf(buf, "image[%i]", i);
			str = obj.get(buf);
			// make sure, there are always 4, 8, 12, ... images (for all directions)
			if (str.empty() && i % 4 == 0) {
				sprintf(buf, "frontimage[%i]", i);
				str = obj.get(buf);
				if(str.empty()) {
					break;
				}
				sprintf(buf, "image[%i]", i);
				str = obj.get(buf);
			}
			keys.append(str);

			if(  image_type==roadsign_desc_t::USE_FRONTIMAGE  ) {
				sprintf(buf, "frontimage[%i]", i);
				str = obj.get(buf);
				front_keys.append(str);
			}
		}
	}
	imagelist_writer_t::instance()->write_obj(fp, node, keys);
	if(  image_type>=roadsign_desc_t::USE_FRONTIMAGE  ) {
		imagelist_writer_t::instance()->write_obj(fp, node, front_keys);
	}

	// probably add some icons, if defined
	slist_tpl<string> cursorkeys;

	string c = string(obj.get("cursor")), i=string(obj.get("icon"));
	cursorkeys.append(c);
	cursorkeys.append(i);
	if (!c.empty() || !i.empty()) {
		cursorskin_writer_t::instance()->write_obj(fp, node, obj, cursorkeys);
	}

	node.write(fp);
}
