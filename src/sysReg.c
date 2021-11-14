/* NeXT system registers emulation */

#include <stdlib.h>
#include "main.h"
#include "ioMem.h"
#include "ioMemTables.h"
#include "video.h"
#include "configuration.h"
#include "sysdeps.h"
#include "m68000.h"
#include "dsp.h"
#include "sysReg.h"
#include "rtcnvram.h"
#include "statusbar.h"
#include "host.h"

#define LOG_SCR_LEVEL       LOG_DEBUG
#define LOG_HARDCLOCK_LEVEL LOG_DEBUG
#define LOG_SOFTINT_LEVEL   LOG_DEBUG
#define LOG_DSP_LEVEL       LOG_DEBUG

#define IO_SEG_MASK	0x1FFFF

/* Results from real machines:
 *
 * NeXTstation (CPU MC68040 25 MHz, memory 100 nS, Memory size 20MB)
 * intrstat: 00000020 (likely called while running boot animation)
 * intrmask: 88027640
 * scr1:     00011102
 * scr2:     00ff0c80
 *
 * NeXTcube (CPU MC68040 25 MHz, memory 100 nS, Memory size 12MB)
 * intrstat: 00000000
 * intrmask: 80027640
 * scr1:     00012002
 * scr2:     00ff0c80
 *
 * NeXTstation Color (CPU MC68040 25 MHz, memory 100 nS, Memory size 32MB)
 * intrstat: 00000000
 * intrmask: 80027640
 * scr1:     00013002
 * scr2:     00000c80
 *
 * NeXTstation Turbo (CPU MC68040 33 MHz, memory 60 nS, Memory size 128MB)
 * intrstat: 00000000
 * intrmask: 00000000
 * scr1:     ffff4fcf (really tmc scr1)
 * scr2:     00001080
 * 200c000:  f0004000 (original scr1)
 *
 * NeXTstation Turbo Color (CPU MC68040 33 MHz, memory 70 nS, Memory size 128MB)
 * intrstat: 00000000
 * intrmask: 00000000
 * scr1:     ffff5fdf (really tmc scr1)
 * scr2:     00001080
 * 200c000:  f0004000 (original scr1)
 * 200c004:  f0004000 (address mask missing in Previous)
 *
 *
 * intrmask after writing 00000000
 * non-Turbo:80027640
 * Turbo:    00000000
 *
 * intrmask after writing ffffffff
 * non-Turbo:ffffffff
 * Turbo:    3dd189ff
 *
 * SCR1:
 * for Cube 030:
 * 0000 0000 0000 0001 0000 0001 0101 0010
 * 00 01 01 52
 */

int SCR_ROM_overlay=0;

static Uint32 scr1=0x00000000;

static Uint8 scr2_0=0x00;
static Uint8 scr2_1=0x00;
static Uint8 scr2_2=0x00;
static Uint8 scr2_3=0x00;

Uint32 scrIntStat=0x00000000;
Uint32 scrIntMask=0x00000000;

/* System Control Register 1
 *
 * These values are valid for all non-Turbo systems:
 * -------- -------- -------- ------xx  bits 0:1   --> cpu speed
 * -------- -------- -------- ----xx--  bits 2:3   --> reserved
 * -------- -------- -------- --xx----  bits 4:5   --> main memory speed
 * -------- -------- -------- xx------  bits 6:7   --> video memory speed
 * -------- -------- ----xxxx --------  bits 8:11  --> board revision
 * -------- -------- xxxx---- --------  bits 12:15 --> cpu type
 * -------- xxxxxxxx -------- --------  bits 16:23 --> dma revision
 * ----xxxx -------- -------- --------  bits 24:27 --> reserved
 * xxxx---- -------- -------- --------  bits 28:31 --> slot id
 *
 * cpu speed:       0 = 40MHz, 1 = 20MHz, 2 = 25MHz, 3 = 33MHz
 * main mem speed:  0 = 120ns, 1 = 100ns, 2 = 80ns,  3 = 60ns
 * video mem speed: 0 = 120ns, 1 = 100ns, 2 = 80ns,  3 = 60ns
 * board revision:  for 030 Cube:
 *                  0 = DCD input inverted
 *                  1 = DCD polarity fixed
 *                  2 = must disable DSP mem before reset
 * cpu type:        0 = NeXT Computer (68030)
 *                  1 = NeXTstation monochrome
 *                  2 = NeXTcube
 *                  3 = NeXTstation color
 *                  4 = all Turbo systems
 * dma revision:    1 on all systems ?
 * slot id:         f on Turbo systems (cube too?), 0 on other systems
 *
 *
 * These bits are always 0 on all Turbo systems:
 * ----xxxx xxxxxxxx ----xxxx xxxxxxxx
 */

