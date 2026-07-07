/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include <stdio.h>

#ifdef _WIN32
#include <windows.h>

static wchar_t *utf8_to_wide(char const *text)
{
	int const size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
	if(  size == 0  ) {
		return NULL;
	}

	wchar_t *wide = new wchar_t[size];
	if(  MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, size) == 0  ) {
		delete [] wide;
		return NULL;
	}
	return wide;
}
#endif

FILE *dr_fopen(const char *filename, const char *mode)
{
#ifdef _WIN32
	wchar_t *wide_filename = utf8_to_wide(filename);
	wchar_t *wide_mode = utf8_to_wide(mode);
	FILE *file = wide_filename != NULL && wide_mode != NULL ? _wfopen(wide_filename, wide_mode) : NULL;
	delete [] wide_filename;
	delete [] wide_mode;
	return file;
#else
	return fopen(filename, mode);
#endif
}
