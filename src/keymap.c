/*
  Previous - keymap.c

  This file is distributed under the GNU General Public License, version 2
  or at your option any later version. Read the file gpl.txt for details.

  Here we translate key presses and mouse motion.
*/
const char Keymap_fileid[] = "Previous keymap.c";

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


/* ------- NeXT scancodes ------- */
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


void Keymap_Init(void) {}

/*-----------------------------------------------------------------------*/
/**
 * This function translates the scancodes provided by SDL to NeXT
 * scancode values.
 */
static uint8_t Keymap_GetKeyFromScancode(SDL_Scancode sdlscancode)
{
	Log_Printf(LOG_KEYMAP_LEVEL, "[Keymap] Scancode: %i (%s)\n", sdlscancode, SDL_GetScancodeName(sdlscancode));

	switch (sdlscancode) {
		case SDL_SCANCODE_ESCAPE:
		case SDL_SCANCODE_GRAVE:          return NEXTKEY_ESC;
		case SDL_SCANCODE_1:              return NEXTKEY_1;
		case SDL_SCANCODE_2:              return NEXTKEY_2;
		case SDL_SCANCODE_3:              return NEXTKEY_3;
		case SDL_SCANCODE_4:              return NEXTKEY_4;
		case SDL_SCANCODE_5:              return NEXTKEY_5;
		case SDL_SCANCODE_6:              return NEXTKEY_6;
		case SDL_SCANCODE_7:              return NEXTKEY_7;
		case SDL_SCANCODE_8:              return NEXTKEY_8;
		case SDL_SCANCODE_9:              return NEXTKEY_9;
		case SDL_SCANCODE_0:              return NEXTKEY_0;
		case SDL_SCANCODE_MINUS:          return NEXTKEY_MINUS;
		case SDL_SCANCODE_EQUALS:         return NEXTKEY_EQUALS;
		case SDL_SCANCODE_BACKSPACE:      return NEXTKEY_DELETE;

		case SDL_SCANCODE_TAB:            return NEXTKEY_TAB;
		case SDL_SCANCODE_Q:              return NEXTKEY_q;
		case SDL_SCANCODE_W:              return NEXTKEY_w;
		case SDL_SCANCODE_E:              return NEXTKEY_e;
		case SDL_SCANCODE_R:              return NEXTKEY_r;
		case SDL_SCANCODE_T:              return NEXTKEY_t;
		case SDL_SCANCODE_Y:              return NEXTKEY_y;
		case SDL_SCANCODE_U:              return NEXTKEY_u;
		case SDL_SCANCODE_I:              return NEXTKEY_i;
		case SDL_SCANCODE_O:              return NEXTKEY_o;
		case SDL_SCANCODE_P:              return NEXTKEY_p;
		case SDL_SCANCODE_LEFTBRACKET:    return NEXTKEY_OPENBRACKET;
		case SDL_SCANCODE_RIGHTBRACKET:   return NEXTKEY_CLOSEBRACKET;
		case SDL_SCANCODE_BACKSLASH:      return NEXTKEY_BACKSLASH;

		case SDL_SCANCODE_A:              return NEXTKEY_a;
		case SDL_SCANCODE_S:              return NEXTKEY_s;
		case SDL_SCANCODE_D:              return NEXTKEY_d;
		case SDL_SCANCODE_F:              return NEXTKEY_f;
		case SDL_SCANCODE_G:              return NEXTKEY_g;
		case SDL_SCANCODE_H:              return NEXTKEY_h;
		case SDL_SCANCODE_J:              return NEXTKEY_j;
		case SDL_SCANCODE_K:              return NEXTKEY_k;
		case SDL_SCANCODE_L:              return NEXTKEY_l;
		case SDL_SCANCODE_SEMICOLON:      return NEXTKEY_SEMICOLON;
		case SDL_SCANCODE_APOSTROPHE:     return NEXTKEY_QUOTE;
		case SDL_SCANCODE_RETURN:         return NEXTKEY_RETURN;

		case SDL_SCANCODE_NONUSBACKSLASH: return NEXTKEY_BACKQUOTE;
		case SDL_SCANCODE_Z:              return NEXTKEY_z;
		case SDL_SCANCODE_X:              return NEXTKEY_x;
		case SDL_SCANCODE_C:              return NEXTKEY_c;
		case SDL_SCANCODE_V:              return NEXTKEY_v;
		case SDL_SCANCODE_B:              return NEXTKEY_b;
		case SDL_SCANCODE_N:              return NEXTKEY_n;
		case SDL_SCANCODE_M:              return NEXTKEY_m;
		case SDL_SCANCODE_COMMA:          return NEXTKEY_COMMA;
		case SDL_SCANCODE_PERIOD:         return NEXTKEY_PERIOD;
		case SDL_SCANCODE_SLASH:          return NEXTKEY_SLASH;
		case SDL_SCANCODE_SPACE:          return NEXTKEY_SPACE;

		case SDL_SCANCODE_NUMLOCKCLEAR:   return NEXTKEY_BACKQUOTE;
		case SDL_SCANCODE_KP_EQUALS:      return NEXTKEY_KEYPAD_EQUALS;
		case SDL_SCANCODE_KP_DIVIDE:      return NEXTKEY_KEYPAD_DIVIDE;
		case SDL_SCANCODE_KP_MULTIPLY:    return NEXTKEY_KEYPAD_MULTIPLY;
		case SDL_SCANCODE_KP_7:           return NEXTKEY_KEYPAD_7;
		case SDL_SCANCODE_KP_8:           return NEXTKEY_KEYPAD_8;
		case SDL_SCANCODE_KP_9:           return NEXTKEY_KEYPAD_9;
		case SDL_SCANCODE_KP_MINUS:       return NEXTKEY_KEYPAD_MINUS;
		case SDL_SCANCODE_KP_4:           return NEXTKEY_KEYPAD_4;
		case SDL_SCANCODE_KP_5:           return NEXTKEY_KEYPAD_5;
		case SDL_SCANCODE_KP_6:           return NEXTKEY_KEYPAD_6;
		case SDL_SCANCODE_KP_PLUS:        return NEXTKEY_KEYPAD_PLUS;
		case SDL_SCANCODE_KP_1:           return NEXTKEY_KEYPAD_1;
		case SDL_SCANCODE_KP_2:           return NEXTKEY_KEYPAD_2;
		case SDL_SCANCODE_KP_3:           return NEXTKEY_KEYPAD_3;
		case SDL_SCANCODE_KP_0:           return NEXTKEY_KEYPAD_0;
		case SDL_SCANCODE_KP_PERIOD:      return NEXTKEY_KEYPAD_PERIOD;
		case SDL_SCANCODE_KP_ENTER:       return NEXTKEY_KEYPAD_ENTER;

		case SDL_SCANCODE_LEFT:           return NEXTKEY_LEFT_ARROW;
		case SDL_SCANCODE_RIGHT:          return NEXTKEY_RIGHT_ARROW;
		case SDL_SCANCODE_UP:             return NEXTKEY_UP_ARROW;
		case SDL_SCANCODE_DOWN:           return NEXTKEY_DOWN_ARROW;

		/* Special keys */
		case SDL_SCANCODE_F10:
		case SDL_SCANCODE_DELETE:         return NEXTKEY_POWER;
		case SDL_SCANCODE_F5:
		case SDL_SCANCODE_END:            return NEXTKEY_VOLUME_DOWN;
		case SDL_SCANCODE_F6:
		case SDL_SCANCODE_HOME:           return NEXTKEY_VOLUME_UP;
		case SDL_SCANCODE_F1:
		case SDL_SCANCODE_PAGEDOWN:       return NEXTKEY_BRGHTNESS_DOWN;
		case SDL_SCANCODE_F2:
		case SDL_SCANCODE_PAGEUP:         return NEXTKEY_BRIGHTNESS_UP;

		default:                          return NEXTKEY_NONE;
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
		case SDLK_BACKSLASH:              return NEXTKEY_BACKSLASH;
		case SDLK_RIGHTBRACKET:           return NEXTKEY_CLOSEBRACKET;
		case SDLK_LEFTBRACKET:            return NEXTKEY_OPENBRACKET;
		case SDLK_i:                      return NEXTKEY_i;
		case SDLK_o:                      return NEXTKEY_o;
		case SDLK_p:                      return NEXTKEY_p;
		case SDLK_LEFT:                   return NEXTKEY_LEFT_ARROW;
		case SDLK_KP_0:                   return NEXTKEY_KEYPAD_0;
		case SDLK_KP_PERIOD:              return NEXTKEY_KEYPAD_PERIOD;
		case SDLK_KP_ENTER:               return NEXTKEY_KEYPAD_ENTER;
		case SDLK_DOWN:                   return NEXTKEY_DOWN_ARROW;
		case SDLK_RIGHT:                  return NEXTKEY_RIGHT_ARROW;
		case SDLK_KP_1:                   return NEXTKEY_KEYPAD_1;
		case SDLK_KP_4:                   return NEXTKEY_KEYPAD_4;
		case SDLK_KP_6:                   return NEXTKEY_KEYPAD_6;
		case SDLK_KP_3:                   return NEXTKEY_KEYPAD_3;
		case SDLK_KP_PLUS:                return NEXTKEY_KEYPAD_PLUS;
		case SDLK_UP:                     return NEXTKEY_UP_ARROW;
		case SDLK_KP_2:                   return NEXTKEY_KEYPAD_2;
		case SDLK_KP_5:                   return NEXTKEY_KEYPAD_5;
		case SDLK_BACKSPACE:              return NEXTKEY_DELETE;
		case SDLK_EQUALS:                 return NEXTKEY_EQUALS;
		case SDLK_MINUS:                  return NEXTKEY_MINUS;
		case SDLK_8:                      return NEXTKEY_8;
		case SDLK_9:                      return NEXTKEY_9;
		case SDLK_0:                      return NEXTKEY_0;
		case SDLK_KP_7:                   return NEXTKEY_KEYPAD_7;
		case SDLK_KP_8:                   return NEXTKEY_KEYPAD_8;
		case SDLK_KP_9:                   return NEXTKEY_KEYPAD_9;
		case SDLK_KP_MINUS:               return NEXTKEY_KEYPAD_MINUS;
		case SDLK_KP_MULTIPLY:            return NEXTKEY_KEYPAD_MULTIPLY;
		case SDLK_BACKQUOTE:              return NEXTKEY_BACKQUOTE;
		case SDLK_KP_EQUALS:              return NEXTKEY_KEYPAD_EQUALS;
		case SDLK_KP_DIVIDE:              return NEXTKEY_KEYPAD_DIVIDE;
		case SDLK_RETURN:                 return NEXTKEY_RETURN;
		case SDLK_QUOTE:                  return NEXTKEY_QUOTE;
		case SDLK_SEMICOLON:              return NEXTKEY_SEMICOLON;
		case SDLK_l:                      return NEXTKEY_l;
		case SDLK_COMMA:                  return NEXTKEY_COMMA;
		case SDLK_PERIOD:                 return NEXTKEY_PERIOD;
		case SDLK_SLASH:                  return NEXTKEY_SLASH;
		case SDLK_z:                      return NEXTKEY_z;
		case SDLK_x:                      return NEXTKEY_x;
		case SDLK_c:                      return NEXTKEY_c;
		case SDLK_v:                      return NEXTKEY_v;
		case SDLK_b:                      return NEXTKEY_b;
		case SDLK_m:                      return NEXTKEY_m;
		case SDLK_n:                      return NEXTKEY_n;
		case SDLK_SPACE:                  return NEXTKEY_SPACE;
		case SDLK_a:                      return NEXTKEY_a;
		case SDLK_s:                      return NEXTKEY_s;
		case SDLK_d:                      return NEXTKEY_d;
		case SDLK_f:                      return NEXTKEY_f;
		case SDLK_g:                      return NEXTKEY_g;
		case SDLK_k:                      return NEXTKEY_k;
		case SDLK_j:                      return NEXTKEY_j;
		case SDLK_h:                      return NEXTKEY_h;
		case SDLK_TAB:                    return NEXTKEY_TAB;
		case SDLK_q:                      return NEXTKEY_q;
		case SDLK_w:                      return NEXTKEY_w;
		case SDLK_e:                      return NEXTKEY_e;
		case SDLK_r:                      return NEXTKEY_r;
		case SDLK_u:                      return NEXTKEY_u;
		case SDLK_y:                      return NEXTKEY_y;
		case SDLK_t:                      return NEXTKEY_t;
		case SDLK_ESCAPE:                 return NEXTKEY_ESC;
		case SDLK_1:                      return NEXTKEY_1;
		case SDLK_2:                      return NEXTKEY_2;
		case SDLK_3:                      return NEXTKEY_3;
		case SDLK_4:                      return NEXTKEY_4;
		case SDLK_7:                      return NEXTKEY_7;
		case SDLK_6:                      return NEXTKEY_6;
		case SDLK_5:                      return NEXTKEY_5;

		/* Special Keys */
		case SDLK_F10:
		case SDLK_DELETE:                 return NEXTKEY_POWER;
		case SDLK_F5:
		case SDLK_END:                    return NEXTKEY_VOLUME_DOWN;
		case SDLK_F6:
		case SDLK_HOME:                   return NEXTKEY_VOLUME_UP;
		case SDLK_F1:
		case SDLK_PAGEDOWN:               return NEXTKEY_BRGHTNESS_DOWN;
		case SDLK_F2:
		case SDLK_PAGEUP:                 return NEXTKEY_BRIGHTNESS_UP;

		default:                          return NEXTKEY_NONE;
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
		modifiers |= NEXTKEY_MOD_META;
	}
	if (mod & KMOD_LSHIFT) {
		modifiers |= NEXTKEY_MOD_LSHIFT;
	}
	if (mod & KMOD_RSHIFT) {
		modifiers |= NEXTKEY_MOD_RSHIFT;
	}
	if (mod & KMOD_LGUI) {
		modifiers |= ConfigureParams.Keyboard.bSwapCmdAlt?NEXTKEY_MOD_LALT:NEXTKEY_MOD_LCTRL;
	}
	if (mod & KMOD_RGUI) {
		modifiers |= ConfigureParams.Keyboard.bSwapCmdAlt?NEXTKEY_MOD_RALT:NEXTKEY_MOD_RCTRL;
	}
	if (mod & KMOD_LALT) {
		modifiers |= ConfigureParams.Keyboard.bSwapCmdAlt?NEXTKEY_MOD_LCTRL:NEXTKEY_MOD_LALT;
	}
	if (mod & KMOD_RALT) {
		modifiers |= ConfigureParams.Keyboard.bSwapCmdAlt?NEXTKEY_MOD_RCTRL:NEXTKEY_MOD_RALT;
	}
	if (mod & KMOD_CAPS) {
		modifiers |= NEXTKEY_MOD_LSHIFT;
	}
	return modifiers;
}