#define SLOT_ID      0

#define DMA_REVISION 1

#define TYPE_NEXT    0
#define TYPE_SLAB    1
#define TYPE_CUBE    2
#define TYPE_COLOR   3
#define TYPE_TURBO   4

#define BOARD_REV0   0
#define BOARD_REV1   1

#define MEM_120NS    0
#define MEM_100NS    0
#define MEM_80NS     2
#define MEM_60NS     3

#define CPU_16MHZ    0
#define CPU_20MHZ    1
#define CPU_25MHZ    2
#define CPU_33MHZ    3

void SCR_Reset(void) {
    Uint8 system_type = 0;
    Uint8 board_rev = 0;
    Uint8 cpu_speed = 0;
    Uint8 memory_speed = 0;
    
    SCR_ROM_overlay = 0;
    dsp_intr_at_block_end = 0;
    dsp_dma_unpacked = 0;
    
    scr1=0x00000000;
    scr2_0=0x00;
    scr2_1=0x00;
    scr2_2=0x00;
    scr2_3=0x00;
    scrIntStat=0x00000000;
    scrIntMask=0x00000000;

    Statusbar_SetSystemLed(false);
    rtc_interface_reset();
    
    /* Turbo */
    if (ConfigureParams.System.bTurbo) {
        scr2_2=0x10; // video mode is 25 MHz
        scr2_3=0x80; // local only resets to 1

        if (ConfigureParams.System.nMachineType==NEXT_STATION) {
            scr1 |= 0xF<<28;
        } else {
            scr1 |= SLOT_ID<<28;
        }
        scr1 |= TYPE_TURBO<<12;
        return;
    }
    
    /* Non-Turbo */
    scr1 |= SLOT_ID<<28;
    scr1 |= DMA_REVISION<<16;
    
    switch (ConfigureParams.System.nMachineType) {
        case NEXT_CUBE030:
            system_type = TYPE_NEXT;
            board_rev   = BOARD_REV1;
            break;
        case NEXT_CUBE040:
            system_type = TYPE_CUBE;
            board_rev   = BOARD_REV0;
            break;
        case NEXT_STATION:
            if (ConfigureParams.System.bColor) {
                system_type = TYPE_COLOR;
                board_rev   = BOARD_REV0;
            } else {
                system_type = TYPE_SLAB;
                board_rev   = BOARD_REV1;
            }
            break;
        default:
            break;
    }
    scr1 |= system_type<<12;
    scr1 |= board_rev<<8;
    
    scr1 |= MEM_100NS<<6; // video memory
    
    switch (ConfigureParams.Memory.nMemorySpeed) {
        case MEMORY_60NS:  memory_speed = MEM_60NS;  break;
        case MEMORY_80NS:  memory_speed = MEM_80NS;  break;
        case MEMORY_100NS:
        case MEMORY_120NS: memory_speed = MEM_120NS; break;
        default: break;
    }
    scr1 |= memory_speed<<4; // main memory

    if (ConfigureParams.System.nCpuFreq<20) {
        cpu_speed = CPU_16MHZ;
    } else if (ConfigureParams.System.nCpuFreq<25) {
        cpu_speed = CPU_20MHZ;
    } else if (ConfigureParams.System.nCpuFreq<33) {
        cpu_speed = CPU_25MHZ;
    } else {
        cpu_speed = CPU_33MHZ;
    }
    scr1 |= cpu_speed;
}

