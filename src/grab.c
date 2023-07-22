/*
  Previous - grab.c

  This file is distributed under the GNU General Public License, version 2
  or at your option any later version. Read the file gpl.txt for details.

  Grab screen or sound and save it to a PNG or AIFF file.
*/
const char Grab_fileid[] = "Previous grab.c";

#include "main.h"
#include "configuration.h"
#include "log.h"
#include "dimension.hpp"
#include "file.h"
#include "paths.h"
#include "statusbar.h"
#include "m68000.h"
#include "host.h"
#include "grab.h"


#if HAVE_LIBPNG
#include <png.h>

#define NEXT_SCREEN_HEIGHT 832
#define NEXT_SCREEN_WIDTH  1120

#if ND_STEP
#define ND_OFFSET 0
#else
#define ND_OFFSET (4*4)
#endif

/**
 * Convert framebuffer data to RGBA and fill buffer.
 */
static bool Grab_FillBuffer(uint8_t* buf) {
	uint8_t* fb;
	int i, j;
	
	if (ConfigureParams.Screen.nMonitorType==MONITOR_TYPE_DIMENSION) {
		fb = (uint8_t*)(nd_vram_for_slot(ND_SLOT(ConfigureParams.Screen.nMonitorNum)));
		if (fb) {
			for (i = 0, j = ND_OFFSET; i < (NEXT_SCREEN_WIDTH*NEXT_SCREEN_HEIGHT*4); i+=4, j+=4) {		
				if (i && (i%(NEXT_SCREEN_WIDTH*4))==0)
					j+=32*4;

				buf[i+0] = fb[j+2]; // r
				buf[i+1] = fb[j+1]; // g
				buf[i+2] = fb[j+0]; // b
				buf[i+3] = 0xff;    // a
			}
			return true;
		}
	} else {
		if (NEXTVideo) {
			fb = NEXTVideo;
			if (ConfigureParams.System.bColor) {
				for (i = 0, j = 0; i < (NEXT_SCREEN_WIDTH*NEXT_SCREEN_HEIGHT*4); i+=4, j+=2) {
					if (!ConfigureParams.System.bTurbo && i && (i%(NEXT_SCREEN_WIDTH*4))==0)
						j+=32*2;
					
					buf[i+0] = (fb[j+0]&0xf0) | ((fb[j+0]&0xf0)>>4); // r
					buf[i+1] = (fb[j+0]&0x0f) | ((fb[j+0]&0x0f)<<4); // g
					buf[i+2] = (fb[j+1]&0xf0) | ((fb[j+1]&0xf0)>>4); // b
					buf[i+3] = 0xff;                                 // a
				}
			} else {
				for (i = 0, j = 0; i < (NEXT_SCREEN_WIDTH*NEXT_SCREEN_HEIGHT*4); i+=16, j++) {
					if (!ConfigureParams.System.bTurbo && i && (i%(NEXT_SCREEN_WIDTH*4))==0)
						j+=32/4;
					
					buf[i+ 0] = buf[i+ 1] = buf[i+ 2] = (~(fb[j] >> 6) & 3) * 0x55; buf[i+ 3] = 0xff; // rgba
					buf[i+ 4] = buf[i+ 5] = buf[i+ 6] = (~(fb[j] >> 4) & 3) * 0x55; buf[i+ 7] = 0xff; // rgba
					buf[i+ 8] = buf[i+ 9] = buf[i+10] = (~(fb[j] >> 2) & 3) * 0x55; buf[i+11] = 0xff; // rgba
					buf[i+12] = buf[i+13] = buf[i+14] = (~(fb[j] >> 0) & 3) * 0x55; buf[i+15] = 0xff; // rgba
				}
			}
			return true;
		}
	}
	return false;
}

/**
 * Create PNG file.
 */
static bool Grab_MakePNG(FILE* fp) {
	png_structp png_ptr  = NULL;
	png_infop   info_ptr = NULL;
	png_text    pngtext;
	
	char        key[]    = "Title";
	char        text[]   = "Previous Screen Grab";

	int         y        = 0;
	bool        result   = false;
	
	off_t       start    = 0;
	uint8_t*    src_ptr  = NULL;
	uint8_t*    buf      = malloc(NEXT_SCREEN_WIDTH*NEXT_SCREEN_HEIGHT*4);
	
	if (buf) {
		if (Grab_FillBuffer(buf)) {
			/* Create and initialize the png_struct with error handler functions. */
			png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
			if (png_ptr) {
				/* Allocate/initialize the image information data. */
				info_ptr = png_create_info_struct(png_ptr);
				if (info_ptr) {
					/* libpng ugliness: Set error handling when not supplying own
					 * error handling functions in the png_create_write_struct() call.
					 */
					if (!setjmp(png_jmpbuf(png_ptr))) {
						/* store current pos in fp (could be != 0 for avi recording) */
						start = ftello(fp);
						
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
						
						result = true;
					}
				}
				/* handles info_ptr being NULL */
				png_destroy_write_struct(&png_ptr, &info_ptr);
			}
		}
		free(buf);
	}	
	return result;
}

