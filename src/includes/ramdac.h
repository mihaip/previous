#pragma once

#ifndef __RAMDAC_H__
#define __RAMDAC_H__

typedef struct {
    int   addr;
    int   idx;
    uint32_t wtt_tmp;
    uint32_t wtt[0x10];
    uint8_t ccr[0xC];
    uint8_t reg[0x30];
    uint8_t ram[0x630];
} bt463;



#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
    uae_u32 bt463_bget(bt463* ramdac, uaecptr addr);
    void bt463_bput(bt463* ramdac, uaecptr addr, uae_u32 b);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __RAMDAC_H__ */
