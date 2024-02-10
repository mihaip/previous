/*
  Previous - adb.c

  This file is distributed under the GNU General Public License, version 2
  or at your option any later version. Read the file gpl.txt for details.

  This file contains a simulation of the Apple Desktop Bus.
*/
const char Adb_fileid[] = "Previous adb.c";

#include "main.h"
#include "configuration.h"
#include "m68000.h"
#include "sysdeps.h"
#include "sysReg.h"
#include "rtcnvram.h"
#include "adb.h"

#define LOG_ADB_LEVEL      LOG_DEBUG
#define LOG_ADB_CMD_LEVEL  LOG_DEBUG


/* ADB devices */
#define ADB_ADDR_INVAL  0
#define ADB_ADDR_KBD    2
#define ADB_ADDR_MOUSE  3
#define ADB_ADDR_TABLET 4

#define ADB_TYPE_MOUSE  1
#define ADB_TYPE_KBD    2

#define ADB_MAX_EVENTS 16

/* Keyboard */
static struct {
	uint8_t addr;
	uint8_t event[ADB_MAX_EVENTS];
	int write;
	int read;
	int next;
} adb_kbd;

static void adb_kbd_flush(void) {
	adb_kbd.read = adb_kbd.write = 0;
}

static void adb_kbd_reset(void) {
	adb_kbd.addr = ADB_ADDR_KBD;
	adb_kbd_flush();
}

static bool adb_kbd_request(void) {
	return (adb_kbd.write != adb_kbd.read);
}

static void adb_kbd_put(uint8_t event) {
	adb_kbd.next = adb_kbd.write + 1;
	if (adb_kbd.next >= ADB_MAX_EVENTS) {
		adb_kbd.next = 0;
	}
	if (adb_kbd.next == adb_kbd.read) {
		Log_Printf(LOG_WARN, "[ADB] Keyboard events queue overflow");
	} else {
		adb_kbd.event[adb_kbd.write] = event;
		adb_kbd.write = adb_kbd.next;
	}
}

static bool adb_kbd_get(uint8_t* event) {	
	if (adb_kbd.write != adb_kbd.read) {
		adb_kbd.next = adb_kbd.read + 1;
		if (adb_kbd.next >= ADB_MAX_EVENTS) {
			adb_kbd.next = 0;
		}
		*event = adb_kbd.event[adb_kbd.read];
		adb_kbd.read = adb_kbd.next;
		return true;
	}
	*event = 0xff;
	return false;
}

static void adb_keyup(uint8_t key) {
	if (key == 0x7f) {
		rtc_stop_pdown_request();
	} else if (key < 0x7f) {
		adb_kbd_put(0x80 | key);
	}
}

static void adb_keydown(uint8_t key) {
	if (key == 0x7f) {
		rtc_request_power_down();
	} else if (key < 0x7f) {
		adb_kbd_put(key);
	}
}

/* Mouse */
static struct {
	uint8_t addr;
	bool event;
	bool right;
	bool left;
	int x;
	int y;
} adb_mouse;

static void adb_mouse_flush(void) {
	adb_mouse.event = false;
}

static void adb_mouse_reset(void) {
	adb_mouse.addr = ADB_ADDR_MOUSE;
	adb_mouse_flush();
}

static bool adb_mouse_request(void) {
	return adb_mouse.event;
}

static bool adb_mouse_get(uint8_t* event) {
	if (adb_mouse.event) {
		if (adb_mouse.y < 0) {
			event[0] = (0x40 + adb_mouse.y) | 0x40;
		} else {
			event[0] = adb_mouse.y;
		}
		if (adb_mouse.x < 0) {
			event[1] = (0x40 + adb_mouse.x) | 0x40;
		} else {
			event[1] = adb_mouse.x;
		}
		if (adb_mouse.left == 0) {
			event[0] |= 0x80;
		}
		if (adb_mouse.right == 0) {
			event[1] |= 0x80;
		}
		adb_mouse.event = false;
		
		return true;
	}
	return false;
}

static void adb_mouse_move(int x, int y) {
	adb_mouse.x = x;
	adb_mouse.y = y;
	adb_mouse.event = true;
}

static void adb_mouse_button(bool left, bool down) {
	if (left) {
		adb_mouse.left  = down;
	} else {
		adb_mouse.right = down;
	}
	adb_mouse.event = true;
}


/* ADB registers */
static struct {
	uint32_t intstatus;
	uint32_t intmask;
	uint32_t config;
	uint32_t status;
	uint32_t command;
	uint32_t bitcount;
	uint32_t data0;
	uint32_t data1;
} adb;

#define ADB_INTSTATUS   0x00 /* rw */
#define ADB_INTMASK     0x08 /* rw */
#define ADB_SETINT      0x10 /* w */
#define ADB_CONFIG      0x18 /* rw */
#define ADB_CTRL        0x20 /* w */
#define ADB_STATUS      0x28 /* r */
#define ADB_CMD         0x30 /* rw */
#define ADB_COUNT       0x38 /* rw */

#define ADB_DATA0       0x80 /* rw */
#define ADB_DATA1       0x88 /* rw */


/* ADB interrupt registers */
#define ADB_INT_REJECT      0x01
#define ADB_INT_POLLSTOP    0x02
#define ADB_INT_ACCESS      0x04
#define ADB_INT_RESET       0x08

/* ADB configuration register */
#define ADB_CONF_SYSTEM     0x01
#define ADB_CONF_WATCHDOG   0x02