/**
 * Open file and save PNG data to it.
 */
static void Grab_SaveFile(char* szPathName) {
	FILE *fp = NULL;

	fp = File_Open(szPathName, "wb");
	if (!fp) {
		Log_Printf(LOG_WARN, "[Grab] Error: Could not open file %s", szPathName);
		return;
	}
	
	if (Grab_MakePNG(fp)) {
		Statusbar_AddMessage("Saving screen to file", 0);
	} else {
		Log_Printf(LOG_WARN, "[Grab] Error: Could not create PNG file");
	}
	
	File_Close(fp);
}

/**
 * Grab screen.
 */
void Grab_Screen(void) {
	int i;
	char szFileName[32];
	char *szPathName = NULL;
	
	if (!szFileName) return;
	
	if (File_DirExists(ConfigureParams.Printer.szPrintToFileName)) {
		for (i = 0; i < 1000; i++) {
			snprintf(szFileName, sizeof(szFileName), "next_screen_%03d", i);
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
	if (szPathName) {
		free(szPathName);
	}
}
#else // !HAVE_LIBPNG
void Grab_Screen(void) {
	Log_Printf(LOG_WARN, "[Grab] Screen grab not supported (libpng missing)");
}
#endif // HAVE_LIBPNG


/*
 AIFF file output
 
 We simply save out the AIFF format headers and then write the sample data. 
 When we stop recording we complete the size information in the headers and 
 close up.
 
 All data is stored in big endian byte order.
 
 Format Chunk (12 bytes in length total) Byte Number
 0 - 3    "FORM" (ASCII Characters)
 4 - 7    Total Length Of Package To Follow (Binary, big endian)
 8 - 11   "AIFF" (ASCII Characters)
 
 Common Chunk (24 bytes in length total) Byte Number
 0 - 3    "COMM" (ASCII Characters)
 4 - 7    Length Of COMM Chunk (Binary, always 18)
 8 - 9    Number Of Channles (1=Mono, 2=Stereo)
 10 - 13  Number Of Samples
 14 - 15  Bits Per Sample
 16 - 25  Sample Rate (80-bit IEEE 754 floating point number, in Hz)
 
 SSND Chunk Byte Number
 0 - 3    "SSND" (ASCII Characters)
 4 - 7    Length Of Data To Follow
 8 - 11   Offset
 12 - 15  Block Size
 16 - end Data (Samples)
 */

static lock_t GrabSoundLock;               /* Protect AIFF file and variables */

static FILE*  AiffFileHndl;                /* Pointer to our AIFF file */
static int    nAiffOutputBytes;            /* Number of sample bytes saved */
volatile bool bRecordingAiff = false;      /* Is an AIFF file open and recording? */

static uint8_t AiffHeader[54] =
{
	/* Format chunk */
	'F', 'O', 'R', 'M',      /* "FORM" (ASCII Characters) */
	0, 0, 0, 0,              /* Total Length Of Package To Follow (patched when file is closed) */
	'A', 'I', 'F', 'F',      /* "AIFF" (ASCII Characters) */
	/* Common chunk */
	'C', 'O', 'M', 'M',      /* "COMM" (ASCII Characters) */
	0, 0, 0, 18,             /* Length Of COMM Chunk (18) */
	0, 2,                    /* Number of channels (2 for stereo) */
	0, 0, 0, 0,              /* Number of samples (patched when file is closed) */
	0, 16,                   /* Bits per sample (16 bit) */
	0x40, 0x0e, 0xac, 0x44,  /* Sample rate (44,1 kHz) */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00,
	/* Sound data chunk */
	'S', 'S', 'N', 'D',      /* "SSND" (ASCII Characters) */
	0, 0, 0, 0,              /* Length of SSND Chunk (patched when file is closed) */
	0, 0, 0, 0,              /* Offset */
	0, 0, 0, 0               /* Block size */
};


/**
 * Write sizes to AIFF header, then close the AIFF file.
 */
static void Grab_CloseSoundFile(void)
{
	uint32_t nAiffFileBytes;
	uint32_t nAiffDataBytes;
	uint32_t nAiffSamples;
	
	if (bRecordingAiff)
	{
		bRecordingAiff = false;
		
		/* Update headers with sizes */
		nAiffFileBytes = 46+nAiffOutputBytes; /* length of headers minus 8 bytes plus length of data */
		nAiffDataBytes = 8+nAiffOutputBytes;  /* length of data plus 8 bytes */
		nAiffSamples   = nAiffOutputBytes/2;  /* length of data divided by bytes per sample */
		
		/* Patch length of file in header structure */
		AiffHeader[4] = (uint8_t)((nAiffFileBytes >> 24) & 0xff);
		AiffHeader[5] = (uint8_t)((nAiffFileBytes >> 16) & 0xff);
		AiffHeader[6] = (uint8_t)((nAiffFileBytes >>  8) & 0xff);
		AiffHeader[7] = (uint8_t)((nAiffFileBytes >>  0) & 0xff);
		
		/* Patch number of samples in header structure */
		AiffHeader[22] = (uint8_t)((nAiffSamples >> 24) & 0xff);
		AiffHeader[23] = (uint8_t)((nAiffSamples >> 16) & 0xff);
		AiffHeader[24] = (uint8_t)((nAiffSamples >>  8) & 0xff);
		AiffHeader[25] = (uint8_t)((nAiffSamples >>  0) & 0xff);

		/* Patch length of data in header structure */
		AiffHeader[42] = (uint8_t)((nAiffDataBytes >> 24) & 0xff);
		AiffHeader[43] = (uint8_t)((nAiffDataBytes >> 16) & 0xff);
		AiffHeader[44] = (uint8_t)((nAiffDataBytes >>  8) & 0xff);
		AiffHeader[45] = (uint8_t)((nAiffDataBytes >>  0) & 0xff);
		
		/* Write updated header to file */
		if (!File_Write(AiffHeader, sizeof(AiffHeader), 0, AiffFileHndl))
		{
			perror("[Grab] Grab_CloseSoundFile:");
		}
		
		/* Close file */
		AiffFileHndl = File_Close(AiffFileHndl);
		
		/* And inform user */
		Log_Printf(LOG_WARN, "[Grab] Stopping sound record");
		Statusbar_AddMessage("Stop saving sound to file", 0);
	}
}

/**
 * Open AIFF output file and write header.
 */
static void Grab_OpenSoundFile(void)
{
	int i;
	char szFileName[32];
	char *szPathName = NULL;
	
	if (bRecordingAiff) {
		Grab_CloseSoundFile();
	}
	
	nAiffOutputBytes = 0;
	
	if (File_DirExists(ConfigureParams.Printer.szPrintToFileName)) {
		
		/* Build file name */
		for (i = 0; i < 1000; i++) {
			snprintf(szFileName, sizeof(szFileName), "next_sound_%03d", i);
			szPathName = File_MakePath(ConfigureParams.Printer.szPrintToFileName, szFileName, ".aiff");
			
			if (File_Exists(szPathName)) {
				continue;
			}
			break;
		}
		
		if (i >= 1000) {
			Log_Printf(LOG_WARN, "[Grab] Error: Maximum sound grab count exceeded (%d)", i);
			goto done;
		}
		
		/* Create our file */
		AiffFileHndl = File_Open(szPathName, "wb");
		if (!AiffFileHndl)
		{
			Log_Printf(LOG_WARN, "[Grab] Failed to create sound file %s: ", szPathName);
			goto done;
		}
		
		/* Write header to file */
		if (File_Write(AiffHeader, sizeof(AiffHeader), 0, AiffFileHndl))
		{
			bRecordingAiff = true;
			Log_Printf(LOG_WARN, "[Grab] Starting sound record");
			Statusbar_AddMessage("Start saving sound to file", 0);
		}
		else
		{
			perror("[Grab] Grab_OpenSoundFile:");
		}
		
	done:
		if (szPathName) {
			free(szPathName);
		}
	}
}

/**
 * Update AIFF file with current samples.
 */
void Grab_Sound(uint8_t* samples, int len)
{
	host_lock(&GrabSoundLock);
	if (bRecordingAiff)
	{
		/* Append samples to our AIFF file */
		if (fwrite(samples, len, 1, AiffFileHndl) != 1)
		{
			perror("[Grab] Grab_Sound:");
			Grab_CloseSoundFile();
		}

		/* Add samples to AIFF file length counter */
		nAiffOutputBytes += len;
	}
	host_unlock(&GrabSoundLock);
}

/**
 * Start/Stop recording sound.
 */
void Grab_SoundToggle(void) {
	host_lock(&GrabSoundLock);
	if (bRecordingAiff) {
		Grab_CloseSoundFile();
	} else {
		Grab_OpenSoundFile();
	}
	host_unlock(&GrabSoundLock);
}

/**
 * Stop any recording activities.
 */
void Grab_Stop(void) {
	host_lock(&GrabSoundLock);
	Grab_CloseSoundFile();
	host_unlock(&GrabSoundLock);
}
