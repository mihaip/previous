/*
  Previous - grab.h

  This file is distributed under the GNU General Public License, version 2
  or at your option any later version. Read the file gpl.txt for details.
*/

#ifndef PREVIOUS_GRAB_H
#define PREVIOUS_GRAB_H

extern void Grab_Screen(void);

extern void Grab_Sound(uint8_t* samples, int len);
extern void Grab_SoundToggle(void);

extern void Grab_Stop(void);

extern volatile bool bRecordingAiff;

#endif /* PREVIOUS_GRAB_H */