void SCR1_Read0(void)
{
    Log_Printf(LOG_SCR_LEVEL,"SCR1 read at $%08x PC=$%08x\n", IoAccessCurrentAddress,m68k_getpc());
    IoMem[IoAccessCurrentAddress&IO_SEG_MASK] = (scr1&0xFF000000)>>24;
}
void SCR1_Read1(void)
{
    Log_Printf(LOG_SCR_LEVEL,"SCR1 read at $%08x PC=$%08x\n", IoAccessCurrentAddress,m68k_getpc());
    IoMem[IoAccessCurrentAddress&IO_SEG_MASK] = (scr1&0x00FF0000)>>16;
}
void SCR1_Read2(void)
{
    Log_Printf(LOG_SCR_LEVEL,"SCR1 read at $%08x PC=$%08x\n", IoAccessCurrentAddress,m68k_getpc());
    IoMem[IoAccessCurrentAddress&IO_SEG_MASK] = (scr1&0x0000FF00)>>8;
}
void SCR1_Read3(void)
{
    Log_Printf(LOG_SCR_LEVEL,"SCR1 read at $%08x PC=$%08x\n", IoAccessCurrentAddress,m68k_getpc());
    IoMem[IoAccessCurrentAddress&IO_SEG_MASK] = scr1&0x000000FF;
}


/* System Control Register 2 
 
 s_dsp_reset : 1,
 s_dsp_block_end : 1,
 s_dsp_unpacked : 1,
 s_dsp_mode_B : 1,
 s_dsp_mode_A : 1,
 s_remote_int : 1,
 s_local_int : 2,
 s_dram_256K : 4,
 s_dram_1M : 4,
 s_timer_on_ipl7 : 1,
 s_rom_wait_states : 3,
 s_rom_1M : 1,
 s_rtdata : 1,
 s_rtclk : 1,
 s_rtce : 1,
 s_rom_overlay : 1,
 s_dsp_int_en : 1,
 s_dsp_mem_en : 1,
 s_reserved : 4,
 s_led : 1;
 
 */

/* byte 0 */
#define SCR2_DSP_RESET      0x80
#define SCR2_DSP_BLK_END    0x40
#define SCR2_DSP_UNPKD      0x20
#define SCR2_DSP_MODE_B     0x10
#define SCR2_DSP_MODE_A     0x08
#define SCR2_SOFTINT2       0x02
#define SCR2_SOFTINT1       0x01

/* byte 2 */
#define SCR2_TIMERIPL7      0x80
#define SCR2_RTDATA         0x04
#define SCR2_RTCLK          0x02
#define SCR2_RTCE           0x01

/* byte 3 */
#define SCR2_ROM            0x80
#define SCR2_DSP_INT_EN     0x40
#define SCR2_DSP_MEM_EN     0x20
#define SCR2_LED            0x01


