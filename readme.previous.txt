

                                  Previous 2.3



 Contents:
 ---------
1. License
2. About Previous
3. Compiling and installing
4. Known problems
5. Running Previous
6. Contributors
7. Contact


 1) License
 ----------

This program is free software; you can redistribute it and/or modify it under 
the terms of the GNU General Public License as published by the Free Software 
Foundation; either version 2 of the License, or (at your option) any later 
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY 
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A 
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with 
this program; if not, write to the Free Software Foundation, 51 Franklin Street, 
Fifth Floor, Boston, MA 02110-1335 USA


 2) About Previous
 -----------------

Previous is a NeXT Computer emulator based on the Atari emulator Hatari. It 
uses the latest m68k emulation core from WinUAE and the i860 emulator from 
Jason Eckhardt. Previous works on all Systems which are supported by the SDL2 
library.

Previous emulates the following machines:
  NeXT Computer (original 68030 Cube)
  NeXTcube
  NeXTcube Turbo
  NeXTstation
  NeXTstation Turbo
  NeXTstation Color
  NeXTstation Turbo Color
  NeXTdimension Graphics Board


 3) Compiling and installing
 ---------------------------

For using Previous, you need to have installed the following libraries:

Required:
  > The SDL2 library v2.0.5 or later (http://www.libsdl.org)
  > The zlib compression library (http://www.gzip.org/zlib/)

Optional:
  > The libpng PNG reference library (http://www.libpng.org)
    This is required for printing to files.
  > The pcap library (https://github.com/the-tcpdump-group/libpcap or 
    https://www.winpcap.org)
    This is required if networking via PCAP is preferred over SLiRP.


Don't forget to also install the header files of these libraries for compiling
Previous (some Linux distributions use separate development packages for these
header files)!

For compiling Previous, you need C and C++ compilers and a working CMake 
(v3.3 or later) installation (see http://www.cmake.org/ for details).

CMake can generate makefiles for various flavors of "Make" (like GNU-Make)
and various IDEs like Xcode on macOS. To run CMake, you have to pass the
path to the sources of Previous as parameter. For example, run the following
command sequence to configure the build of Previous in a separate build 
directory (assuming that the current working directory is the top of the source 
tree):

	mkdir -p build
	cd build
	cmake ..

Have a look at the manual of CMake for other options. Alternatively, you can
use the "cmake-gui" program to configure the sources with a graphical
application or "ccmake" to configure them with ncurses UI.

Once CMake has successfully configured the build settings, you can compile
Previous with:

	cmake --build .

If all works fine, you should get the executable "Previous" in the src/ sub-
directory of the build tree. 


 4) Status
 ---------

Previous is stable, but some parts are still work in progress. Some hardware 
is not yet emulated. Status of the individual components is as follows:
  CPU             good (but not cycle-exact)
  MMU             good
  FPU             good
  DSP             good
  DMA             good
  NextBus         good
  Memory          good
  2-bit graphics  good
  Color graphics  good
  RTC             good
  Timers          good
  SCSI drive      good
  MO drive        good
  Floppy drive    good
  Ethernet        good
  Serial          dummy
  Printer         good
  Sound           good
  Keyboard        good
  Mouse           good
  ADB             dummy
  Nitro           dummy
  Dimension       partial (no video I/O)


There are remaining problems with the host to emulated machine interface for
input devices.


 5) Known issues
 ---------------

  > Un-emulated hardware may cause problems when attempted to being used.
  > NeXTdimension emulation does not work on hosts with big endian byte order.
  > ROM Monitor, boot log and boot animations won't show on NeXTdimension 
    monitor, if NeXTdimension main memory exceeds 32 MB. This is a bug in the 
    NeXT ROM.
  > The MO drive causes slow downs and hangs when both drives are connected, but 
    only one disk is inserted. This is no emulation issue but a bug in NeXTstep.
  > DSP sound has timing related issues. playscore under NeXTstep 0.9 sometimes 
    produces bad audio in variable speed mode. ScorePlayer under NeXTstep 2.x 
    produces distorted sound in normal CPU mode.
  > Shortcuts do not work properly or overlap with host commands on some 
    platforms.
  > CPU timings are not correct. You may experience performance differences 
    compared to real hardware.
  > 68882 transcendental FPU instructions produce results identical to 68040 
    FPSP.
    The results are slightly different from real 68882 results.
  > Changing network connection settings while a guest system is running 
    can cause permanently lost connections, especially under NeXTstep 2.2.


 6) Release notes
 ----------------

Previous v1.0:
  > Initial release.

Previous v1.1:
  > Adds Turbo chipset emulation.
  > Improves DSP interrupt handling.
  > Improves hardclock timing.

Previous v1.2:
  > Adds support for running Mac OS via Daydream.
  > Improves mouse movement handling.
  > Adds dummy Nitro emulation.
  > Improves dummy SCC emulation.

Previous v1.3:
  > Adds Laser Printer emulation.
  > Introduces option for swapping cmd and alt key.

Previous v1.4:
  > Adds NeXTdimension emulation, including emulated i860 CPU.
  > Improves timings and adds a mode for higher than real speed.
  > Improves emulator efficiency through optimizations and threads.
  > Improves mouse movement handling.
  > Improves Real Time Clock. Time is now handled correctly.

Previous v1.5:
  > Adds emulation of soundbox microphone to enable sound recording.
  > Fixes bug in SCSI code. Images greater than 4 GB are now supported.
  > Fixes bug in Real Time Clock. Years after 1999 are now accepted.
  > Fixes bug that prevented screen output on Linux.
  > Fixes bug that caused NeXTdimension to fail after disabling thread.

Previous v1.6:
  > Adds SoftFloat FPU emulation. Fixes FPU on non-x86 host platforms.
  > Adds emulation of FPU arithmetic exceptions.
  > Adds support for second magneto-optical disk drive.
  > Fixes bug that caused a crash when writing to an NFS server.
  > Fixes bug that prevented NeXTdimension from stopping in rare cases.
  > Fixes bug that caused external i860 interrupts to be delayed.
  > Fixes bug that prevented sound input under NeXTstep 0.8.
  > Fixes bug that caused temporary speed anomalies after pausing.
  > Improves dummy RAMDAC emulation.

Previous v1.7:
  > Adds support for twisted-pair Ethernet.
  > Adds SoftFloat emulation for 68882 transcendental FPU instructions.
  > Adds SoftFloat emulation for i860 floating point instructions.
  > Improves 68040 FPU emulation to support resuming of instructions.
  > Improves Ethernet connection stability.
  > Improves efficiency while emulation is paused.
  > Improves device timings to be closer to real hardware.
  > Fixes bug in timing system. MO drive now works in variable speed mode.
  > Fixes bug in 68040 MMU that caused crashes and kernel panics.
  > Fixes bug in 68040 FPU that caused crashes due to unnormal zero.
  > Fixes bug in FMOVEM that modified wrong FPU registers.
  > Fixes bug that sometimes caused hangs if sound was disabled.
  > Fixes bug that caused lags in responsiveness during sound output.
  > Fixes bug that caused a crash when using write protected image files.

Previous v1.8:
  > Removes support for host keyboard repeat because it became useless.
  > Fixes bug that caused FMOVECR to return wrong values in some cases.
  > Fixes bug in timing system that caused hangs in variable speed mode.

Previous v1.9:
  > Adds support for networking via PCAP library.
  > Improves 68030 and 68040 CPU to support all tracing modes.

Previous v2.0:
  > Adds support for multiple NeXTdimension boards.
  > Improves i860 timings to be closer to real hardware.

Previous v2.1:
  > Improves emulation efficiency.
  > Removes NeXTdimension startup timing hack.
  > Fixes bug that caused FLOGNP1 to give wrong results in rare cases.
  > Fixes bug that caused FABS and FNEG to incorrectly handle infinity.
  > Fixes bug that caused logarithmic functions to fail on NaN input.
  > Fixes bug that caused incorrect results from DSP ROL and ROR.

Previous v2.2:
  > Adds support for using custom MAC address.
  > Improves accuracy of tables programmed to DSP data ROM.
  > Fixes bug that prevented reset warning after changing preferences.

Previous v2.3:
  > Adds internal NFS server for easier file sharing with host system.
  > Improves DSP, CPU and FPU emulation accuracy.
  > Fixes bug that caused sporadic power-on test failures and crashes.
  > Fixes bug that prevented Previous from starting with SDL > 2.0.12.

Previous v2.4 (unreleased):
  > Fixes bug that caused slow disk access when running Mac OS.


 7) Running Previous
 -------------------

For running the emulator, you need an image of the boot ROM of the emulated 
machine.

While the emulator is running, you can open the configuration menu by pressing 
F12, toggle between fullscreen and windowed mode by pressing F11 and initiate 
a clean shut down by pressing F10 (emulates the power button).


 8) Contributors
 ---------------

Previous was written by Andreas Grabher, Simon Schubiger and Gilles Fetis.

Many thanks go to the members of the NeXT International Forums for their help. 
Special thanks go to Gavin Thomas Nicol, Piotr Twarecki, Toni Wilen, Michael 
Bosshard, Thomas Huth, Olivier Galibert, Jason Eckhardt, Jason Stevens, Daniel 
L'Hommedieu, Tomaz Slivnik, Vaughan Kaufman, Peter Leonard and Brent Spillner!

This emulator would not exist without their help.


 9) Contact
 ----------

If you want to contact the authors of Previous, please have a look at the 
NeXT International Forums (http://www.nextcomputers.org/forums).
