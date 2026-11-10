/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef DATAOBJ_HEIGHT_MAP_EXPORTER_H
#define DATAOBJ_HEIGHT_MAP_EXPORTER_H

#include "../simtypes.h"

class karte_t;

/** Exports terrain grid heights for the linear heightmap loader (2026-10-23). */
class height_map_exporter_t
{
public:
	struct report_t
	{
		sint8 minimum_height;
		sint8 maximum_height;
		sint8 groundwater;
		/// Number of exported vertices whose decoded height differs.
		uint32 changed_heights;
		/// Outer vertices differing from the adjacent vertex duplicated on import.
		uint32 changed_border_heights;
		/// Maximum absolute encoding error, excluding the omitted outer border.
		sint16 maximum_error;

		report_t();
	};

	/** Inspect quantization and outer-border loss without changing the world.
	 * Counts describe linear decoding plus the loader's +1, before slope correction.
	 */
	static report_t analyze(const karte_t &world);

	/** Write one gray8 pixel per tile, using the grid vertex at its upper left.
	 * Uses a single row of image memory and a sibling temporary file. Existing
	 * destination files are refused; false reports invalid settings or I/O failure.
	 * Call from the game thread so terrain cannot change during export.
	 */
	static bool write_png(const karte_t &world, const char *filename, report_t &report);

	/** Find the closest linear pixel for an actual world height (including +1).
	 * Ties use the smallest pixel. Invalid height limits return zero.
	 */
	static uint8 encode_height(sint16 height, sint8 minimum_height, sint8 maximum_height);
};

#endif
