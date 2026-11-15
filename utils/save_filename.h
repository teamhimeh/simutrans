/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef UTILS_SAVE_FILENAME_H
#define UTILS_SAVE_FILENAME_H

#include <string>

/**
 * Validates the name of a savegame that is chosen by a script.
 *
 * @p name is a UTF-8 file name without directory and without extension.
 * Only names that are safe on Windows, macOS and Linux are accepted; nothing is ever rewritten.
 * Rejected: empty or too long names, invalid UTF-8, control and invisible formatting characters,
 * path separators, characters forbidden on Windows (: * ? " < > |), names starting with '.',
 * names starting or ending with a space, names ending with '.', and Windows device names.
 *
 * @param error receives a short English reason if the name is rejected
 * @return true if @p name may be used
 */
bool is_valid_script_save_name(const std::string &name, std::string &error);

/// Longest accepted name in bytes (the ".sve" extension is added on top of this).
enum { SCRIPT_SAVE_NAME_MAX_BYTES = 128 };

#endif
