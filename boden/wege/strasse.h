#ifndef boden_wege_strasse_h
#define boden_wege_strasse_h

#include "weg.h"

/**
 * Cars are able to drive on roads.
 *
 * @author Hj. Malthaner
 */
class strasse_t : public weg_t
{
public:
	enum { AVOID_CITYROAD = 0x01 };
	
	static const way_desc_t *default_strasse;

	strasse_t(loadsave_t *file);
	strasse_t();

	inline waytype_t get_waytype() const {return road_wt;}

	void set_gehweg(bool janein);

	virtual void rdwr(loadsave_t *file);
	
private:
	uint8 street_flags;
	
public:
	uint8 get_street_flag() const { return street_flags; }
	void set_street_flag(uint8 s) { street_flags = s; }
  bool get_avoid_cityroad() const { return street_flags&AVOID_CITYROAD; } 
	void set_avoid_cityroad(bool s) { s ? street_flags |= AVOID_CITYROAD : street_flags &= ~AVOID_CITYROAD; }
};

#endif