/*-----------------------------------------------------------------------*/
/**
 * Mouse wheel mapped to cursor keys
 */
static void post_key_event(int sym, int scan)
{
	SDL_Event sdlevent;
	sdlevent.type = SDL_KEYDOWN;
	sdlevent.key.keysym.sym      = sym;
	sdlevent.key.keysym.scancode = scan;
	sdlevent.key.keysym.mod      = KMOD_NONE;
	SDL_PushEvent(&sdlevent);
	sdlevent.type = SDL_KEYUP;
	sdlevent.key.keysym.sym      = sym;
	sdlevent.key.keysym.scancode = scan;
	sdlevent.key.keysym.mod      = KMOD_NONE;
	SDL_PushEvent(&sdlevent);
}

void Keymap_MouseWheel(SDL_MouseWheelEvent* event)
{
	int32_t x, y;
	
	if (ConfigureParams.Mouse.bEnableMapToKey) {
		x = event->x;
		y = event->y;
		
		if (event->direction == SDL_MOUSEWHEEL_FLIPPED) {
			x = -x;
			y = -y;
		}
		
		if      (x < 0) post_key_event(SDLK_LEFT,  SDL_SCANCODE_LEFT);
		else if (x > 0) post_key_event(SDLK_RIGHT, SDL_SCANCODE_RIGHT);
		
		if      (y < 0) post_key_event(SDLK_DOWN,  SDL_SCANCODE_DOWN);
		else if (y > 0) post_key_event(SDLK_UP,    SDL_SCANCODE_UP);
		
		Log_Printf(LOG_KEYMAP_LEVEL, "[Keymap] Scrolling x = %d, y = %d\n", x, y);
	}
}


/*-----------------------------------------------------------------------*/
/**
 * User pressed key down
 */
void Keymap_KeyDown(const SDL_Keysym *sdlkey)
{
	uint8_t next_mod, next_key;

	if (ConfigureParams.Keyboard.nKeymapType == KEYMAP_SYMBOLIC) {
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

	if (ConfigureParams.Keyboard.nKeymapType == KEYMAP_SYMBOLIC) {
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


/*-----------------------------------------------------------------------*/
/**
 * For use in configuration file routines
 */
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
