#include <time.h>
#include <stdio.h>

#include "host.h"
#include "configuration.h"
#include "debugui.h"
#include "dialog.h"
#include "dimension.hpp"
#include "dsp.h"
#include "hatari-glue.h"
#include "ioMem.h"
#include "keymap.h"
#include "log.h"
#include "m68000.h"
#include "main.h"
#include "NextBus.hpp"
#include "paths.h"
#include "reset.h"
#include "screen.h"
#include "snd.h"

volatile bool bQuitProgram = false;

static bool Main_Init(void) {
    if (!Log_Init()) {
        fprintf(stderr, "Logging/tracing initialization failed\n");
        exit(-1);
    }

    Screen_Init();
    Keymap_Init();
    Main_SetTitle(NULL);

    M68000_Init();
    DSP_Init();
    IoMem_Init();
    /* Done as last, needs CPU & DSP running... */
    DebugUI_Init();

    Dialog_CheckFiles();

    return !Reset_Cold();
}

bool Main_PauseEmulation(bool visualize) {
    return false;
}

bool Main_UnPauseEmulation(void) {
    NextBus_Pause(false);
    Sound_Pause(false);
    Screen_Pause(false);
    host_pause_time(false);

    return true;
}

void Main_Halt(void) {
    printf("Main_Halt\n");
    Main_RequestQuit(false);
}

void Main_RequestQuit(bool confirm) {
    printf("Main_RequestQuit\n");
    bQuitProgram = true;
}

void Main_SendSpecialEvent(int type) {
    switch (type) {
        case MAIN_PAUSE:
            printf("MAIN_PAUSE event (ignored)\n");
            break;
        case MAIN_UNPAUSE:
            printf("MAIN_UNPAUSE event (ignored)\n");
            break;
        case MAIN_REPAINT:
            Screen_Repaint();
            break;
        case MAIN_ND_DISPLAY:
            nd_display_repaint();
            break;
        case MAIN_HALT:
            Main_Halt();
            break;
        default:
            printf("Special event %d ignored\n", type);
            break;
    }
}

void Main_SpeedReset(void) {
    printf("Main_SpeedReset\n");
}

void Main_SetTitle(const char *title) {
    printf("Main_SetTitle(\"%s\")\n", title);
}

static void Main_Loop(void) {
    Main_UnPauseEmulation();

    while (!bQuitProgram) {
        CycInt_AddRelativeInterruptUs(1000, 0, INTERRUPT_EVENT_LOOP);
        M68000_Start();
    }
}

void Main_EventHandlerInterrupt(void) {
	CycInt_AcknowledgeInterrupt();

    // TODO: get events from JS
    // TODO: Main_Speed-style speed adjustment?
	// Main_Speed(rt, vt);

	CycInt_AddRelativeInterruptUs((1000*1000)/200, 0, INTERRUPT_EVENT_LOOP); // poll events with 200 Hz
}

static void Main_UnInit(void) {
    IoMem_UnInit();
    Screen_UnInit();
    Exit680x0();

    DebugUI_UnInit();
    Log_UnInit();

    Paths_UnInit();
}

int main(int argc, char *argv[]) {
    srand(time(NULL));

    Paths_Init(argv[0]);

    Configuration_SetDefault();

    Configuration_Load("previous.cfg");

    Configuration_Apply(true);

    if (!Main_Init()) {
        fprintf(stderr, "Initialization failed\n");
        exit(-1);
    }
    Main_Loop();
    Main_UnInit();

    return 0;
}
