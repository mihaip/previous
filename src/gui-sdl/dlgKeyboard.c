/*
  Previous - dlgKeyboard.c

  This file is distributed under the GNU General Public License, version 2
  or at your option any later version. Read the file gpl.txt for details.
*/
const char DlgKeyboard_fileid[] = "Previous dlgKeyboard.c";

#include <unistd.h>

#include "main.h"
#include "configuration.h"
#include "dialog.h"
#include "sdlgui.h"
#include "keymap.h"

#define DLGKEY_SCANCODE  4
#define DLGKEY_SYMBOLIC  5
#define DLGKEY_SWAP      8
#define DLGKEY_EXIT      45

static char key_names[SHORTCUT_NONE][2][16];

/* The keyboard dialog: */
static SGOBJ keyboarddlg[] =
{
	{ SGBOX, 0, 0, 0,0, 47,31, NULL },
	{ SGTEXT, 0, 0, 15,1, 16,1, "Keyboard options" },

	{ SGBOX, 0, 0, 2,3, 21,7, NULL },
	{ SGTEXT, 0, 0, 4,4, 17,1, "Keyboard mapping:" },
	{ SGRADIOBUT, 0, 0, 6,6, 10,1, "Scancode" },
	{ SGRADIOBUT, 0, 0, 6,8, 10,1, "Symbolic" },

	{ SGBOX, 0, 0, 24,3, 21,7, NULL },
	{ SGTEXT, 0, 0, 26,4, 12,1, "Key options:" },
	{ SGCHECKBOX, 0, 0, 26,6, 18,1, "Swap cmd and alt" },

	{ SGBOX,  0, 0,  2,11, 43,15, NULL },
	{ SGTEXT, 0, 0,  4,12, 10,1, "Shortcuts:" },
	{ SGTEXT, 0, 0, 18,12, 10,1, "ctrl-alt-X  or  Fn" },

	{ SGTEXT, 0, 0,  6,14, 20,1, "Show main menu" },
	{ SGTEXT, 0, 0, 26,14,  8,1, key_names[SHORTCUT_OPTIONS][0] },
	{ SGTEXT, 0, 0, 34,14, 11,1, key_names[SHORTCUT_OPTIONS][1] },
	{ SGTEXT, 0, 0,  6,15, 20,1, "Pause" },
	{ SGTEXT, 0, 0, 26,15,  8,1, key_names[SHORTCUT_PAUSE][0] },
	{ SGTEXT, 0, 0, 34,15, 11,1, key_names[SHORTCUT_PAUSE][1] },
	{ SGTEXT, 0, 0,  6,16, 20,1, "Cold reset" },
	{ SGTEXT, 0, 0, 26,16,  8,1, key_names[SHORTCUT_COLDRESET][0] },
	{ SGTEXT, 0, 0, 34,16, 11,1, key_names[SHORTCUT_COLDRESET][1] },
	{ SGTEXT, 0, 0,  6,17, 20,1, "Lock/unlock mouse" },
	{ SGTEXT, 0, 0, 26,17,  8,1, key_names[SHORTCUT_MOUSEGRAB][0] },
	{ SGTEXT, 0, 0, 34,17, 11,1, key_names[SHORTCUT_MOUSEGRAB][1] },
	{ SGTEXT, 0, 0,  6,18, 20,1, "Screen toggle" },
	{ SGTEXT, 0, 0, 26,18,  8,1, key_names[SHORTCUT_DIMENSION][0] },
	{ SGTEXT, 0, 0, 34,18, 11,1, key_names[SHORTCUT_DIMENSION][1] },
	{ SGTEXT, 0, 0,  6,19, 20,1, "Grab screen" },
	{ SGTEXT, 0, 0, 26,19,  8,1, key_names[SHORTCUT_SCREENSHOT][0] },
	{ SGTEXT, 0, 0, 34,19, 11,1, key_names[SHORTCUT_SCREENSHOT][1] },
	{ SGTEXT, 0, 0,  6,20, 20,1, "Recording on/off" },
	{ SGTEXT, 0, 0, 26,20,  8,1, key_names[SHORTCUT_RECORD][0] },
	{ SGTEXT, 0, 0, 34,20, 11,1, key_names[SHORTCUT_RECORD][1] },
	{ SGTEXT, 0, 0,  6,21, 20,1, "Fullscreen on/off" },
	{ SGTEXT, 0, 0, 26,21,  8,1, key_names[SHORTCUT_FULLSCREEN][0] },
	{ SGTEXT, 0, 0, 34,21, 11,1, key_names[SHORTCUT_FULLSCREEN][1] },
	{ SGTEXT, 0, 0,  6,22, 20,1, "Show/hide statusbar" },
	{ SGTEXT, 0, 0, 26,22,  8,1, key_names[SHORTCUT_STATUSBAR][0] },
	{ SGTEXT, 0, 0, 34,22, 11,1, key_names[SHORTCUT_STATUSBAR][1] },
	{ SGTEXT, 0, 0,  6,23, 20,1, "Sound on/off" },
	{ SGTEXT, 0, 0, 26,23,  8,1, key_names[SHORTCUT_SOUND][0] },
	{ SGTEXT, 0, 0, 34,23, 11,1, key_names[SHORTCUT_SOUND][1] },
	{ SGTEXT, 0, 0,  6,24, 20,1, "Quit" },
	{ SGTEXT, 0, 0, 26,24,  8,1, key_names[SHORTCUT_QUIT][0] },
	{ SGTEXT, 0, 0, 34,24, 11,1, key_names[SHORTCUT_QUIT][1] },

	{ SGBUTTON, SG_DEFAULT, 0, 13,28, 21,1, "Back to main menu" },
	{ SGSTOP, 0, 0, 0,0, 0,0, NULL }
};


