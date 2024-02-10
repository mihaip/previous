#include <time.h>
#include <stdio.h>
#include <emscripten.h>

#include "host.h"
#include "configuration.h"
#include "debugui.h"
#include "dialog.h"
#include "dimension.hpp"
#include "dsp.h"
#include "hatari-glue.h"
#include "ioMem.h"
#include "keymap.h"
#include "kms.h"
#include "log.h"
#include "m68000.h"
#include "main.h"
#include "NextBus.hpp"
#include "paths.h"
#include "reset.h"
#include "screen.h"
#include "scsi.h"
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

static void Main_ReadJSInput(void) {
    int lock = EM_ASM_INT_V({ return workerApi.acquireInputLock(); });
    if (!lock) {
        return;
    }

    int mouse_button_state = EM_ASM_INT_V({
        return workerApi.getInputValue(workerApi.InputBufferAddresses.mouseButtonStateAddr);
    });
    if (mouse_button_state > -1) {
        if (mouse_button_state == 0) {
            Keymap_MouseUp(true);
        } else {
            Keymap_MouseDown(true);
        }
    }
    int mouse_button2_state = EM_ASM_INT_V({
        return workerApi.getInputValue(workerApi.InputBufferAddresses.mouseButton2StateAddr);
    });
    if (mouse_button2_state > -1) {
        if (mouse_button2_state == 0) {
            Keymap_MouseUp(false);
        } else {
            Keymap_MouseDown(false);
        }
    }

    int has_mouse_position = EM_ASM_INT_V({
        return workerApi.getInputValue(workerApi.InputBufferAddresses.mousePositionFlagAddr);
    });
    if (has_mouse_position) {
        int delta_x = EM_ASM_INT_V({
            return workerApi.getInputValue(workerApi.InputBufferAddresses.mouseDeltaXAddr);
        });
        int delta_y = EM_ASM_INT_V({
            return workerApi.getInputValue(workerApi.InputBufferAddresses.mouseDeltaYAddr);
        });
        Keymap_MouseMove(delta_x, delta_y);
    }

    int has_key_event = EM_ASM_INT_V(
        { return workerApi.getInputValue(workerApi.InputBufferAddresses.keyEventFlagAddr); });
    if (has_key_event) {
        int keycode = EM_ASM_INT_V({
            return workerApi.getInputValue(workerApi.InputBufferAddresses.keyCodeAddr);
        });
        int modifiers = EM_ASM_INT_V({
            return workerApi.getInputValue(workerApi.InputBufferAddresses.keyModifiersAddr);
        });
        int keystate = EM_ASM_INT_V({
            return workerApi.getInputValue(workerApi.InputBufferAddresses.keyStateAddr);
        });

        if (keystate == 0) {
            kms_keyup(modifiers, keycode);
        } else {
            kms_keydown(modifiers, keycode);
        }
    }

    EM_ASM({ workerApi.releaseInputLock(); });
}

EM_JS(char*, consumeDiskName, (void), {
    const diskName = workerApi.disks.consumeDiskName();
    if (!diskName || !diskName.length) {
        return 0;
    }
    const diskNameLength = lengthBytesUTF8(diskName) + 1;
    const diskNameCstr = _malloc(diskNameLength);
    stringToUTF8(diskName, diskNameCstr, diskNameLength);
    return diskNameCstr;
});

static void Main_HandleDiskInsertions(void) {
    int index = -1;
    for (int i = 0; i < ESP_MAX_DEVS; i++) {
        SCSIDISK disk = ConfigureParams.SCSI.target[i];
        if (disk.nDeviceType == SD_CD && !disk.bDiskInserted) {
            index = i;
            break;
        }
    }
    if (index == -1) {
        // No available SCSI slots.
        return;
    }
    char *diskName = consumeDiskName();
    if (diskName) {
        ConfigureParams.SCSI.target[index].szImageName[0] = '/';
        memcpy(ConfigureParams.SCSI.target[index].szImageName + 1, diskName, strlen(diskName) + 1);
        free(diskName);
        ConfigureParams.SCSI.target[index].bDiskInserted = true;
        SCSI_Insert(index);
    }
}

void Main_EventHandlerInterrupt(void) {
	CycInt_AcknowledgeInterrupt();

    Main_ReadJSInput();

    // Run polling tasks less frequently than the event loop.
    static int counter = 0;

    static int last_sleep_counter = 0;
    int64_t time_offset = host_real_time_offset();
    if (time_offset > 0) {
        // Also has the side effect of ensuring that periodic tasks are run.
        EM_ASM_({ workerApi.sleep($0); }, (double) time_offset / 1000000);
        last_sleep_counter = counter;
    }

    if (counter++ % 50 == 0) {
        Main_HandleDiskInsertions();

        if (counter - last_sleep_counter > 50) {
            // If we haven't slept in a while, at least ensure that we're
            // running periodic tasks.
            EM_ASM({ workerApi.sleep(0); });
            last_sleep_counter = counter;
        }
    }

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
