/*
  Hatari - str.h

  This file is distributed under the GNU General Public License, version 2
  or at your option any later version. Read the file gpl.txt for details.
*/

#ifndef HATARI_STR_H
#define HATARI_STR_H

#include "config.h"
#include <string.h>
#if HAVE_STRINGS_H
# include <strings.h>
#endif

#if !HAVE_LIBC_STRLCPY
#define strlcpy(dst, src, len) SDL_strlcpy(dst, src, len)
#endif

#define Str_Free(s) { free(s); s = NULL; }

extern char *Str_Trim(char *buffer);
extern char *Str_ToUpper(char *pString);
extern char *Str_ToLower(char *pString);
extern char *Str_Alloc(int len);
extern char *Str_Dup(const char *str);

#endif  /* HATARI_STR_H */
