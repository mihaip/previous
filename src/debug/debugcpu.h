/*
  Hatari - debugcpu.h

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  Public CPU debugging header.
*/

#ifndef HATARI_DEBUGCPU_H
#define HATARI_DEBUGCPU_H

void DebugCpu_Check(void);
void DebugCpu_SetDebugging(void);
int DebugCpu_DisAsm(int nArgc, char *psArgs[]);
int DebugCpu_MemDump(int nArgc, char *psArgs[]);
int DebugCpu_Register(int nArgc, char *psArgs[]);
int DebugCpu_GetRegisterAddress(const char *reg, uint32_t **addr);

uint32_t DBGMemory_ReadLong(uint32_t addr);
uint16_t DBGMemory_ReadWord(uint32_t addr);
uint8_t  DBGMemory_ReadByte(uint32_t addr);

void DBGMemory_WriteLong(uint32_t addr,uint32_t val);
void DBGMemory_WriteWord(uint32_t addr,uint16_t val);
void DBGMemory_WriteByte(uint32_t addr,uint8_t val);

#endif /* HATARI_DEBUGCPU_H */
