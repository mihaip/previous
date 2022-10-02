/*
  Hatari - ioMem.h

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/

#ifndef HATARI_IOMEM_H
#define HATARI_IOMEM_H

#include "config.h"
#include "memory.h"

#define IoMem NEXTIo

extern uint32_t IoAccessBaseAddress;
extern uint32_t IoAccessCurrentAddress;
extern int nIoMemAccessSize;

/**
 * Read 32-bit word from IO memory space without interception.
 * NOTE - value will be converted to PC endian.
 */
static inline uint32_t IoMem_ReadLong(uint32_t Address)
{
	Address &= 0x01ffff;
	return do_get_mem_long(&IoMem[Address]);
}


/**
 * Read 16-bit word from IO memory space without interception.
 * NOTE - value will be converted to PC endian.
 */
static inline uint16_t IoMem_ReadWord(uint32_t Address)
{
	Address &= 0x01ffff;
	return do_get_mem_word(&IoMem[Address]);
}


/**
 * Read 8-bit byte from IO memory space without interception.
 */
static inline uint8_t IoMem_ReadByte(uint32_t Address)
{
	Address &= 0x01ffff;
 	return IoMem[Address];
}


/**
 * Write 32-bit word into IO memory space without interception.
 * NOTE - value will be convert to 68000 endian
 */
static inline void IoMem_WriteLong(uint32_t Address, uint32_t Var)
{
	Address &= 0x01ffff;
	do_put_mem_long(&IoMem[Address], Var);
}


/**
 * Write 16-bit word into IO memory space without interception.
 * NOTE - value will be convert to 68000 endian.
 */
static inline void IoMem_WriteWord(uint32_t Address, uint16_t Var)
{
	Address &= 0x01ffff;
	do_put_mem_word(&IoMem[Address], Var);
}


/**
 * Write 8-bit byte into IO memory space without interception.
 */
static inline void IoMem_WriteByte(uint32_t Address, uint8_t Var)
{
	Address &= 0x01ffff;
	IoMem[Address] = Var;
}


void IoMem_Init(void);
void IoMem_UnInit(void);

uae_u32 IoMem_bget(uaecptr addr);
uae_u32 IoMem_wget(uaecptr addr);
uae_u32 IoMem_lget(uaecptr addr);

void IoMem_bput(uaecptr addr, uae_u32 val);
void IoMem_wput(uaecptr addr, uae_u32 val);
void IoMem_lput(uaecptr addr, uae_u32 val);

void IoMem_BusErrorEvenReadAccess(void);
void IoMem_BusErrorOddReadAccess(void);
void IoMem_BusErrorEvenWriteAccess(void);
void IoMem_BusErrorOddWriteAccess(void);
void IoMem_VoidRead(void);
void IoMem_VoidRead_00(void);
void IoMem_VoidWrite(void);
void IoMem_WriteWithoutInterception(void);
void IoMem_ReadWithoutInterception(void);
void IoMem_WriteWithoutInterceptionButTrace(void);
void IoMem_ReadWithoutInterceptionButTrace(void);
void IoMem_Debug(void);

#endif