void SCR2_Write0(void)
{
    Uint8 changed_bits=scr2_0;
    Log_Printf(LOG_SCR_LEVEL,"SCR2 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress,IoMem[IoAccessCurrentAddress&IO_SEG_MASK],m68k_getpc());
    scr2_0=IoMem[IoAccessCurrentAddress&IO_SEG_MASK];
    changed_bits ^= scr2_0;
    
    if (changed_bits&SCR2_SOFTINT1) {
        Log_Printf(LOG_SOFTINT_LEVEL,"[SCR2] SOFTINT1 change at $%08x val=%x PC=$%08x\n",
                   IoAccessCurrentAddress,scr2_0&SCR2_SOFTINT1,m68k_getpc());
        if (scr2_0&SCR2_SOFTINT1)
            set_interrupt(INT_SOFT1,SET_INT);
        else
            set_interrupt(INT_SOFT1,RELEASE_INT);
    }
    
    if (changed_bits&SCR2_SOFTINT2) {
        Log_Printf(LOG_SOFTINT_LEVEL,"[SCR2] SOFTINT2 change at $%08x val=%x PC=$%08x\n",
                           IoAccessCurrentAddress,scr2_0&SCR2_SOFTINT2,m68k_getpc());
        if (scr2_0&SCR2_SOFTINT2) 
            set_interrupt(INT_SOFT2,SET_INT);
        else
            set_interrupt(INT_SOFT2,RELEASE_INT);
    }
    
    /* DSP bits */
    if (changed_bits&SCR2_DSP_RESET) {
        if (scr2_0&SCR2_DSP_RESET) {
            if (!(scr2_0&SCR2_DSP_MODE_A)) {
                Log_Printf(LOG_DSP_LEVEL,"[SCR2] DSP Mode A");
            }
            if (!(scr2_0&SCR2_DSP_MODE_B)) {
                Log_Printf(LOG_DSP_LEVEL,"[SCR2] DSP Mode B");
            }
            Log_Printf(LOG_DSP_LEVEL,"[SCR2] DSP Start (mode %i)",(~(scr2_0>>3))&3);
            DSP_Start((~(scr2_0>>3))&3);
        } else {
            Log_Printf(LOG_DSP_LEVEL,"[SCR2] DSP Reset");
            DSP_Reset();
        }
    }
    if (changed_bits&SCR2_DSP_BLK_END) {
        dsp_intr_at_block_end = scr2_0&SCR2_DSP_BLK_END;
        Log_Printf(LOG_DSP_LEVEL,"[SCR2] %s DSP interrupt from DMA at block end",dsp_intr_at_block_end?"enable":"disable");
    }
    if (changed_bits&SCR2_DSP_UNPKD) {
        dsp_dma_unpacked = scr2_0&SCR2_DSP_UNPKD;
        Log_Printf(LOG_DSP_LEVEL,"[SCR2] %s DSP DMA unpacked mode",dsp_dma_unpacked?"enable":"disable");
    }
}

void SCR2_Read0(void)
{
    Log_Printf(LOG_SCR_LEVEL,"SCR2 read at $%08x PC=$%08x\n", IoAccessCurrentAddress,m68k_getpc());
    IoMem[IoAccessCurrentAddress&IO_SEG_MASK]=scr2_0;
}

void SCR2_Write1(void)
{
    Log_Printf(LOG_SCR_LEVEL,"SCR2 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress,IoMem[IoAccessCurrentAddress&IO_SEG_MASK],m68k_getpc());
    scr2_1=IoMem[IoAccessCurrentAddress&IO_SEG_MASK];
}

void SCR2_Read1(void)
{
    Log_Printf(LOG_SCR_LEVEL,"SCR2 read at $%08x PC=$%08x\n", IoAccessCurrentAddress,m68k_getpc());
    IoMem[IoAccessCurrentAddress&IO_SEG_MASK]=scr2_1;
}

void SCR2_Write2(void)
{
    Uint8 changed_bits=scr2_2;
    Log_Printf(LOG_SCR_LEVEL,"SCR2 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress,IoMem[IoAccessCurrentAddress&IO_SEG_MASK],m68k_getpc());
    scr2_2=IoMem[IoAccessCurrentAddress&IO_SEG_MASK];
    changed_bits^=scr2_2;
    
    if (changed_bits&SCR2_TIMERIPL7) {
        Log_Printf(LOG_WARN,"[SCR2] TIMER IPL7 change at $%08x val=%x PC=$%08x\n",
                   IoAccessCurrentAddress,scr2_2&SCR2_TIMERIPL7,m68k_getpc());
    }

    /* RTC enabled */
    if (scr2_2&SCR2_RTCE) {
        if ((changed_bits&SCR2_RTCLK) && !(scr2_2&SCR2_RTCLK)) {
            rtc_interface_write(scr2_2&SCR2_RTDATA);
        }
    } else if (changed_bits&SCR2_RTCE) {
        rtc_interface_reset();
    }
}

void SCR2_Read2(void)
{
    Log_Printf(LOG_SCR_LEVEL,"SCR2 read at $%08x PC=$%08x\n", IoAccessCurrentAddress,m68k_getpc());
    if (rtc_interface_read()) {
        scr2_2 |= SCR2_RTDATA;
    } else {
        scr2_2 &= ~SCR2_RTDATA;
    }
    IoMem[IoAccessCurrentAddress&IO_SEG_MASK]=scr2_2;
}

void SCR2_Write3(void)
{    
    Uint8 changed_bits=scr2_3;
    Log_Printf(LOG_SCR_LEVEL,"SCR2 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress,IoMem[IoAccessCurrentAddress & IO_SEG_MASK],m68k_getpc());
    scr2_3=IoMem[IoAccessCurrentAddress&IO_SEG_MASK];
    changed_bits^=scr2_3;
    
    if (changed_bits&SCR2_ROM) {
        SCR_ROM_overlay=scr2_3&SCR2_ROM;
        Log_Printf(LOG_WARN,"[SCR2] ROM change at $%08x val=%x PC=$%08x\n",
                   IoAccessCurrentAddress,scr2_3&SCR2_ROM,m68k_getpc());
    }
    if (changed_bits&SCR2_LED) {
        Log_Printf(LOG_DEBUG,"[SCR2] LED change at $%08x val=%x PC=$%08x\n",
                   IoAccessCurrentAddress,scr2_3&SCR2_LED,m68k_getpc());
        Statusbar_SetSystemLed(scr2_3&SCR2_LED);
    }
    
    if (changed_bits&SCR2_DSP_INT_EN) {
        Log_Printf(LOG_DSP_LEVEL,"[SCR2] DSP interrupt at level %i",(scr2_3&SCR2_DSP_INT_EN)?4:3);
        if (scrIntStat&(INT_DSP_L3|INT_DSP_L4)) {
            Log_Printf(LOG_DSP_LEVEL,"[SCR2] Switching DSP interrupt to level %i",(scr2_3&SCR2_DSP_INT_EN)?4:3);
            set_interrupt(INT_DSP_L3|INT_DSP_L4, RELEASE_INT);
            set_dsp_interrupt(SET_INT);
        }
    }
    if (changed_bits&SCR2_DSP_MEM_EN) {
        Log_Printf(LOG_WARN,"[SCR2] %s DSP memory",(scr2_3&SCR2_DSP_MEM_EN)?"disable":"enable");
    }
}


void SCR2_Read3(void)
{
    Log_Printf(LOG_SCR_LEVEL,"SCR2 read at $%08x PC=$%08x\n", IoAccessCurrentAddress,m68k_getpc());
    IoMem[IoAccessCurrentAddress&IO_SEG_MASK]=scr2_3;
}



/* Interrupt Status Register */

void IntRegStatRead(void) {
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, scrIntStat);
}

void IntRegStatWrite(void) {
    Log_Printf(LOG_WARN, "[INT] Interrupt status register is read-only.");
}

void set_dsp_interrupt(Uint8 state) {
    if (scr2_3&SCR2_DSP_INT_EN || ConfigureParams.System.bTurbo) {
        set_interrupt(INT_DSP_L4, state);
    } else {
        set_interrupt(INT_DSP_L3, state);
    }
}

void set_interrupt(Uint32 intr, Uint8 state) {
    /* The interrupt gets polled by the cpu via intlev()
     * --> see previous-glue.c
     */
    if (state==SET_INT) {
        scrIntStat |= intr;
    } else {
        scrIntStat &= ~intr;
    }
}

int scr_get_interrupt_level(Uint32 interrupt) {
    if (!interrupt) {
        return 0;
    } else if (interrupt&INT_L7_MASK) {
        return 7;
    } else if ((interrupt&INT_TIMER) && (scr2_2&SCR2_TIMERIPL7)) {
        return 7;
    } else if (interrupt&INT_L6_MASK) {
        return 6;
    } else if (interrupt&INT_L5_MASK) {
        return 5;
    } else if (interrupt&INT_L4_MASK) {
        return 4;
    } else if (interrupt&INT_L3_MASK) {
        return 3;
    } else if (interrupt&INT_L2_MASK) {
        return 2;
    } else if (interrupt&INT_L1_MASK) {
        return 1;
    } else {
        abort();
    }
}

/* Interrupt Mask Register */
#define INT_NONMASKABLE 0x80027640
#define INT_ZEROBITS    0xC22E7600 // Turbo

void IntRegMaskRead(void) {
    if (ConfigureParams.System.bTurbo) {
        IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, scrIntMask&~INT_ZEROBITS);
    } else {
        IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, scrIntMask);
    }
}

void IntRegMaskWrite(void) {
    scrIntMask = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    if (ConfigureParams.System.bTurbo) {
        scrIntMask |= INT_ZEROBITS;
    } else {
        scrIntMask |= INT_NONMASKABLE;
    }
    Log_Printf(LOG_DEBUG, "Interrupt mask: %08x", scrIntMask);
}


/* Hardclock internal interrupt */

#define HARDCLOCK_ENABLE 0x80
#define HARDCLOCK_LATCH  0x40
#define HARDCLOCK_ZERO   0x3F

static Uint8 hardclock_csr=0;
static Uint8 hardclock1=0;
static Uint8 hardclock0=0;
static int latch_hardclock=0;

static Uint64 hardClockLastLatch;

void Hardclock_InterruptHandler ( void )
{
    CycInt_AcknowledgeInterrupt();
    if ((hardclock_csr&HARDCLOCK_ENABLE) && (latch_hardclock>0)) {
//      Log_Printf(LOG_WARN,"[INT] throwing hardclock %lld", host_time_us());
        set_interrupt(INT_TIMER,SET_INT);
        Uint64 now = host_time_us();
        host_hardclock(latch_hardclock, now - hardClockLastLatch);
        hardClockLastLatch = now;
        CycInt_AddRelativeInterruptUs(latch_hardclock, 0, INTERRUPT_HARDCLOCK);
    }
}


void HardclockRead0(void){
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=(latch_hardclock>>8);
    Log_Printf(LOG_HARDCLOCK_LEVEL,"[hardclock] read at $%08x val=%02x PC=$%08x", IoAccessCurrentAddress,IoMem[IoAccessCurrentAddress & IO_SEG_MASK],m68k_getpc());
}
void HardclockRead1(void){
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=latch_hardclock&0xff;
    Log_Printf(LOG_HARDCLOCK_LEVEL,"[hardclock] read at $%08x val=%02x PC=$%08x", IoAccessCurrentAddress,IoMem[IoAccessCurrentAddress & IO_SEG_MASK],m68k_getpc());
}

void HardclockWrite0(void){
    Log_Printf(LOG_HARDCLOCK_LEVEL,"[hardclock] write at $%08x val=%02x PC=$%08x", IoAccessCurrentAddress,IoMem[IoAccessCurrentAddress & IO_SEG_MASK],m68k_getpc());
    hardclock0=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
}
void HardclockWrite1(void){
    Log_Printf(LOG_HARDCLOCK_LEVEL,"[hardclock] write at $%08x val=%02x PC=$%08x",IoAccessCurrentAddress,IoMem[IoAccessCurrentAddress & IO_SEG_MASK],m68k_getpc());
    hardclock1=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
}

void HardclockWriteCSR(void) {
    Log_Printf(LOG_HARDCLOCK_LEVEL,"[hardclock] write at $%08x val=%02x PC=$%08x", IoAccessCurrentAddress,IoMem[IoAccessCurrentAddress & IO_SEG_MASK],m68k_getpc());
    hardclock_csr=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
    if (hardclock_csr&HARDCLOCK_LATCH) {
        hardclock_csr&= ~HARDCLOCK_LATCH;
        latch_hardclock=(hardclock0<<8)|hardclock1;
        hardClockLastLatch = host_time_us();
    }
    if ((hardclock_csr&HARDCLOCK_ENABLE) && (latch_hardclock>0)) {
        Log_Printf(LOG_HARDCLOCK_LEVEL,"[hardclock] enable periodic interrupt (%i microseconds).", latch_hardclock);
        CycInt_AddRelativeInterruptUs(latch_hardclock, 0, INTERRUPT_HARDCLOCK);
    } else {
        Log_Printf(LOG_HARDCLOCK_LEVEL,"[hardclock] disable periodic interrupt.");
    }
    set_interrupt(INT_TIMER,RELEASE_INT);
}
void HardclockReadCSR(void) {
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=hardclock_csr;
//  Log_Printf(LOG_WARN,"[hardclock] read at $%08x val=%02x PC=$%08x", IoAccessCurrentAddress,IoMem[IoAccessCurrentAddress & IO_SEG_MASK],m68k_getpc());
    set_interrupt(INT_TIMER,RELEASE_INT);
}


/* Event counter register */

static Uint64 sysTimerOffset = 0;
static bool   resetTimer;

void System_Timer_Read(void) {
    Uint64 now = host_time_us();
    if(resetTimer) {
        sysTimerOffset = now;
        resetTimer = false;
    }
    now -= sysTimerOffset;
    IoMem_WriteLong(IoAccessCurrentAddress&IO_SEG_MASK, now & 0xFFFFF);
}

void System_Timer_Write(void) {
    resetTimer = true;
}

/* Color Video Interrupt Register */

#define VID_CMD_CLEAR_INT    0x01
#define VID_CMD_ENABLE_INT   0x02
#define VID_CMD_UNBLANK      0x04

Uint8 col_vid_intr = 0;

void ColorVideo_CMD_Write(void) {
    col_vid_intr=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
    Log_Printf(LOG_DEBUG,"[Color Video] Command write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    
    if (col_vid_intr&VID_CMD_CLEAR_INT) {
        set_interrupt(INT_DISK, RELEASE_INT);
    }
}

void color_video_interrupt(void) {
    if (col_vid_intr&VID_CMD_ENABLE_INT) {
        set_interrupt(INT_DISK, SET_INT);
        col_vid_intr &= ~VID_CMD_ENABLE_INT;
    }
}
