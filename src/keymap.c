/*
  Hatari - keymap.c

  This file is distributed under the GNU General Public License, version 2
  or at your option any later version. Read the file gpl.txt for details.

  Here we process a key press and the remapping of the scancodes.
*/
const char Keymap_fileid[] = "Hatari keymap.c";

#include <ctype.h>
#include "main.h"
#include "keymap.h"
#include "configuration.h"
#include "file.h"
#include "str.h"
#include "debugui.h"
#include "log.h"
#include "kms.h"


#define  LOG_KEYMAP_LEVEL   LOG_DEBUG

/* Definitions for NeXT scancodes *
 
 Byte at address 0x0200e00a contains modifier key values:
 x--- ----  valid
 -x-- ----  alt_right
 --x- ----  alt_left
 ---x ----  command_right
 ---- x---  command_left
 ---- -x--  shift_right
 ---- --x-  shift_left
 ---- ---x  control
 
 Byte at address 0x0200e00b contains key values:
 x--- ----  key up (1) or down (0)
 -xxx xxxx  key_code
 
 */

#define NEXTKEY_NONE            0x00
#define NEXTKEY_BRGHTNESS_DOWN  0x01
#define NEXTKEY_VOLUME_DOWN     0x02
#define NEXTKEY_BACKSLASH       0x03
#define NEXTKEY_CLOSEBRACKET    0x04
#define NEXTKEY_OPENBRACKET     0x05
#define NEXTKEY_i               0x06
#define NEXTKEY_o               0x07
#define NEXTKEY_p               0x08
#define NEXTKEY_LEFT_ARROW      0x09
/* missing */
#define NEXTKEY_KEYPAD_0        0x0B
#define NEXTKEY_KEYPAD_PERIOD   0x0C
#define NEXTKEY_KEYPAD_ENTER    0x0D
/* missing */
#define NEXTKEY_DOWN_ARROW      0x0F
#define NEXTKEY_RIGHT_ARROW     0x10
#define NEXTKEY_KEYPAD_1        0x11
#define NEXTKEY_KEYPAD_4        0x12
#define NEXTKEY_KEYPAD_6        0x13
#define NEXTKEY_KEYPAD_3        0x14
#define NEXTKEY_KEYPAD_PLUS     0x15
#define NEXTKEY_UP_ARROW        0x16
#define NEXTKEY_KEYPAD_2        0x17
#define NEXTKEY_KEYPAD_5        0x18
#define NEXTKEY_BRIGHTNESS_UP   0x19
#define NEXTKEY_VOLUME_UP       0x1A
#define NEXTKEY_DELETE          0x1B
#define NEXTKEY_EQUALS          0x1C
#define NEXTKEY_MINUS           0x1D
#define NEXTKEY_8               0x1E
#define NEXTKEY_9               0x1F
#define NEXTKEY_0               0x20
#define NEXTKEY_KEYPAD_7        0x21
#define NEXTKEY_KEYPAD_8        0x22
#define NEXTKEY_KEYPAD_9        0x23
#define NEXTKEY_KEYPAD_MINUS    0x24
#define NEXTKEY_KEYPAD_MULTIPLY 0x25
#define NEXTKEY_BACKQUOTE       0x26
#define NEXTKEY_KEYPAD_EQUALS   0x27
#define NEXTKEY_KEYPAD_DIVIDE   0x28
/* missing */
#define NEXTKEY_RETURN          0x2A
#define NEXTKEY_QUOTE           0x2B
#define NEXTKEY_SEMICOLON       0x2C
#define NEXTKEY_l               0x2D
#define NEXTKEY_COMMA           0x2E
#define NEXTKEY_PERIOD          0x2F
#define NEXTKEY_SLASH           0x30
#define NEXTKEY_z               0x31
#define NEXTKEY_x               0x32
#define NEXTKEY_c               0x33
#define NEXTKEY_v               0x34
#define NEXTKEY_b               0x35
#define NEXTKEY_m               0x36
#define NEXTKEY_n               0x37
#define NEXTKEY_SPACE           0x38
#define NEXTKEY_a               0x39
#define NEXTKEY_s               0x3A
#define NEXTKEY_d               0x3B
#define NEXTKEY_f               0x3C
#define NEXTKEY_g               0x3D
#define NEXTKEY_k               0x3E
#define NEXTKEY_j               0x3F
#define NEXTKEY_h               0x40
#define NEXTKEY_TAB             0x41
#define NEXTKEY_q               0x42
#define NEXTKEY_w               0x43
#define NEXTKEY_e               0x44
#define NEXTKEY_r               0x45
#define NEXTKEY_u               0x46
#define NEXTKEY_y               0x47
#define NEXTKEY_t               0x48
#define NEXTKEY_ESC             0x49
#define NEXTKEY_1               0x4A
#define NEXTKEY_2               0x4B
#define NEXTKEY_3               0x4C
#define NEXTKEY_4               0x4D
#define NEXTKEY_7               0x4E
#define NEXTKEY_6               0x4F
#define NEXTKEY_5               0x50
#define NEXTKEY_POWER           0x58

