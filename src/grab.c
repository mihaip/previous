/*
  Previous - grab.c

  This file is distributed under the GNU General Public License, version 2
  or at your option any later version. Read the file gpl.txt for details.

  Grab screen and save it as PNG file.
*/
const char Grab_fileid[] = "Previous grab.c";

#include "main.h"
#include "configuration.h"
#include "log.h"
#include "dimension.hpp"
#include "paths.h"
#include "statusbar.h"
#include "m68000.h"
#include "grab.h"


#if HAVE_LIBPNG
#include "file.h"
#include <png.h>

#define NEXT_SCREEN_HEIGHT 832
#define NEXT_SCREEN_WIDTH  1120


/* Convert framebuffer data to RGBA and fill buffer */
static bool Grab_FillBuffer(uint8_t* buf) {
	uint8_t* fb;
	int i, j;
	
	if (ConfigureParams.Screen.nMonitorType==MONITOR_TYPE_DIMENSION) {
		fb = (uint8_t*)(nd_vram_for_slot(ND_SLOT(ConfigureParams.Screen.nMonitorNum)));
		if (fb) {
#if ND_STEP
			j = 0;
#else
			j = 16*4;
#endif
			for (i = 0; i < (NEXT_SCREEN_WIDTH*NEXT_SCREEN_HEIGHT*4); i+=4) {		
				if (i && (i%(NEXT_SCREEN_WIDTH*4))==0)
					j+=32*4;

				buf[i+0] = fb[i+j+2]; // r
				buf[i+1] = fb[i+j+1]; // g
				buf[i+2] = fb[i+j+0]; // b
				buf[i+3] = 0xff;      // a
			}
			return true;
		}
	} else {
		if (NEXTVideo) {
			fb = NEXTVideo;
			j = 0;
			if (ConfigureParams.System.bColor) {
				for (i = 0; i < (NEXT_SCREEN_WIDTH*NEXT_SCREEN_HEIGHT*4); i+=4) {
					if (!ConfigureParams.System.bTurbo && i && (i%(NEXT_SCREEN_WIDTH*4))==0)
						j+=32*2;
					
					buf[i+0] = (fb[(i>>1)+j+0]&0xF0) | ((fb[(i>>1)+j+0]&0xF0)>>4); // r
					buf[i+1] = (fb[(i>>1)+j+0]&0x0F) | ((fb[(i>>1)+j+0]&0x0F)<<4); // g
					buf[i+2] = (fb[(i>>1)+j+1]&0xF0) | ((fb[(i>>1)+j+1]&0xF0)>>4); // b
					buf[i+3] = 0xff;                                               // a
				}
			} else {
				for (i = 0; i < (NEXT_SCREEN_WIDTH*NEXT_SCREEN_HEIGHT/4); i++) {
					if (!ConfigureParams.System.bTurbo && i && (i%(NEXT_SCREEN_WIDTH/4))==0)
						j+=32/4;
					
					buf[i*16+ 0] = buf[i*16+ 1] = buf[i*16+ 2] = (~(fb[i+j] >> 6) & 3) * 0x55; // rgb
					buf[i*16+ 4] = buf[i*16+ 5] = buf[i*16+ 6] = (~(fb[i+j] >> 4) & 3) * 0x55; // rgb
					buf[i*16+ 8] = buf[i*16+ 9] = buf[i*16+10] = (~(fb[i+j] >> 2) & 3) * 0x55; // rgb
					buf[i*16+12] = buf[i*16+13] = buf[i*16+14] = (~(fb[i+j] >> 0) & 3) * 0x55; // rgb
					buf[i*16+ 3] = 0xff; // a
					buf[i*16+ 7] = 0xff; // a
					buf[i*16+11] = 0xff; // a
					buf[i*16+15] = 0xff; // a
				}
			}
			return true;
		}
	}
	return false;
}