/*-----------------------------------------------------------------------*/
/**
 * Show and process the "Keyboard" dialog.
 */
void Dialog_KeyboardDlg(void)
{
	int i, but;

	SDLGui_CenterDlg(keyboarddlg);

	/* Set up dialog from actual values: */
	keyboarddlg[DLGKEY_SCANCODE].state &= ~SG_SELECTED;
	keyboarddlg[DLGKEY_SYMBOLIC].state &= ~SG_SELECTED;

	switch (ConfigureParams.Keyboard.nKeymapType) {
		case KEYMAP_SCANCODE:
			keyboarddlg[DLGKEY_SCANCODE].state |= SG_SELECTED;
			break;
		case KEYMAP_SYMBOLIC:
			keyboarddlg[DLGKEY_SYMBOLIC].state |= SG_SELECTED;
			break;
			
		default:
			break;
	}
	
	if (ConfigureParams.Keyboard.bSwapCmdAlt) {
		keyboarddlg[DLGKEY_SWAP].state |= SG_SELECTED;
	}
	else {
		keyboarddlg[DLGKEY_SWAP].state &= ~SG_SELECTED;
	}

	/* Load actual shortcut keys */
	for (i = 0; i < SHORTCUT_NONE; i++) {
		snprintf(key_names[i][0], 8,"-%s", Keymap_GetKeyName(ConfigureParams.Shortcut.withModifier[i]));
		snprintf(key_names[i][1],11, "%s", Keymap_GetKeyName(ConfigureParams.Shortcut.withoutModifier[i]));
	}

	/* Show the dialog: */
	do
	{
		but = SDLGui_DoDialog(keyboarddlg);
	}
	while (but != DLGKEY_EXIT && but != SDLGUI_QUIT
	       && but != SDLGUI_ERROR && !bQuitProgram);

	/* Read values from dialog: */
	if (keyboarddlg[DLGKEY_SCANCODE].state & SG_SELECTED)
		ConfigureParams.Keyboard.nKeymapType = KEYMAP_SCANCODE;
	else
		ConfigureParams.Keyboard.nKeymapType = KEYMAP_SYMBOLIC;

	ConfigureParams.Keyboard.bSwapCmdAlt = (keyboarddlg[DLGKEY_SWAP].state & SG_SELECTED);
}