#define NEXTKEY_MOD_META        0x01
#define NEXTKEY_MOD_LSHIFT      0x02
#define NEXTKEY_MOD_RSHIFT      0x04
#define NEXTKEY_MOD_LCTRL       0x08
#define NEXTKEY_MOD_RCTRL       0x10
#define NEXTKEY_MOD_LALT        0x20
#define NEXTKEY_MOD_RALT        0x40


void Keymap_Init(void)
{

}

/*-----------------------------------------------------------------------*/
/**
 * This function translates the scancodes provided by SDL to NeXT
 * scancode values.
 */
static uint8_t Keymap_GetKeyFromScancode(SDL_Scancode sdlscancode)
{
	Log_Printf(LOG_KEYMAP_LEVEL, "[Keymap] Scancode: %i (%s)\n", sdlscancode, SDL_GetScancodeName(sdlscancode));

	switch (sdlscancode) {
		case SDL_SCANCODE_ESCAPE: return 0x49;
		case SDL_SCANCODE_GRAVE: return 0x49;
		case SDL_SCANCODE_1: return 0x4a;
		case SDL_SCANCODE_2: return 0x4b;
		case SDL_SCANCODE_3: return 0x4c;
		case SDL_SCANCODE_4: return 0x4d;
		case SDL_SCANCODE_5: return 0x50;
		case SDL_SCANCODE_6: return 0x4f;
		case SDL_SCANCODE_7: return 0x4e;
		case SDL_SCANCODE_8: return 0x1e;
		case SDL_SCANCODE_9: return 0x1f;
		case SDL_SCANCODE_0: return 0x20;
		case SDL_SCANCODE_MINUS: return 0x1d;
		case SDL_SCANCODE_EQUALS: return 0x1c;
		case SDL_SCANCODE_BACKSPACE: return 0x1b;

		case SDL_SCANCODE_TAB: return 0x41;
		case SDL_SCANCODE_Q: return 0x42;
		case SDL_SCANCODE_W: return 0x43;
		case SDL_SCANCODE_E: return 0x44;
		case SDL_SCANCODE_R: return 0x45;
		case SDL_SCANCODE_T: return 0x48;
		case SDL_SCANCODE_Y: return 0x47;
		case SDL_SCANCODE_U: return 0x46;
		case SDL_SCANCODE_I: return 0x06;
		case SDL_SCANCODE_O: return 0x07;
		case SDL_SCANCODE_P: return 0x08;
		case SDL_SCANCODE_LEFTBRACKET: return 0x05;
		case SDL_SCANCODE_RIGHTBRACKET: return 0x04;
		case SDL_SCANCODE_BACKSLASH: return 0x03;

		case SDL_SCANCODE_A: return 0x39;
		case SDL_SCANCODE_S: return 0x3a;
		case SDL_SCANCODE_D: return 0x3b;
		case SDL_SCANCODE_F: return 0x3c;
		case SDL_SCANCODE_G: return 0x3d;
		case SDL_SCANCODE_H: return 0x40;
		case SDL_SCANCODE_J: return 0x3f;
		case SDL_SCANCODE_K: return 0x3e;
		case SDL_SCANCODE_L: return 0x2d;
		case SDL_SCANCODE_SEMICOLON: return 0x2c;
		case SDL_SCANCODE_APOSTROPHE: return 0x2b;
		case SDL_SCANCODE_RETURN: return 0x2a;

		case SDL_SCANCODE_NONUSBACKSLASH: return 0x26;
		case SDL_SCANCODE_Z: return 0x31;
		case SDL_SCANCODE_X: return 0x32;
		case SDL_SCANCODE_C: return 0x33;
		case SDL_SCANCODE_V: return 0x34;
		case SDL_SCANCODE_B: return 0x35;
		case SDL_SCANCODE_N: return 0x37;
		case SDL_SCANCODE_M: return 0x36;
		case SDL_SCANCODE_COMMA: return 0x2e;
		case SDL_SCANCODE_PERIOD: return 0x2f;
		case SDL_SCANCODE_SLASH: return 0x30;
		case SDL_SCANCODE_SPACE: return 0x38;

		case SDL_SCANCODE_NUMLOCKCLEAR: return 0x26;
		case SDL_SCANCODE_KP_EQUALS: return 0x27;
		case SDL_SCANCODE_KP_DIVIDE: return 0x28;
		case SDL_SCANCODE_KP_MULTIPLY: return 0x25;
		case SDL_SCANCODE_KP_7: return 0x21;
		case SDL_SCANCODE_KP_8: return 0x22;
		case SDL_SCANCODE_KP_9: return 0x23;
		case SDL_SCANCODE_KP_MINUS: return 0x24;
		case SDL_SCANCODE_KP_4: return 0x12;
		case SDL_SCANCODE_KP_5: return 0x18;
		case SDL_SCANCODE_KP_6: return 0x13;
		case SDL_SCANCODE_KP_PLUS: return 0x15;
		case SDL_SCANCODE_KP_1: return 0x11;
		case SDL_SCANCODE_KP_2: return 0x17;
		case SDL_SCANCODE_KP_3: return 0x14;
		case SDL_SCANCODE_KP_0: return 0x0b;
		case SDL_SCANCODE_KP_PERIOD: return 0x0c;
		case SDL_SCANCODE_KP_ENTER: return 0x0d;

		case SDL_SCANCODE_LEFT: return 0x09;
		case SDL_SCANCODE_RIGHT: return 0x10;
		case SDL_SCANCODE_UP: return 0x16;
		case SDL_SCANCODE_DOWN: return 0x0f;

		/* Special keys */
		case SDL_SCANCODE_F10:
		case SDL_SCANCODE_DELETE: return 0x58;   /* Power */
		case SDL_SCANCODE_F5:
		case SDL_SCANCODE_END: return 0x02;      /* Sound down */
		case SDL_SCANCODE_F6:
		case SDL_SCANCODE_HOME: return 0x1a;     /* Sound up */
		case SDL_SCANCODE_F1:
		case SDL_SCANCODE_PAGEDOWN: return 0x01; /* Brightness down */
		case SDL_SCANCODE_F2:
		case SDL_SCANCODE_PAGEUP: return 0x19;   /* Brightness up */

		default: return 0x00;
	}
}


