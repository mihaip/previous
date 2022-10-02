#pragma once

#ifndef __NEXT_BUS_H__
#define __NEXT_BUS_H__

#include "log.h"
#include "cycInt.h"

#define LOG_NEXTBUS_LEVEL   LOG_NONE

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
    void nextbus_init(void);

    uint32_t nextbus_slot_lget(uint32_t addr);
    uint32_t nextbus_slot_wget(uint32_t addr);
    uint32_t nextbus_slot_bget(uint32_t addr);
    void nextbus_slot_lput(uint32_t addr, uint32_t val);
    void nextbus_slot_wput(uint32_t addr, uint32_t val);
    void nextbus_slot_bput(uint32_t addr, uint32_t val);

    uint32_t nextbus_board_lget(uint32_t addr);
    uint32_t nextbus_board_wget(uint32_t addr);
    uint32_t nextbus_board_bget(uint32_t addr);
    void nextbus_board_lput(uint32_t addr, uint32_t val);
    void nextbus_board_wput(uint32_t addr, uint32_t val);
    void nextbus_board_bput(uint32_t addr, uint32_t val);
    
    void NextBus_Reset(void);
    void NextBus_Pause(bool pause);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#ifdef __cplusplus

#define FOR_EACH_SLOT(slot) for(int slot = 2; slot < 8; slot += 2)
#define IF_NEXT_DIMENSION(slot, nd) if(NextDimension* nd = dynamic_cast<NextDimension*>(nextbus[(slot)]))

class NextBusSlot {
public:
    int slot;
    
    NextBusSlot(int slot);
    
    virtual ~NextBusSlot();
    
    virtual uint32_t slot_lget(uint32_t addr);
    virtual uint16_t slot_wget(uint32_t addr);
    virtual uint8_t  slot_bget(uint32_t addr);
    virtual void   slot_lput(uint32_t addr, uint32_t val);
    virtual void   slot_wput(uint32_t addr, uint16_t val);
    virtual void   slot_bput(uint32_t addr, uint8_t val);
    
    virtual uint32_t board_lget(uint32_t addr);
    virtual uint16_t board_wget(uint32_t addr);
    virtual uint8_t  board_bget(uint32_t addr);
    virtual void   board_lput(uint32_t addr, uint32_t val);
    virtual void   board_wput(uint32_t addr, uint16_t val);
    virtual void   board_bput(uint32_t addr, uint8_t val);
    
    virtual void   reset(void);
    virtual void   pause(bool pause);
};

class NextBusBoard : public NextBusSlot {
public:
    NextBusBoard(int slot);
};

extern NextBusSlot* nextbus[];

#endif /* __cplusplus */

#endif /* __NEXT_BUS_H__ */
