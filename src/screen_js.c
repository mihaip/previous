#include "configuration.h"
#include "host.h"
#include "memory.h"
#include "screen.h"
#include "video.h"
#include <emscripten.h>
#include <arpa/inet.h>

// Use xxHash in inline mode to avoid having to include a separate .c file.
#define XXH_STATIC_LINKING_ONLY
#define XXH_INLINE_ALL
#include "xxHash/xxhash.h"

// TODO: remove the need for these SDL stubs
SDL_Surface*  sdlscrn = NULL;        /* The SDL screen surface */

/* extern for shortcuts */
volatile bool bGrabMouse    = false; /* Grab the mouse cursor in the window */
volatile bool bInFullScreen = false; /* true if in full screen */

static const int NeXT_SCRN_WIDTH  = 1120;
static const int NeXT_SCRN_HEIGHT = 832;
static uint8_t* frame_buffer;
static int frame_buffer_width;
static int frame_buffer_height;
static int frame_buffer_size;
static XXH64_hash_t last_update_hash;

static uint32_t BWCOLORS[] = {
    0xFFFFFFFF,
    0xFFAAAAAA,
    0xFF555555,
    0XFF000000,
};
static uint32_t BW2RGB[0x400];
static uint32_t COL2RGB[0x10000];

void Screen_Init(void) {
    frame_buffer_width  = NeXT_SCRN_WIDTH;
    frame_buffer_height = NeXT_SCRN_HEIGHT;
    frame_buffer_size = frame_buffer_height * frame_buffer_width * 4;
    frame_buffer = malloc(frame_buffer_size);
    last_update_hash = 0;

    /* initialize BW lookup table */
    for(int i = 0; i < 0x100; i++) {
        // 2 bits per pixel to represent 4 possible grays.
        BW2RGB[i*4+0] = BWCOLORS[(i >> 6) & 3];
        BW2RGB[i*4+1] = BWCOLORS[(i >> 4) & 3];
        BW2RGB[i*4+2] = BWCOLORS[(i >> 2) & 3];
        BW2RGB[i*4+3] = BWCOLORS[(i >> 0) & 3];
    }
    /* initialize color lookup table */
    for(int pixel16 = 0; pixel16 < 0x10000; pixel16++) {
        // 16-bit values are represented as RGBx (4 bits per channel), but due
        // to the big-endian nature of the 68K while WebAssembly is little-
        // endian, they're read as BxRG.
        int r = pixel16 & 0x00F0; r >>= 4; r |= r << 4;
        int g = pixel16 & 0x000F; g >>= 0;  g |= g << 4;
        int b = pixel16 & 0xF000; b >>= 12;  b |= b << 4;
        uint32_t pixel32 = r | (g << 8) | (b << 16) | 0xFF000000;
        COL2RGB[pixel16] = pixel32;
    }

    EM_ASM_({ workerApi.didOpenVideo($0, $1); }, frame_buffer_width, frame_buffer_height);

    // TOOD: look at ConfigureParams.Screen.nMonitorType
}

void Screen_UnInit(void) {
    free(frame_buffer);
}

void Screen_StatusbarChanged(void) {
    printf("Screen_StatusbarChanged\n");
}

void Screen_UpdateRects(SDL_Surface *screen, int numrects, SDL_Rect *rects) {
    printf("Screen_UpdateRects\n");
}

void Screen_UpdateRect(SDL_Surface *screen, int32_t x, int32_t y, int32_t w, int32_t h) {
    printf("Screen_UpdateRect\n");
}

void Screen_ModeChanged(void) {
    // TOOD: look at ConfigureParams.Screen.nMonitorType
    printf("Screen_ModeChanged\n");
}

void Screen_EnterFullScreen(void) {
    printf("Screen_EnterFullScreen\n");
}

void Screen_ReturnFromFullScreen(void) {
    printf("Screen_ReturnFromFullScreen\n");
}

void Screen_ShowMainWindow(void) {
    printf("Screen_ShowMainWindow\n");
}

void Screen_Pause(bool pause) {
    printf("Screen_Pause\n");
}

void Screen_SizeChanged(void) {
    printf("Screen_SizeChanged\n");
}

void Screen_Repaint(void) {
    if (!Video_Enabled()) {
        EM_ASM({ workerApi.blit(0, 0); });
        return;
    }

    int pitch = NeXT_SCRN_WIDTH + (ConfigureParams.System.bTurbo ? 0 : 32);
    int screenByteSize;
    if (ConfigureParams.System.bColor) {
        screenByteSize = NeXT_SCRN_HEIGHT * pitch * sizeof(uint16_t);
    } else {
        pitch /= 4;
        screenByteSize = NeXT_SCRN_HEIGHT * pitch * sizeof(uint8_t);
    }
    XXH64_hash_t update_hash = XXH3_64bits(NEXTVideo, screenByteSize);
    if (update_hash == last_update_hash) {
        // Screen has not changed, but we still let the JS know so that it can
        // keep track of screen refreshes when deciding how long to idle for.
        EM_ASM({ workerApi.blit(0, 0); });
        return;
    }
    last_update_hash = update_hash;

    uint32_t* dst = (uint32_t*)frame_buffer;
    if (ConfigureParams.System.bColor) {
        for(int y = 0; y < NeXT_SCRN_HEIGHT; y++) {
            uint16_t* src = (uint16_t*)NEXTVideo + (y*pitch);
            for(int x = 0; x < NeXT_SCRN_WIDTH; x++) {
                *dst++ = COL2RGB[*src++];
            }
        }
    } else {
        for(int y = 0; y < NeXT_SCRN_HEIGHT; y++) {
            int src = y * pitch;
            for(int x = 0; x < NeXT_SCRN_WIDTH/4; x++, src++) {
                int idx = NEXTVideo[src] * 4;
                *dst++  = BW2RGB[idx+0];
                *dst++  = BW2RGB[idx+1];
                *dst++  = BW2RGB[idx+2];
                *dst++  = BW2RGB[idx+3];
            }
        }
    }

  EM_ASM_({ workerApi.blit($0, $1); }, frame_buffer, frame_buffer_size);
}