/*-----------------------------------------------------------------------*/
/**
 * This function translates the key symbols provided by SDL to NeXT
 * scancode values.
 */
static uint8_t Keymap_GetKeyFromSymbol(SDL_Keycode sdlkey)
{
	Log_Printf(LOG_KEYMAP_LEVEL, "[Keymap] Symkey: %s\n", SDL_GetKeyName(sdlkey));

	switch (sdlkey) {
		case SDLK_BACKSLASH: return 0x03;
		case SDLK_RIGHTBRACKET: return 0x04;
		case SDLK_LEFTBRACKET: return 0x05;
		case SDLK_i: return 0x06;
		case SDLK_o: return 0x07;
		case SDLK_p: return 0x08;
		case SDLK_LEFT: return 0x09;
		case SDLK_KP_0: return 0x0B;
		case SDLK_KP_PERIOD: return 0x0C;
		case SDLK_KP_ENTER: return 0x0D;
		case SDLK_DOWN: return 0x0F;
		case SDLK_RIGHT: return 0x10;
		case SDLK_KP_1: return 0x11;
		case SDLK_KP_4: return 0x12;
		case SDLK_KP_6: return 0x13;
		case SDLK_KP_3: return 0x14;
		case SDLK_KP_PLUS: return 0x15;
		case SDLK_UP: return 0x16;
		case SDLK_KP_2: return 0x17;
		case SDLK_KP_5: return 0x18;
		case SDLK_BACKSPACE: return 0x1B;
		case SDLK_EQUALS: return 0x1C;
		case SDLK_MINUS: return 0x1D;
		case SDLK_8: return 0x1E;
		case SDLK_9: return 0x1F;
		case SDLK_0: return 0x20;
		case SDLK_KP_7: return 0x21;
		case SDLK_KP_8: return 0x22;
		case SDLK_KP_9: return 0x23;
		case SDLK_KP_MINUS: return 0x24;
		case SDLK_KP_MULTIPLY: return 0x25;
		case SDLK_BACKQUOTE: return 0x26;
		case SDLK_KP_EQUALS: return 0x27;
		case SDLK_KP_DIVIDE: return 0x28;
		case SDLK_RETURN: return 0x2A;
		case SDLK_QUOTE: return 0x2B;
		case SDLK_SEMICOLON: return 0x2C;
		case SDLK_l: return 0x2D;
		case SDLK_COMMA: return 0x2E;
		case SDLK_PERIOD: return 0x2F;
		case SDLK_SLASH: return 0x30;
		case SDLK_z: return 0x31;
		case SDLK_x: return 0x32;
		case SDLK_c: return 0x33;
		case SDLK_v: return 0x34;
		case SDLK_b: return 0x35;
		case SDLK_m: return 0x36;
		case SDLK_n: return 0x37;
		case SDLK_SPACE: return 0x38;
		case SDLK_a: return 0x39;
		case SDLK_s: return 0x3A;
		case SDLK_d: return 0x3B;
		case SDLK_f: return 0x3C;
		case SDLK_g: return 0x3D;
		case SDLK_k: return 0x3E;
		case SDLK_j: return 0x3F;
		case SDLK_h: return 0x40;
		case SDLK_TAB: return 0x41;
		case SDLK_q: return 0x42;
		case SDLK_w: return 0x43;
		case SDLK_e: return 0x44;
		case SDLK_r: return 0x45;
		case SDLK_u: return 0x46;
		case SDLK_y: return 0x47;
		case SDLK_t: return 0x48;
		case SDLK_ESCAPE: return 0x49;
		case SDLK_1: return 0x4A;
		case SDLK_2: return 0x4B;
		case SDLK_3: return 0x4C;
		case SDLK_4: return 0x4D;
		case SDLK_7: return 0x4E;
		case SDLK_6: return 0x4F;
		case SDLK_5: return 0x50;

		/* Special Keys */
		case SDLK_F10:
		case SDLK_DELETE: return 0x58;   /* Power */
		case SDLK_F5:
		case SDLK_END: return 0x02;      /* Sound down */
		case SDLK_F6:
		case SDLK_HOME: return 0x1a;     /* Sound up */
		case SDLK_F1:
		case SDLK_PAGEDOWN: return 0x01; /* Brightness down */
		case SDLK_F2:
		case SDLK_PAGEUP: return 0x19;   /* Brightness up */

		default: return 0x00;
	}
}