/* ADB control register */
#define ADB_CTRL_EN_POLL    0x01
#define ADB_CTRL_DIS_POLL   0x02
#define ADB_CTRL_XMIT_CMD   0x04
#define ADB_CTRL_RESET_ADB  0x08
#define ADB_CTRL_RESET_WD   0x10

/* ADB status register */
#define ADB_STAT_CONFLICT   0x01
#define ADB_STAT_REQUEST    0x02
#define ADB_STAT_TIMEOUT    0x04
#define ADB_STAT_DATAPEND   0x08
#define ADB_STAT_RESET      0x10
#define ADB_STAT_ACCESS     0x20
#define ADB_STAT_POLL_EN    0x40
#define ADB_STAT_POLL_OV    0x80

/* ADB command register */
#define ADB_CMD_REG_MASK    0x03
#define ADB_CMD_CMD_MASK    0x0C
#define ADB_CMD_ADDR_MASK   0xF0

/* ADB bit count register */
#define ADB_CNT_MASK        0x7F



static void adb_check_interrupt(void) {
	if (adb.intstatus&adb.intmask) {
		set_interrupt(INT_DISK, SET_INT);
	} else {
		set_interrupt(INT_DISK, RELEASE_INT);
	}
}

static void adb_interrupt(uint32_t intr) {
	if (adb.status&ADB_STAT_POLL_EN) {
		intr &= ~(ADB_INT_ACCESS);
	}
	adb.intstatus |= intr;
	adb_check_interrupt();
}

static void adb_timeout(void) {
	adb.status |= ADB_STAT_TIMEOUT;
	adb_interrupt(ADB_INT_ACCESS);
}

static uint32_t adb_read_data(uint8_t* data) {	
	data[0] = adb.data0 >> 24;
	data[1] = adb.data0 >> 16;
	data[2] = adb.data0 >> 8;
	data[3] = adb.data0;
	data[4] = adb.data1 >> 24;
	data[5] = adb.data1 >> 16;
	data[6] = adb.data1 >> 8;
	data[7] = adb.data1;
	
	return adb.bitcount >> 3;
}

static void adb_write_data(uint8_t* data, uint32_t len) {
	if (len > 8) {
		Log_Printf(LOG_WARN, "[ADB] Data: Lengh of data is too big (%d)", len);
		len = 8;
	}
	adb.data0 = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
	adb.data1 = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
	
	adb.bitcount = len << 3;
	adb.status |= ADB_STAT_DATAPEND;
	adb_interrupt(ADB_INT_ACCESS);
}

/* ADB commands */
#define ADB_CMD_RESET   0
#define ADB_CMD_FLUSH   1
#define ADB_CMD_LISTEN  2
#define ADB_CMD_TALK    3