/* Create PNG file */
static bool Grab_MakePNG(FILE* fp) {
	png_structp png_ptr  = NULL;
	png_infop   info_ptr = NULL;
	png_text    pngtext;
	
	char        key[]    = "Title";
	char        text[]   = "Previous Screen Grab";

	int         y        = 0;
	bool        ret      = false;
	
	uint8_t*    src_ptr  = NULL;
	uint8_t*    buf      = malloc(NEXT_SCREEN_WIDTH*NEXT_SCREEN_HEIGHT*4);
	
	if (!buf) {
		return false;
	}	
	if (!Grab_FillBuffer(buf)) {
		free(buf);
		return false;
	}

	/* Create and initialize the png_struct with error handler functions. */
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) {
		goto png_cleanup;
	}
	
	/* Allocate/initialize the image information data. */
	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		goto png_cleanup;
	}
	
	/* libpng ugliness: Set error handling when not supplying own
	 * error handling functions in the png_create_write_struct() call.
	 */
	if (setjmp(png_jmpbuf(png_ptr))) {
		goto png_cleanup;
	}
	
	/* store current pos in fp (could be != 0 for avi recording) */
	off_t start = ftello(fp);
	
	/* initialize the png structure */
	png_init_io(png_ptr, fp);
	
	/* image data properties */
	png_set_IHDR(png_ptr, info_ptr, NEXT_SCREEN_WIDTH, NEXT_SCREEN_HEIGHT, 8, PNG_COLOR_TYPE_RGB_ALPHA,
				 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
				 PNG_FILTER_TYPE_DEFAULT);
	
	/* image info */
	pngtext.key = key;
	pngtext.text = text;
	pngtext.compression = PNG_TEXT_COMPRESSION_NONE;
#ifdef PNG_iTXt_SUPPORTED
	pngtext.lang = NULL;
#endif
	png_set_text(png_ptr, info_ptr, &pngtext, 1);
	
	/* write the file header information */
	png_write_info(png_ptr, info_ptr);
		
	for (y = 0; y < NEXT_SCREEN_HEIGHT; y++)
	{		
		src_ptr = buf + y * NEXT_SCREEN_WIDTH * 4;
		
		png_write_row(png_ptr, src_ptr);
	}
	
	/* write the additional chunks to the PNG file */
	png_write_end(png_ptr, info_ptr);
	
	ret = true;
	
png_cleanup:
	if (png_ptr) {
		/* handles info_ptr being NULL */
		png_destroy_write_struct(&png_ptr, &info_ptr);
	}
	if (buf) {
		free(buf);
	}
	
	return ret;
}

/* Open file and save PNG data to it */
static void Grab_SaveFile(char* szPathName) {
	FILE *fp = NULL;

	fp = File_Open(szPathName, "wb");
	if (!fp) {
		Log_Printf(LOG_WARN, "[Grab] Error: Could not open file %s", szPathName);
		return;
	}
	
	if (Grab_MakePNG(fp)) {
		Statusbar_AddMessage("Saving screen grab", 0);
	} else {
		Log_Printf(LOG_WARN, "[Grab] Error: Could not create PNG file");
	}
	
	File_Close(fp);
}

/* Grab screen */
void Grab_Screen(void) {
	int i;
	static char *szPathName = NULL;
	char *szFileName = malloc(FILENAME_MAX);
	
	if (!szFileName)  return;
	
	if (File_DirExists(ConfigureParams.Printer.szPrintToFileName)) {
		for (i = 0; i < 1000; i++) {
			snprintf(szFileName, FILENAME_MAX, "%03d_next_screen", i);
			szPathName = File_MakePath(ConfigureParams.Printer.szPrintToFileName, szFileName, ".png");
			
			if (File_Exists(szPathName)) {
				continue;
			}
			
			Grab_SaveFile(szPathName);
			break;
		}
		
		if (i >= 1000) {
			Log_Printf(LOG_WARN, "[Grab] Error: Maximum screen grab count exceeded (%d)", i);
		}
	}

	free(szFileName);
}
#else
void Grab_Screen(void) {
	Log_Printf(LOG_WARN, "[Grab] Screen grab not supported (libpng missing)");
}
#endif