/*-----------------------------------------------------------------------*/
/**
 * This functions translates the modifieres provided by SDL to NeXT 
 * modifier bits.
 */
static uint8_t Keymap_GetModifiers(uint16_t mod)
{
	uint8_t modifiers = 0;

	if (mod & KMOD_CTRL) {
		modifiers |= 0x01;
	}
	if (mod & KMOD_LSHIFT) {
		modifiers |= 0x02;
	}
	if (mod & KMOD_RSHIFT) {
		modifiers |= 0x04;
	}
	if (mod & KMOD_LGUI) {
		modifiers |= ConfigureParams.Keyboard.bSwapCmdAlt?0x20:0x08;
	}
	if (mod & KMOD_RGUI) {
		modifiers |= ConfigureParams.Keyboard.bSwapCmdAlt?0x40:0x10;
	}
	if (mod & KMOD_LALT) {
		modifiers |= ConfigureParams.Keyboard.bSwapCmdAlt?0x08:0x20;
	}
	if (mod & KMOD_RALT) {
		modifiers |= ConfigureParams.Keyboard.bSwapCmdAlt?0x10:0x40;
	}
	if (mod & KMOD_CAPS) {
		modifiers |= 0x02;
	}
	return modifiers;
}


/*-----------------------------------------------------------------------*/
/**
 * Mouse wheel mapped to cursor keys (currently disabled)
 */

static bool pendingX = true;
static bool pendingY = true;

static void post_key_event(int sym, int scan)
{
	SDL_Event sdlevent;
	sdlevent.type = SDL_KEYDOWN;
	sdlevent.key.keysym.sym      = sym;
	sdlevent.key.keysym.scancode = scan;
	SDL_PushEvent(&sdlevent);
	sdlevent.type = SDL_KEYUP;
	sdlevent.key.keysym.sym      = sym;
	sdlevent.key.keysym.scancode = scan;
	SDL_PushEvent(&sdlevent);
}