static void adb_command(void) {
	uint8_t buf[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	
	uint8_t reg  = (adb.command & ADB_CMD_REG_MASK);
	uint8_t cmd  = (adb.command & ADB_CMD_CMD_MASK)  >> 2;
	uint8_t addr = (adb.command & ADB_CMD_ADDR_MASK) >> 4;
	
	if (!ConfigureParams.System.bADB) {
		Log_Printf(LOG_WARN, "[ADB] Command: No device present");
		adb_timeout();
		return;
	}
	
	Log_Printf(LOG_ADB_CMD_LEVEL, "[ADB] Command: %d, address: %d, register: %d", cmd, addr, reg);
	
	switch (cmd) {
		case ADB_CMD_LISTEN:
			Log_Printf(LOG_ADB_CMD_LEVEL, "[ADB] Command: Listen");
			if (addr == adb_kbd.addr) {
				adb_read_data(buf);
				switch (reg) {
					case 3:
						Log_Printf(LOG_WARN, "[ADB] Keyboard: Register 3 write (%02x%02x)", buf[0], buf[1]);
						break;
					default:
						Log_Printf(LOG_WARN, "[ADB] Keyboard: Unknown register write (%d)", reg);
						break;
				}
			} else if (addr == adb_mouse.addr) {
				adb_read_data(buf);
				switch (reg) {
					case 3:
						Log_Printf(LOG_WARN, "[ADB] Mouse: Register 3 write (%02x%02x)", buf[0], buf[1]);
						break;
					default:
						Log_Printf(LOG_WARN, "[ADB] Mouse: Unknown register write (%d)", reg);
						break;
				}
			} else {
				Log_Printf(LOG_WARN, "[ADB] Listen: Unknown device (%d)", addr);
				adb_timeout();
			}
			adb_interrupt(ADB_INT_ACCESS);
			break;
			
		case ADB_CMD_TALK:
			Log_Printf(LOG_ADB_CMD_LEVEL, "[ADB] Command: Talk");
			if (addr == adb_kbd.addr) {
				switch (reg) {
					case 0:
						if (adb_kbd_get(&buf[0])) {
							adb_kbd_get(&buf[1]);
							adb_write_data(buf, 2);
						}
						break;
					case 3:
						buf[0] = 0x60 | ((rand() % 15) + 1);
						buf[1] = ADB_TYPE_KBD;
						adb_write_data(buf, 2);
						break;
					default:
						Log_Printf(LOG_WARN, "[ADB] Keyboard: Unknown register read (%d)", reg);
						adb_timeout();
						break;
				}
			} else if (addr == adb_mouse.addr) {
				switch (reg) {
					case 0:
						if (adb_mouse_get(buf)) {
							adb_write_data(buf, 2);
						}
						break;
					case 3:
						buf[0] = 0x70 | ((rand() % 15) + 1);
						buf[1] = ADB_TYPE_MOUSE;
						adb_write_data(buf, 2);
						break;
					default:
						Log_Printf(LOG_WARN, "[ADB] Mouse: Unknown register read (%d)", reg);
						adb_timeout();
						break;
				}
			} else {
				Log_Printf(LOG_WARN, "[ADB] Talk: Unknown device (%d)", addr);
				adb_timeout();
			}
			break;
			
		case 1: /* reserved */
			Log_Printf(LOG_WARN, "[ADB] Command: Invalid");
			break;
			
		default:
			cmd = reg;
			switch (cmd) {
				case ADB_CMD_RESET:
					Log_Printf(LOG_WARN, "[ADB] Command: Reset");
					adb_kbd_reset();
					adb_mouse_reset();
					adb_timeout();
					break;
					
				case ADB_CMD_FLUSH:
					Log_Printf(LOG_WARN, "[ADB] Command: Flush");
					if (addr == adb_kbd.addr) {
						adb_kbd_flush();
					} else if (addr == adb_mouse.addr) {
						adb_mouse_flush();
					} else {
						Log_Printf(LOG_WARN, "[ADB] Flush: Unknown device (%d)", addr);
					}
					adb_timeout();
					break;
					
				default:
					Log_Printf(LOG_WARN, "[ADB] Command: Invalid");
					adb_timeout();
					break;
			}
			break;
	}
	
	if (adb_kbd_request() || adb_mouse_request()) {
		adb.status |= ADB_STAT_REQUEST;
		adb_interrupt(ADB_INT_ACCESS);
	}
}

static uint32_t adb_intstatus_read(uint32_t addr) {
	Log_Printf(LOG_ADB_LEVEL, "[ADB] Interrupt status read at $%08X val=$%08X",addr,adb.intstatus);
	return adb.intstatus;
}

static void adb_intstatus_write(uint32_t addr, uint32_t val) {
	Log_Printf(LOG_ADB_LEVEL, "[ADB] Interrupt status write at $%08X val=$%08X",addr,val);
	adb.intstatus &= ~val;
	adb_check_interrupt();
}

static uint32_t adb_intmask_read(uint32_t addr) {
	Log_Printf(LOG_ADB_LEVEL, "[ADB] Interrupt mask read at $%08X val=$%08X",addr,adb.intmask);
	return adb.intmask;
}

static void adb_intmask_write(uint32_t addr, uint32_t val) {
	Log_Printf(LOG_ADB_LEVEL, "[ADB] Interrupt mask write at $%08X val=$%08X",addr,val);
	adb.intmask = val;
	adb_check_interrupt();
}

static void adb_setint_write(uint32_t addr, uint32_t val) {
	Log_Printf(LOG_ADB_LEVEL, "[ADB] Set interrupt write at $%08X val=$%08X",addr,val);
	adb.intstatus |= val;
	adb_check_interrupt();
}

static uint32_t adb_config_read(uint32_t addr) {
	Log_Printf(LOG_ADB_LEVEL, "[ADB] Configuration read at $%08X val=$%08X",addr,adb.config);
	return adb.config;
}

static void adb_config_write(uint32_t addr, uint32_t val) {
	Log_Printf(LOG_ADB_LEVEL, "[ADB] Configuration write at $%08X val=$%08X",addr,val);
	adb.config = val;
}

static void adb_control_write(uint32_t addr, uint32_t val) {
	Log_Printf(LOG_ADB_LEVEL, "[ADB] Control write at $%08X val=$%08X",addr,val);
	
	if (val&ADB_CTRL_RESET_ADB) {
		Log_Printf(LOG_WARN, "[ADB] Software reset");
		adb_kbd_reset();
		adb_mouse_reset();
		adb.status &= ~ADB_STAT_POLL_EN;
		adb_interrupt(ADB_INT_RESET);
	}
	if (val&ADB_CTRL_XMIT_CMD) {
		if (adb.status&(ADB_STAT_RESET|ADB_STAT_POLL_EN|ADB_STAT_ACCESS)) {
			adb_interrupt(ADB_INT_REJECT);
		} else {
			adb.status &= ~(ADB_STAT_DATAPEND|ADB_STAT_TIMEOUT|ADB_STAT_REQUEST);
			adb_command();
		}
	}
	if (val&ADB_CTRL_DIS_POLL) {
		adb.status &= ~ADB_STAT_POLL_EN;
	}
	if (val&ADB_CTRL_EN_POLL) {
		if ((adb.status&(ADB_STAT_RESET|ADB_STAT_ACCESS)) && !(adb.status&ADB_STAT_POLL_EN)) {
			adb_interrupt(ADB_INT_REJECT);
		} else {
			/* broken in real hardware      *
			adb.status |= ADB_STAT_POLL_EN; */
		}
	}
}

static uint32_t adb_status_read(uint32_t addr) {
	Log_Printf(LOG_ADB_LEVEL, "[ADB] Status read at $%08X val=$%08X",addr,adb.status);
	return adb.status;
}

static uint32_t adb_command_read(uint32_t addr) {
	Log_Printf(LOG_ADB_LEVEL, "[ADB] Command read at $%08X val=$%08X",addr,adb.command);
	return adb.command;
}

static void adb_command_write(uint32_t addr, uint32_t val) {
	Log_Printf(LOG_ADB_LEVEL, "[ADB] Command write at $%08X val=$%08X",addr,val);
	adb.command = val;
}

static uint32_t adb_bitcount_read(uint32_t addr) {
	Log_Printf(LOG_ADB_LEVEL, "[ADB] Bitcount read at $%08X val=$%08X",addr,adb.bitcount);
	return adb.bitcount & ADB_CNT_MASK;
}

static void adb_bitcount_write(uint32_t addr, uint32_t val) {
	Log_Printf(LOG_ADB_LEVEL, "[ADB] Bitcount write at $%08X val=$%08X",addr,val);
	adb.bitcount = val & ADB_CNT_MASK;
}

static uint32_t adb_data0_read(uint32_t addr) {
	Log_Printf(LOG_ADB_LEVEL, "[ADB] Data0 read at $%08X val=$%08X",addr,adb.data0);
	return adb.data0;
}

static void adb_data0_write(uint32_t addr, uint32_t val) {
	Log_Printf(LOG_ADB_LEVEL, "[ADB] Data0 write at $%08X val=$%08X",addr,val);
	adb.data0 = val;
}

static uint32_t adb_data1_read(uint32_t addr) {
	Log_Printf(LOG_ADB_LEVEL, "[ADB] Data1 read at $%08X val=$%08X",addr,adb.data1);
	return adb.data1;
}

static void adb_data1_write(uint32_t addr, uint32_t val) {
	Log_Printf(LOG_ADB_LEVEL, "[ADB] Data1 write at $%08X val=$%08X",addr,val);
	adb.data1 = val;
}


static uint32_t adb_read_register(uint32_t addr, int size) {
	switch (addr&0xFF) {
		case ADB_INTSTATUS:
			return adb_intstatus_read(addr);
		case ADB_INTMASK:
			return adb_intmask_read(addr);
		case ADB_CONFIG:
			return adb_config_read(addr);
		case ADB_STATUS:
			return adb_status_read(addr);
		case ADB_CMD:
			return adb_command_read(addr);
		case ADB_COUNT:
			return adb_bitcount_read(addr);
		case ADB_DATA0:
			return adb_data0_read(addr);
		case ADB_DATA1:
			return adb_data1_read(addr);
			
		default:
			Log_Printf(LOG_WARN, "[ADB] Illegal read at $%08X",addr);
			M68000_BusError(addr, BUS_ERROR_READ, size, BUS_ERROR_ACCESS_DATA, 0);
			return 0;
	}
}

static void adb_write_register(uint32_t addr, uint32_t val, int size) {
	switch (addr&0xFF) {
		case ADB_INTSTATUS:
			adb_intstatus_write(addr, val);
			break;
		case ADB_INTMASK:
			adb_intmask_write(addr, val);
			break;
		case ADB_SETINT:
			adb_setint_write(addr, val);
			break;
		case ADB_CONFIG:
			adb_config_write(addr, val);
			break;
		case ADB_CTRL:
			adb_control_write(addr, val);
			break;
		case ADB_CMD:
			adb_command_write(addr, val);
			break;
		case ADB_COUNT:
			adb_bitcount_write(addr, val);
			break;
		case ADB_DATA0:
			adb_data0_write(addr, val);
			break;
		case ADB_DATA1:
			adb_data1_write(addr, val);
			break;
			
		default:
			Log_Printf(LOG_WARN, "[ADB] Illegal write at $%08X",addr);
			M68000_BusError(addr, BUS_ERROR_WRITE, size, BUS_ERROR_ACCESS_DATA, val);
			break;
	}
}



uint32_t adb_lget(uint32_t addr) {
	Log_Printf(LOG_DEBUG, "[ADB] lget at $%08X",addr);
	return adb_read_register(addr, BUS_ERROR_SIZE_LONG);
}

uint16_t adb_wget(uint32_t addr) {
	uint8_t shift;
	Log_Printf(LOG_WARN, "[ADB] wget at $%08X",addr);
	
	shift = (2-(addr&2))*8;
	addr &= ~3;
	return (adb_read_register(addr, BUS_ERROR_SIZE_WORD)>>shift)&0xFFFF;
}

uint8_t adb_bget(uint32_t addr) {
	uint8_t shift;
	Log_Printf(LOG_WARN, "[ADB] bget at $%08X",addr);
	
	shift = (3-(addr&3))*8;
	addr &= ~3;
	return (adb_read_register(addr, BUS_ERROR_SIZE_BYTE)>>shift)&0xFF;
}

void adb_lput(uint32_t addr, uint32_t l) {
	Log_Printf(LOG_DEBUG, "[ADB] lput at $%08X",addr);
	adb_write_register(addr, l, BUS_ERROR_SIZE_LONG);
}

void adb_wput(uint32_t addr, uint16_t w) {
	Log_Printf(LOG_WARN, "[ADB] illegal wput at $%08X -> bus error",addr);
	M68000_BusError(addr, BUS_ERROR_WRITE, BUS_ERROR_SIZE_WORD, BUS_ERROR_ACCESS_DATA, w);
}

void adb_bput(uint32_t addr, uint8_t b) {
	Log_Printf(LOG_WARN, "[ADB] illegal bput at $%08X -> bus error",addr);
	M68000_BusError(addr, BUS_ERROR_WRITE, BUS_ERROR_SIZE_BYTE, BUS_ERROR_ACCESS_DATA, b);
}

void ADB_Reset(void) {
	Log_Printf(LOG_WARN, "[ADB] Reset");
	adb.intstatus = 0;
	adb.intmask = 0;
	adb.config = 0;
	adb.command = 0;
	adb.status = 0;
	adb.bitcount = 0;
	adb.data0 = 0;
	adb.data1 = 0;
	
	adb_kbd_reset();
	adb_mouse_reset();
}


/* ADB host input conversion */

#define APPLEKEY_a               0x00
#define APPLEKEY_s               0x01
#define APPLEKEY_d               0x02
#define APPLEKEY_f               0x03
#define APPLEKEY_h               0x04
#define APPLEKEY_g               0x05
#define APPLEKEY_z               0x06
#define APPLEKEY_x               0x07
#define APPLEKEY_c               0x08
#define APPLEKEY_v               0x09
#define APPLEKEY_LESS            0x0a
#define APPLEKEY_b               0x0b
#define APPLEKEY_q               0x0c
#define APPLEKEY_w               0x0d
#define APPLEKEY_e               0x0e
#define APPLEKEY_r               0x0f
#define APPLEKEY_y               0x10
#define APPLEKEY_t               0x11
#define APPLEKEY_1               0x12
#define APPLEKEY_2               0x13
#define APPLEKEY_3               0x14
#define APPLEKEY_4               0x15
#define APPLEKEY_6               0x16
#define APPLEKEY_5               0x17
#define APPLEKEY_EQUALS          0x18
#define APPLEKEY_9               0x19
#define APPLEKEY_7               0x1a
#define APPLEKEY_MINUS           0x1b
#define APPLEKEY_8               0x1c
#define APPLEKEY_0               0x1d
#define APPLEKEY_CLOSEBRACKET    0x1e
#define APPLEKEY_o               0x1f
#define APPLEKEY_u               0x20
#define APPLEKEY_OPENBRACKET     0x21
#define APPLEKEY_i               0x22
#define APPLEKEY_p               0x23
#define APPLEKEY_RETURN          0x24
#define APPLEKEY_l               0x25
#define APPLEKEY_j               0x26
#define APPLEKEY_QUOTE           0x27
#define APPLEKEY_k               0x28
#define APPLEKEY_SEMICOLON       0x29
#define APPLEKEY_BACKSLASH       0x2a
#define APPLEKEY_COMMA           0x2b
#define APPLEKEY_SLASH           0x2c
#define APPLEKEY_n               0x2d
#define APPLEKEY_m               0x2e
#define APPLEKEY_PERIOD          0x2f
#define APPLEKEY_TAB             0x30
#define APPLEKEY_SPACE           0x31
#define APPLEKEY_BACKQUOTE       0x32
#define APPLEKEY_DELETE          0x33
#define APPLEKEY_ESC             0x35
#define APPLEKEY_CTL_LEFT        0x36
#define APPLEKEY_APPLE_LEFT      0x37
#define APPLEKEY_SHIFT_LEFT      0x38
#define APPLEKEY_CAPS_LOCK       0x39
#define APPLEKEY_OPTION_LEFT     0x3a
#define APPLEKEY_LEFT_ARROW      0x3b
#define APPLEKEY_RIGHT_ARROW     0x3c
#define APPLEKEY_DOWN_ARROW      0x3d
#define APPLEKEY_UP_ARROW        0x3e
#define APPLEKEY_KEYPAD_PERIOD   0x41
#define APPLEKEY_KEYPAD_MULTIPLY 0x43
#define APPLEKEY_KEYPAD_PLUS     0x45
#define APPLEKEY_KEYPAD_DIVIDE   0x4b
#define APPLEKEY_KEYPAD_ENTER    0x4c
#define APPLEKEY_KEYPAD_MINUS    0x4e
#define APPLEKEY_KEYPAD_EQUALS   0x51
#define APPLEKEY_KEYPAD_0        0x52
#define APPLEKEY_KEYPAD_1        0x53
#define APPLEKEY_KEYPAD_2        0x54
#define APPLEKEY_KEYPAD_3        0x55
#define APPLEKEY_KEYPAD_4        0x56
#define APPLEKEY_KEYPAD_5        0x57
#define APPLEKEY_KEYPAD_6        0x58
#define APPLEKEY_KEYPAD_7        0x59
#define APPLEKEY_KEYPAD_8        0x5b
#define APPLEKEY_KEYPAD_9        0x5c
#define APPLEKEY_HELP            0x72
#define APPLEKEY_VOLUME_UP       0x73
#define APPLEKEY_BRIGHTNESS_UP   0x74
#define APPLEKEY_VOLUME_DOWN     0x77
#define APPLEKEY_BRIGHTNESS_DOWN 0x79
#define APPLEKEY_SHIFT_RIGHT     0x7b
#define APPLEKEY_OPTION_RIGHT    0x7c
#define APPLEKEY_POWER           0x7f


static uint8_t ADB_GetKeyFromScancode(SDL_Scancode sdlscancode)
{
	Log_Printf(LOG_ADB_LEVEL, "[ADB] Scancode: %i (%s)\n", sdlscancode, SDL_GetScancodeName(sdlscancode));
	
	switch (sdlscancode) {
		case SDL_SCANCODE_ESCAPE:
		case SDL_SCANCODE_GRAVE:          return APPLEKEY_ESC;
		case SDL_SCANCODE_1:              return APPLEKEY_1;
		case SDL_SCANCODE_2:              return APPLEKEY_2;
		case SDL_SCANCODE_3:              return APPLEKEY_3;
		case SDL_SCANCODE_4:              return APPLEKEY_4;
		case SDL_SCANCODE_5:              return APPLEKEY_5;
		case SDL_SCANCODE_6:              return APPLEKEY_6;
		case SDL_SCANCODE_7:              return APPLEKEY_7;
		case SDL_SCANCODE_8:              return APPLEKEY_8;
		case SDL_SCANCODE_9:              return APPLEKEY_9;
		case SDL_SCANCODE_0:              return APPLEKEY_0;
		case SDL_SCANCODE_MINUS:          return APPLEKEY_MINUS;
		case SDL_SCANCODE_EQUALS:         return APPLEKEY_EQUALS;
		case SDL_SCANCODE_BACKSPACE:      return APPLEKEY_DELETE;
			
		case SDL_SCANCODE_TAB:            return APPLEKEY_TAB;
		case SDL_SCANCODE_Q:              return APPLEKEY_q;
		case SDL_SCANCODE_W:              return APPLEKEY_w;
		case SDL_SCANCODE_E:              return APPLEKEY_e;
		case SDL_SCANCODE_R:              return APPLEKEY_r;
		case SDL_SCANCODE_T:              return APPLEKEY_t;
		case SDL_SCANCODE_Y:              return APPLEKEY_y;
		case SDL_SCANCODE_U:              return APPLEKEY_u;
		case SDL_SCANCODE_I:              return APPLEKEY_i;
		case SDL_SCANCODE_O:              return APPLEKEY_o;
		case SDL_SCANCODE_P:              return APPLEKEY_p;
		case SDL_SCANCODE_LEFTBRACKET:    return APPLEKEY_OPENBRACKET;
		case SDL_SCANCODE_RIGHTBRACKET:   return APPLEKEY_CLOSEBRACKET;
		case SDL_SCANCODE_BACKSLASH:      return APPLEKEY_BACKSLASH;
			
		case SDL_SCANCODE_A:              return APPLEKEY_a;
		case SDL_SCANCODE_S:              return APPLEKEY_s;
		case SDL_SCANCODE_D:              return APPLEKEY_d;
		case SDL_SCANCODE_F:              return APPLEKEY_f;
		case SDL_SCANCODE_G:              return APPLEKEY_g;
		case SDL_SCANCODE_H:              return APPLEKEY_h;
		case SDL_SCANCODE_J:              return APPLEKEY_j;
		case SDL_SCANCODE_K:              return APPLEKEY_k;
		case SDL_SCANCODE_L:              return APPLEKEY_l;
		case SDL_SCANCODE_SEMICOLON:      return APPLEKEY_SEMICOLON;
		case SDL_SCANCODE_APOSTROPHE:     return APPLEKEY_QUOTE;
		case SDL_SCANCODE_RETURN:         return APPLEKEY_RETURN;
			
		case SDL_SCANCODE_NONUSBACKSLASH: return APPLEKEY_BACKQUOTE;
		case SDL_SCANCODE_Z:              return APPLEKEY_z;
		case SDL_SCANCODE_X:              return APPLEKEY_x;
		case SDL_SCANCODE_C:              return APPLEKEY_c;
		case SDL_SCANCODE_V:              return APPLEKEY_v;
		case SDL_SCANCODE_B:              return APPLEKEY_b;
		case SDL_SCANCODE_N:              return APPLEKEY_n;
		case SDL_SCANCODE_M:              return APPLEKEY_m;
		case SDL_SCANCODE_COMMA:          return APPLEKEY_COMMA;
		case SDL_SCANCODE_PERIOD:         return APPLEKEY_PERIOD;
		case SDL_SCANCODE_SLASH:          return APPLEKEY_SLASH;
		case SDL_SCANCODE_SPACE:          return APPLEKEY_SPACE;
			
		case SDL_SCANCODE_NUMLOCKCLEAR:   return APPLEKEY_BACKQUOTE;
		case SDL_SCANCODE_KP_EQUALS:      return APPLEKEY_KEYPAD_EQUALS;
		case SDL_SCANCODE_KP_DIVIDE:      return APPLEKEY_KEYPAD_DIVIDE;
		case SDL_SCANCODE_KP_MULTIPLY:    return APPLEKEY_KEYPAD_MULTIPLY;
		case SDL_SCANCODE_KP_7:           return APPLEKEY_KEYPAD_7;
		case SDL_SCANCODE_KP_8:           return APPLEKEY_KEYPAD_8;
		case SDL_SCANCODE_KP_9:           return APPLEKEY_KEYPAD_9;
		case SDL_SCANCODE_KP_MINUS:       return APPLEKEY_KEYPAD_MINUS;
		case SDL_SCANCODE_KP_4:           return APPLEKEY_KEYPAD_4;
		case SDL_SCANCODE_KP_5:           return APPLEKEY_KEYPAD_5;
		case SDL_SCANCODE_KP_6:           return APPLEKEY_KEYPAD_6;
		case SDL_SCANCODE_KP_PLUS:        return APPLEKEY_KEYPAD_PLUS;
		case SDL_SCANCODE_KP_1:           return APPLEKEY_KEYPAD_1;
		case SDL_SCANCODE_KP_2:           return APPLEKEY_KEYPAD_2;
		case SDL_SCANCODE_KP_3:           return APPLEKEY_KEYPAD_3;
		case SDL_SCANCODE_KP_0:           return APPLEKEY_KEYPAD_0;
		case SDL_SCANCODE_KP_PERIOD:      return APPLEKEY_KEYPAD_PERIOD;
		case SDL_SCANCODE_KP_ENTER:       return APPLEKEY_KEYPAD_ENTER;
		
		case SDL_SCANCODE_LEFT:           return APPLEKEY_LEFT_ARROW;
		case SDL_SCANCODE_RIGHT:          return APPLEKEY_RIGHT_ARROW;
		case SDL_SCANCODE_UP:             return APPLEKEY_UP_ARROW;
		case SDL_SCANCODE_DOWN:           return APPLEKEY_DOWN_ARROW;
		
		/* Modifier keys */
		case SDL_SCANCODE_RSHIFT:         return APPLEKEY_SHIFT_RIGHT;
		case SDL_SCANCODE_LSHIFT:         return APPLEKEY_SHIFT_LEFT;
		case SDL_SCANCODE_RGUI:           return APPLEKEY_APPLE_LEFT;
		case SDL_SCANCODE_LGUI:           return APPLEKEY_APPLE_LEFT;
		case SDL_SCANCODE_RCTRL:          return APPLEKEY_HELP;
		case SDL_SCANCODE_LCTRL:          return APPLEKEY_CTL_LEFT;
		case SDL_SCANCODE_RALT:           return APPLEKEY_OPTION_RIGHT;
		case SDL_SCANCODE_LALT:           return APPLEKEY_OPTION_LEFT;
		case SDL_SCANCODE_CAPSLOCK:       return APPLEKEY_CAPS_LOCK;
		
		/* Special keys */
		case SDL_SCANCODE_F10:
		case SDL_SCANCODE_DELETE:         return APPLEKEY_POWER;
		case SDL_SCANCODE_F5:
		case SDL_SCANCODE_END:            return APPLEKEY_VOLUME_DOWN;
		case SDL_SCANCODE_F6:
		case SDL_SCANCODE_HOME:           return APPLEKEY_VOLUME_UP;
		case SDL_SCANCODE_F1:
		case SDL_SCANCODE_PAGEDOWN:       return APPLEKEY_BRIGHTNESS_DOWN;
		case SDL_SCANCODE_F2:
		case SDL_SCANCODE_PAGEUP:         return APPLEKEY_BRIGHTNESS_UP;
			
		default:                          return 0xff;
	}
}

static uint8_t ADB_GetKeyFromSymbol(SDL_Keycode sdlkey)
{
	Log_Printf(LOG_ADB_LEVEL, "[ADB] Symkey: %s\n", SDL_GetKeyName(sdlkey));
	
	switch (sdlkey) {
		case SDLK_BACKSLASH:              return APPLEKEY_BACKSLASH;
		case SDLK_RIGHTBRACKET:           return APPLEKEY_CLOSEBRACKET;
		case SDLK_LEFTBRACKET:            return APPLEKEY_OPENBRACKET;
		case SDLK_i:                      return APPLEKEY_i;
		case SDLK_o:                      return APPLEKEY_o;
		case SDLK_p:                      return APPLEKEY_p;
		case SDLK_LEFT:                   return APPLEKEY_LEFT_ARROW;
		case SDLK_KP_0:                   return APPLEKEY_KEYPAD_0;
		case SDLK_KP_PERIOD:              return APPLEKEY_KEYPAD_PERIOD;
		case SDLK_KP_ENTER:               return APPLEKEY_KEYPAD_ENTER;
		case SDLK_DOWN:                   return APPLEKEY_DOWN_ARROW;
		case SDLK_RIGHT:                  return APPLEKEY_RIGHT_ARROW;
		case SDLK_KP_1:                   return APPLEKEY_KEYPAD_1;
		case SDLK_KP_4:                   return APPLEKEY_KEYPAD_4;
		case SDLK_KP_6:                   return APPLEKEY_KEYPAD_6;
		case SDLK_KP_3:                   return APPLEKEY_KEYPAD_3;
		case SDLK_KP_PLUS:                return APPLEKEY_KEYPAD_PLUS;
		case SDLK_UP:                     return APPLEKEY_UP_ARROW;
		case SDLK_KP_2:                   return APPLEKEY_KEYPAD_2;
		case SDLK_KP_5:                   return APPLEKEY_KEYPAD_5;
		case SDLK_BACKSPACE:              return APPLEKEY_DELETE;
		case SDLK_EQUALS:                 return APPLEKEY_EQUALS;
		case SDLK_MINUS:                  return APPLEKEY_MINUS;
		case SDLK_8:                      return APPLEKEY_8;
		case SDLK_9:                      return APPLEKEY_9;
		case SDLK_0:                      return APPLEKEY_0;
		case SDLK_KP_7:                   return APPLEKEY_KEYPAD_7;
		case SDLK_KP_8:                   return APPLEKEY_KEYPAD_8;
		case SDLK_KP_9:                   return APPLEKEY_KEYPAD_9;
		case SDLK_KP_MINUS:               return APPLEKEY_KEYPAD_MINUS;
		case SDLK_KP_MULTIPLY:            return APPLEKEY_KEYPAD_MULTIPLY;
		case SDLK_BACKQUOTE:              return APPLEKEY_BACKQUOTE;
		case SDLK_KP_EQUALS:              return APPLEKEY_KEYPAD_EQUALS;
		case SDLK_KP_DIVIDE:              return APPLEKEY_KEYPAD_DIVIDE;
		case SDLK_RETURN:                 return APPLEKEY_RETURN;
		case SDLK_QUOTE:                  return APPLEKEY_QUOTE;
		case SDLK_SEMICOLON:              return APPLEKEY_SEMICOLON;
		case SDLK_l:                      return APPLEKEY_l;
		case SDLK_COMMA:                  return APPLEKEY_COMMA;
		case SDLK_PERIOD:                 return APPLEKEY_PERIOD;
		case SDLK_SLASH:                  return APPLEKEY_SLASH;
		case SDLK_z:                      return APPLEKEY_z;
		case SDLK_x:                      return APPLEKEY_x;
		case SDLK_c:                      return APPLEKEY_c;
		case SDLK_v:                      return APPLEKEY_v;
		case SDLK_b:                      return APPLEKEY_b;
		case SDLK_m:                      return APPLEKEY_m;
		case SDLK_n:                      return APPLEKEY_n;
		case SDLK_SPACE:                  return APPLEKEY_SPACE;
		case SDLK_a:                      return APPLEKEY_a;
		case SDLK_s:                      return APPLEKEY_s;
		case SDLK_d:                      return APPLEKEY_d;
		case SDLK_f:                      return APPLEKEY_f;
		case SDLK_g:                      return APPLEKEY_g;
		case SDLK_k:                      return APPLEKEY_k;
		case SDLK_j:                      return APPLEKEY_j;
		case SDLK_h:                      return APPLEKEY_h;
		case SDLK_TAB:                    return APPLEKEY_TAB;
		case SDLK_q:                      return APPLEKEY_q;
		case SDLK_w:                      return APPLEKEY_w;
		case SDLK_e:                      return APPLEKEY_e;
		case SDLK_r:                      return APPLEKEY_r;
		case SDLK_u:                      return APPLEKEY_u;
		case SDLK_y:                      return APPLEKEY_y;
		case SDLK_t:                      return APPLEKEY_t;
		case SDLK_ESCAPE:                 return APPLEKEY_ESC;
		case SDLK_1:                      return APPLEKEY_1;
		case SDLK_2:                      return APPLEKEY_2;
		case SDLK_3:                      return APPLEKEY_3;
		case SDLK_4:                      return APPLEKEY_4;
		case SDLK_7:                      return APPLEKEY_7;
		case SDLK_6:                      return APPLEKEY_6;
		case SDLK_5:                      return APPLEKEY_5;
			
		/* Modifier keys */
		case SDLK_RSHIFT:                 return APPLEKEY_SHIFT_RIGHT;
		case SDLK_LSHIFT:                 return APPLEKEY_SHIFT_LEFT;
		case SDLK_RGUI:                   return APPLEKEY_APPLE_LEFT;
		case SDLK_LGUI:                   return APPLEKEY_APPLE_LEFT;
		case SDLK_RCTRL:                  return APPLEKEY_HELP;
		case SDLK_LCTRL:                  return APPLEKEY_CTL_LEFT;
		case SDLK_RALT:                   return APPLEKEY_OPTION_RIGHT;
		case SDLK_LALT:                   return APPLEKEY_OPTION_LEFT;
		case SDLK_CAPSLOCK:               return APPLEKEY_CAPS_LOCK;

		/* Special Keys */
		case SDLK_F10:
		case SDLK_DELETE:                 return APPLEKEY_POWER;
		case SDLK_F5:
		case SDLK_END:                    return APPLEKEY_VOLUME_DOWN;
		case SDLK_F6:
		case SDLK_HOME:                   return APPLEKEY_VOLUME_UP;
		case SDLK_F1:
		case SDLK_PAGEDOWN:               return APPLEKEY_BRIGHTNESS_DOWN;
		case SDLK_F2:
		case SDLK_PAGEUP:                 return APPLEKEY_BRIGHTNESS_UP;
			
		default:                          return 0xff;
	}
}


void ADB_KeyDown(const SDL_Keysym *sdlkey)
{
	uint8_t adb_key;
	
	if (ConfigureParams.Keyboard.nKeymapType == KEYMAP_SYMBOLIC) {
		adb_key = ADB_GetKeyFromSymbol(sdlkey->sym);
	} else {
		adb_key = ADB_GetKeyFromScancode(sdlkey->scancode);
	}
		
	Log_Printf(LOG_ADB_LEVEL, "[ADB] Keycode: $%02x", adb_key);
	
	adb_keydown(adb_key);
}

void ADB_KeyUp(const SDL_Keysym *sdlkey)
{
	uint8_t adb_key;
	
	if (ConfigureParams.Keyboard.nKeymapType == KEYMAP_SYMBOLIC) {
		adb_key = ADB_GetKeyFromSymbol(sdlkey->sym);
	} else {
		adb_key = ADB_GetKeyFromScancode(sdlkey->scancode);
	}
		
	Log_Printf(LOG_ADB_LEVEL, "[ADB] Keycode: $%02x", adb_key);
	
	adb_keyup(adb_key);
}

void ADB_MouseMove(int x, int y) {
	if (x == 0 && y == 0)
		return;
	
	if      (x < -8) x = -8;
	else if (x >  8) x =  8;
	if      (y < -8) y = -8;
	else if (y >  8) y =  8;
	
	adb_mouse_move(x, y);
}

void ADB_MouseButton(bool left, bool down) {
	adb_mouse_button(left, down);
}
