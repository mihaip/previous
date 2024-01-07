/*
  Previous - kms.h

  This file is distributed under the GNU General Public License, version 2
  or at your option any later version. Read the file gpl.txt for details.
*/

#ifndef PREV_KMS_H
#define PREV_KMS_H

extern void KMS_Reset(void);

extern void KMS_Ctrl_Snd_Write(void);
extern void KMS_Stat_Snd_Read(void);
extern void KMS_Ctrl_KM_Write(void);
extern void KMS_Stat_KM_Read(void);
extern void KMS_Ctrl_TX_Write(void);
extern void KMS_Stat_TX_Read(void);
extern void KMS_Ctrl_Cmd_Write(void);
extern void KMS_Stat_Cmd_Read(void);

extern void KMS_Data_Write(void);
extern void KMS_Data_Read(void);

extern void KMS_KM_Data_Read(void);

extern void kms_keydown(uint8_t modkeys, uint8_t keycode);
extern void kms_keyup(uint8_t modkeys, uint8_t keycode);
extern void kms_mouse_move(int x, bool left, int y, bool up);
extern void kms_mouse_button(bool left, bool down);

extern bool kms_send_codec_receive(uint32_t data);
extern bool kms_can_receive_codec(void);
extern void kms_send_sndout_underrun(void);
extern void kms_send_sndout_request(void);

void Mouse_Handler(void);

#endif /* PREV_KMS_H */