void Keymap_MouseWheel(SDL_MouseWheelEvent* event)
{
	if(!(pendingX)) {
		pendingX = true;
		if     (event->x > 0) post_key_event(SDLK_LEFT,  SDL_SCANCODE_LEFT);
		else if(event->x < 0) post_key_event(SDLK_RIGHT, SDL_SCANCODE_RIGHT);
	}

	if(!(pendingY)) {
		pendingY = true;
		if     (event->y < 0) post_key_event(SDLK_UP,   SDL_SCANCODE_UP);
		else if(event->y > 0) post_key_event(SDLK_DOWN, SDL_SCANCODE_DOWN);
	}
}


/*-----------------------------------------------------------------------*/
/**
 * User pressed key down
 */
void Keymap_KeyDown(const SDL_Keysym *sdlkey)
{
	uint8_t next_mod, next_key;

	if (ConfigureParams.Keyboard.nKeymapType==KEYMAP_SYMBOLIC) {
		next_key = Keymap_GetKeyFromSymbol(sdlkey->sym);
	} else {
		next_key = Keymap_GetKeyFromScancode(sdlkey->scancode);
	}

	next_mod = Keymap_GetModifiers(sdlkey->mod);

	Log_Printf(LOG_KEYMAP_LEVEL, "[Keymap] NeXT Keycode: $%02x, Modifiers: $%02x\n", next_key, next_mod);

	kms_keydown(next_mod, next_key);
}


/*-----------------------------------------------------------------------*/
/**
 * User released key
 */
void Keymap_KeyUp(const SDL_Keysym *sdlkey)
{
	uint8_t next_mod, next_key;

	if (ConfigureParams.Keyboard.nKeymapType==KEYMAP_SYMBOLIC) {
		next_key = Keymap_GetKeyFromSymbol(sdlkey->sym);
	} else {
		next_key = Keymap_GetKeyFromScancode(sdlkey->scancode);
	}

	next_mod = Keymap_GetModifiers(sdlkey->mod);

	Log_Printf(LOG_KEYMAP_LEVEL, "[Keymap] NeXT Keycode: $%02x, Modifiers: $%02x\n", next_key, next_mod);

	kms_keyup(next_mod, next_key);
}


/*-----------------------------------------------------------------------*/
/**
 * Simulate press or release of a key corresponding to given character
 */
void Keymap_SimulateCharacter(char asckey, bool press)
{
	SDL_Keysym sdlkey;

	sdlkey.mod = KMOD_NONE;
	sdlkey.scancode = 0;
	if (isupper((unsigned char)asckey))
	{
		if (press)
		{
			sdlkey.sym = SDLK_LSHIFT;
			Keymap_KeyDown(&sdlkey);
		}
		sdlkey.sym = tolower((unsigned char)asckey);
		sdlkey.mod = KMOD_LSHIFT;
	}
	else
	{
		sdlkey.sym = asckey;
	}
	if (press)
	{
		Keymap_KeyDown(&sdlkey);
	}
	else
	{
		Keymap_KeyUp(&sdlkey);
		if (isupper((unsigned char)asckey))
		{
			sdlkey.sym = SDLK_LSHIFT;
			Keymap_KeyUp(&sdlkey);
		}
	}
}


/*-----------------------------------------------------------------------*/
/**
 * User moved mouse
 */
void Keymap_MouseMove(int dx, int dy)
{
	bool left = false;
	bool up   = false;

	if (dx < 0) {
		dx = -dx;
		left = true;
	}
	if (dy < 0) {
		dy = -dy;
		up = true;
	}
	kms_mouse_move(dx, left, dy, up);
}

/*-----------------------------------------------------------------------*/
/**
 * User pressed mouse button
 */
void Keymap_MouseDown(bool left)
{
	kms_mouse_button(left,true);
}

/*-----------------------------------------------------------------------*/
/**
 * User released mouse button
 */
void Keymap_MouseUp(bool left)
{
	kms_mouse_button(left,false);
}


int Keymap_GetKeyFromName(const char *name)
{
	return SDL_GetKeyFromName(name);
}

const char *Keymap_GetKeyName(int keycode)
{
	if (!keycode)
		return "";

	return SDL_GetKeyName(keycode);
}
