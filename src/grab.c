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
#include "file.h"
#include "paths.h"
#include "statusbar.h"
#include "m68000.h"
#include "grab.h"


#if HAVE_LIBPNG
#include <png.h>

#define NEXT_SCREEN_HEIGHT 832
#define NEXT_SCREEN_WIDTH  1120

/**
 * Convert framebuffer data to RGBA and fill buffer.
 */
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
			for (i = 0; i < (NEXT_SCREEN_WIDTH*NEXT_SCREEN_HEIGHT*4); i+=4, j+=4) {		
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
			j = 0;
			if (ConfigureParams.System.bColor) {
				for (i = 0; i < (NEXT_SCREEN_WIDTH*NEXT_SCREEN_HEIGHT*4); i+=4, j+=2) {
					if (!ConfigureParams.System.bTurbo && i && (i%(NEXT_SCREEN_WIDTH*4))==0)
						j+=32*2;
					
					buf[i+0] = (fb[j+0]&0xF0) | ((fb[j+0]&0xF0)>>4); // r
					buf[i+1] = (fb[j+0]&0x0F) | ((fb[j+0]&0x0F)<<4); // g
					buf[i+2] = (fb[j+1]&0xF0) | ((fb[j+1]&0xF0)>>4); // b
					buf[i+3] = 0xff;                                 // a
				}
			} else {
				for (i = 0; i < (NEXT_SCREEN_WIDTH*NEXT_SCREEN_HEIGHT*4); i+=16, j++) {
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
 WAV file output
 
 We simply save out the WAVE format headers and then write the sample data. 
 When we stop recording we complete the size information in the headers and 
 close up.
 
 All data is stored in little endian byte order.
 
 RIFF Chunk (12 bytes in length total) Byte Number
 0 - 3    "RIFF" (ASCII Characters)
 4 - 7    Total Length Of Package To Follow (Binary, little endian)
 8 - 11   "WAVE" (ASCII Characters)
 
 FORMAT Chunk (24 bytes in length total) Byte Number
 0 - 3    "fmt_" (ASCII Characters)
 4 - 7    Length Of FORMAT Chunk (Binary, always 0x10)
 8 - 9    Always 0x01
 10 - 11  Channel Numbers (Always 0x01=Mono, 0x02=Stereo)
 12 - 15  Sample Rate (Binary, in Hz)
 16 - 19  Bytes Per Second
 20 - 21  Bytes Per Sample: 1=8 bit Mono, 2=8 bit Stereo or 16 bit Mono, 4=16 bit Stereo
 22 - 23  Bits Per Sample
 
 DATA Chunk Byte Number
 0 - 3    "data" (ASCII Characters)
 4 - 7    Length Of Data To Follow
 8 - end  Data (Samples)
 */

static FILE *WavFileHndl;
static int  nWavOutputBytes;            /* Number of sample bytes saved */
static bool bRecordingWav = false;      /* Is a WAV file open and recording? */

static uint8_t WavHeader[44] =
{
	/* RIFF chunk */
	'R', 'I', 'F', 'F',      /* "RIFF" (ASCII Characters) */
	0, 0, 0, 0,              /* Total Length Of Package To Follow (patched when file is closed) */
	'W', 'A', 'V', 'E',      /* "WAVE" (ASCII Characters) */
	/* Format chunk */
	'f', 'm', 't', ' ',      /* "fmt_" (ASCII Characters) */
	0x10, 0, 0, 0,           /* Length Of FORMAT Chunk (always 0x10) */
	0x01, 0,                 /* Always 0x01 */
	0x02, 0,                 /* Number of channels (2 for stereo) */
	0x44, 0xAC, 0x00, 0x00,  /* Sample rate (44,1 kHz) */
	0x10, 0xB1, 0x02, 0x00,  /* Bytes per second (4 times sample rate for 16-bit stereo) */
	0x04, 0,                 /* Bytes per sample (4 = 16 bit stereo) */
	0x10, 0,                 /* Bits per sample (16 bit) */
	/* Data chunk */
	'd', 'a', 't', 'a',
	0, 0, 0, 0,              /* Length of data to follow (will be patched when file is closed) */
};


/**
 * Open WAV output file and write header.
 */
static void Grab_OpenSoundFile(void)
{
	int i;
	char szFileName[32];
	char *szPathName = NULL;
	
	if (!szFileName) return;

	uint32_t nSampleFreq, nBytesPerSec;
	
	bRecordingWav   = false;
	nWavOutputBytes = 0;
	
	nSampleFreq     = 44100;            /* Set frequency (44,1 kHz) */
	nBytesPerSec    = nSampleFreq * 4;  /* Multiply by 4 for 16 bit stereo */

	/* Build file name */
	if (File_DirExists(ConfigureParams.Printer.szPrintToFileName)) {
		for (i = 0; i < 1000; i++) {
			snprintf(szFileName, sizeof(szFileName), "next_sound_%03d", i);
			szPathName = File_MakePath(ConfigureParams.Printer.szPrintToFileName, szFileName, ".wav");
			
			if (File_Exists(szPathName)) {
				continue;
			}
			break;
		}
		
		if (i >= 1000) {
			Log_Printf(LOG_WARN, "[Grab] Error: Maximum sound grab count exceeded (%d)", i);
			goto done;
		}
	}
	
	/* Create our file */
	WavFileHndl = File_Open(szPathName, "wb");
	if (!WavFileHndl)
	{
		Log_Printf(LOG_WARN, "[Grab] Failed to create sound file %s: ", szPathName);
		goto done;
	}
		
	/* Write header to file */
	if (File_Write(WavHeader, sizeof(WavHeader), 0, WavFileHndl))
	{
		bRecordingWav = true;
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

/**
 * Write sizes to WAV header, then close the WAV file.
 */
static void Grab_CloseSoundFile(void)
{
	if (bRecordingWav)
	{
		uint32_t nWavFileBytes;
		
		bRecordingWav = false;
		
		/* Update headers with sizes */
		nWavFileBytes = 36+nWavOutputBytes; /* length of headers minus 8 bytes plus length of data */
		
		/* Patch length of file in header structure */
		WavHeader[4] = (uint8_t)(nWavFileBytes >> 0);
		WavHeader[5] = (uint8_t)(nWavFileBytes >> 8);
		WavHeader[6] = (uint8_t)(nWavFileBytes >> 16);
		WavHeader[7] = (uint8_t)(nWavFileBytes >> 24);
		
		/* Patch length of data in header structure */
		WavHeader[40] = (uint8_t)(nWavOutputBytes >> 0);
		WavHeader[41] = (uint8_t)(nWavOutputBytes >> 8);
		WavHeader[42] = (uint8_t)(nWavOutputBytes >> 16);
		WavHeader[43] = (uint8_t)(nWavOutputBytes >> 24);
		
		/* Write updated header to file */
		if (!File_Write(WavHeader, sizeof(WavHeader), 0, WavFileHndl))
		{
			perror("[Grab] Grab_CloseSoundFile:");
		}
		
		/* Close file */
		WavFileHndl = File_Close(WavFileHndl);
		
		/* And inform user */
		Log_Printf(LOG_WARN, "[Grab] Stopping sound record");
		Statusbar_AddMessage("Stop saving sound to file", 0);
	}
}

/**
 * Update WAV file with current samples.
 */
void Grab_Sound(uint8_t* samples, int len)
{
	int i;
	uint8_t* wav_samples;

	if (bRecordingWav)
	{
		len &= ~1; /* Just to be sure */
		
		wav_samples = malloc(len);
		if (!wav_samples) return;

		/* Convert samples to little endian */
		for (i = 0; i < len; i+=2)
		{
			wav_samples[i+0] = samples[i+1];
			wav_samples[i+1] = samples[i+0];
		}

		/* And append them to our wav file */
		if (fwrite(wav_samples, len, 1, WavFileHndl) != 1)
		{
			perror("[Grab] Grab_Sound:");
			Grab_CloseSoundFile();
		}

		/* Add samples to wav file length counter */
		nWavOutputBytes += len;

		free(wav_samples);
	}
}

/**
 * Start/Stop recording sound.
 */
void Grab_SoundToggle(void) {
	if (bRecordingWav) {
		Grab_CloseSoundFile();
	} else {
		Grab_OpenSoundFile();
	}
}

/**
 * Stop any recording activities.
 */
void Grab_Stop(void) {
	Grab_CloseSoundFile();
}
