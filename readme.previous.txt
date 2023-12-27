

                                  Previous 2.9



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
this program; if not, write to the
  Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, 
  Boston, MA 02110-1301, USA


 2) About Previous
 -----------------

Previous is a NeXT Computer emulator based on the Atari emulator Hatari. It uses 
the latest m68k emulation core from WinUAE and the i860 emulator from Jason 
Eckhardt. Previous works on all Systems which are supported by the SDL2 library.

Previous emulates the following machines:
  NeXT Computer (original 68030 Cube)
  NeXTcube
  NeXTcube Turbo
  NeXTstation
  NeXTstation Turbo
  NeXTstation Color
  NeXTstation Turbo Color
  NeXTdimension Graphics Board

Less common machines can be emulated using custom options:
  NeXTstation with up to 128 MB RAM (serial number ABB 002 8000 or later): 
     Select NeXTstation Turbo and set CPU clock to 25 MHz
  NeXTcube, NeXTstation or NeXTstation Color with board revision 1:
     Select one of the above machines and set RTC chip to MCCS1850

Note that some hardware is only supported by later operating system versions.
Listed below are the system and ROM versions that introduced support for new
hardware:

  NeXTstep 0.8: NeXT Computer
           2.0: NeXTcube, NeXTstation
           2.1: NeXTstation Color, NeXTdimension
           2.2: NeXTcube Turbo, NeXTstation Turbo, NeXTstation Turbo Color

  ROM Rev. 0.8: NeXT Computer
           2.0: NeXTcube, NeXTstation, NeXTdimension
           2.4: NeXTstation Color
           3.0: NeXTcube Turbo, NeXTstation Turbo, NeXTstation Turbo Color

Previous includes an internal NFS and NetInfo server for advanced interaction 
with the host system. It also includes an internal time server that makes it 
possible to automatically synchronise host and guest system time. Please note 
that only NeXTstep 2.0 and later support automatic time synchronisation.

Previous comes with a command line utility called "ditool" (disk image tool). It
can be used to extract raw disk image files into a directory on the host system. 
You can get further informations about ditool's features and how to use it by 
calling it with the -h option (ditool -h). Be careful using the -clean option, 
because it will delete all files from the specified directory without asking.

Previous is able to netboot from a directory containing an extracted bootable 
installation of NeXTstep. The netboot feature requires a case sensitive file 
system and extended file attributes (sys/xattr.h) being available on the host 
system.


 3) Compiling and installing
 ---------------------------

For using Previous, you need to have installed the following libraries:

Required:
  > The SDL2 library v2.26.0 or later (http://www.libsdl.org)

Optional:
  > The zlib compression library (https://www.zlib.net)
    This is required for using libpng.
  > The libpng PNG reference library (http://www.libpng.org)
    This is required for printing to files.
  > The pcap library (https://www.tcpdump.org or https://www.winpcap.org)
    This is required if networking via PCAP is preferred over SLiRP.


Don't forget to also install the header files of these libraries for compiling
Previous (some Linux distributions use separate development packages for these
header files)!

For compiling Previous, you need C and C++ compilers and a working CMake (v3.3 
or later) installation (see http://www.cmake.org/ for details).

CMake can generate makefiles for various flavors of "Make" (like GNU-Make) and 
various IDEs like Xcode on macOS. To run CMake, you have to pass the path to the 
sources of Previous as parameter. For example, run the following command 
sequence to configure the build of Previous in a separate build directory 
(assuming that the current working directory is the top of the source tree):

	mkdir -p build
	cd build
	cmake ..

Have a look at the manual of CMake for other options. Alternatively, you can use 
the "cmake-gui" program to configure the sources with a graphical application or 
"ccmake" to configure them with ncurses UI.

Once CMake has successfully configured the build settings, you can compile 
Previous with:

	cmake --build .

If all works fine, you should get the executable "Previous" in the src/ sub-
directory of the build tree. 

Some systems do not support rendering from secondary threads. In case you get a 
black window try compiling with rendering threads disabled. In the instructions 
above replace "cmake .." with "../configure --disable-rendering-thread".

For more detailed building instructions read the file building.previous.txt.


 4) Status
 ---------

Previous is stable, but some parts are still work in progress. Some hardware is 
not yet emulated. Status of the individual components is as follows:
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
  Serial          partial
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

Issues in Previous:
  > Un-emulated hardware may cause problems when attempted to being used.
  > NeXTdimension emulation does not work on hosts with big endian byte order.
  > Shortcuts do not work properly or overlap with host commands on some 
    platforms.
  > CPU timings are not correct. You may experience performance differences 
    compared to real hardware.
  > 68882 transcendental FPU instructions produce results identical to 68040 
    FPSP. The results are slightly different from real 68882 results.
  > Diagnostic tests for Ethernet fail. Diagnostic tests for SCSI Disk and 
    Monitor/Sound fail in certain situations due to timing issues. Disable 
    variable speed mode to reliably pass SCSI Disk diagnostics. Disable sound to 
    pass Monitor/Sound diagnostics.
  > Background DMA Read from DSP test menu of the diagnostics utility fails for 
    an unknown reason. The test program is probably faulty. It sets the DMA 
    channel to the wrong direction.

Issues in NeXTstep:
  > The MO drive causes slow downs and hangs when both drives are connected, but 
    only one disk is inserted. This is confirmed to be a bug in NeXTstep.
  > ROM Monitor, boot log and boot animations won't show on NeXTdimension 
    monitor, if NeXTdimension main memory exceeds 32 MB. This is confirmed to be 
    a bug in the NeXT ROM.
  > Trying to netboot a non-Turbo 68040 machine while no Ethernet cable is 
    connected causes a hang. "ben" stops the system immediately while "btp" 
    shows one dot before it stops. This is the exact same behavior as seen on 
    real hardware. This is confirmed to be a bug in the NeXT ROM.


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

Previous v2.4:
  > Adds internal NetInfo server for easier networking.
  > Adds support for netbooting from a shared directory.
  > Adds support for switching between single and multiple screen mode.
  > Improves network auto-configuration.
  > Improves performance and accuracy of FSINCOS.
  > Improves sound volume adjustment and low-pass filter.
  > Improves mouse movement handling.
  > Improves DMA emulation accuracy.
  > Improves SCC and Floppy drive emulation to pass diagnostic tests.
  > Improves accuracy of system control and status registers.
  > Improves reliability of dual magneto-optical disk drive setups.
  > Fixes bug that caused slow disk access when running Mac OS.
  > Fixes bug that caused sound recording to be unreliable.
  > Fixes bug that prevented volume adjustment in certain conditions.
  > Fixes bug that caused network interface detection to be unreliable.
  > Fixes bug that caused keyboard initiated reset and NMI to fail.
  > Fixes bug in Real Time Clock. Time is now saved correctly.
  > Fixes bug that caused error messages during printer startup.
  > Fixes bug that caused black screen instead of options dialog on start.

Previous v2.5:
  > Adds support for changing window size.
  > Adds keyboard shortcut to hide statusbar.
  > Fixes bug that caused sporadic failures of the NeXTdimension board.
  > Fixes bug that caused display errors when using NeXTdimension board.

Previous v2.6:
  > Improves system control and status register access for booting Plan 9.
  > Improves window resizing.
  > Fixes bug that caused problems when changing statusbar visibility.

Previous v2.7:
  > Adds ability to unlock the mouse cursor with control-click.
  > Adds compile-time option to do all rendering from the main thread.
  > Improves accuracy of Ethernet controller on Turbo systems.
  > Improves handling of caps lock and modifier keys when using shortcuts.
  > Improves behavior of file selection dialog in certain situations.
  > Improves behavior of user interface in some edge cases.
  > Fixes bug that caused Daydream to hang on start up.
  > Fixes bug that caused errors when formatting floppy disks.
  > Fixes bug that prevented sound recording on newer versions of macOS.
  > Fixes bug that caused NeXTdimension window to start with wrong size.
  > Fixes bug in low-pass filter that could cause audible noise.

Previous v2.8:
  > Adds internal network time server.
  > Adds support for showing the main window if an alert occurs.
  > Adds support for creating block and character specials via NFS.
  > Adds support for setting up DNS from the internal NetInfo server.
  > Improves SCSI controller emulation to pass diagnostic tests.
  > Improves accuracy of SCSI command and error condition handling.
  > Improves accuracy of Real Time Clock chip.
  > Improves accuracy of Ethernet DMA channel for booting NetBSD.
  > Improves internal NFS server to support writes via TCP.
  > Improves accuracy of system status registers for later NeXTstations.
  > Fixes bug that caused endless loop after STOP instruction.
  > Fixes bug that prevented correct detection of network interface.
  > Fixes bug that could lead to incorrect SCSI controller detection.
  > Fixes bug in SCSI DMA channel that could cause incomplete transfers.
  > Fixes bug that broke networking on 32-bit host platforms.
  > Fixes bug that could cause device number corruption on NFS.

Previous v2.9:
  > Adds support for using any base clock speed in variable speed mode.
  > Adds support for saving screen contents to a file.
  > Adds support for saving sound output to a file.
  > Improves SCC emulation for booting Mac OS 7.5.3 via Daydream.

Previous v3.0 (unreleased):
  > Adds support for mapping scroll wheel to cursor keys.
  > Adds option for simulating DSP bootstrap ROM.
  > Adds support for RESET instruction.
  > Adds support for identifying some SCSI disk models for easier setup.
  > Improves screen simulation to not show screen contents while blanked.
  > Improves DSP emulation accuracy.
  > Improves CPU emulation accuracy.
  > Improves keyboard options to show actual shortcuts.
  > Fixes bug that caused mouse to be unlocked after resuming emulation.
  > Fixes bug that caused stuck modifier keys after using shortcuts.
  > Fixes bug that caused sporadic display errors in the boot options dialog.


 7) Running Previous
 -------------------

For running the emulator, you need an image of the boot ROM of the emulated 
machine.

You can initiate a clean shut down of the emulator using the simulated power 
key. The power key is mapped to F10 and the Delete key of your keyboard.

While the emulator is running, you can open the configuration menu by pressing 
F12 and toggle between fullscreen and windowed mode by pressing F11.

Further shortcuts can be invoked by simultaneously pressing ctrl and alt and
one of the following keys:

O: Shows main menu (same as F12).
P: Pauses emulation and resumes it when pressed again.
C: Initiates a cold reset. All unsaved changes will be lost.
M: Locks the mouse cursor to the window and releases it when pressed again.
N: Switches between screens if multiple screens are used in single window or 
   fullscreen mode.
G: Grabs the screen contents and saves them to a PNG file. The file is saved 
   inside the directory specified for printer output.
R: Records all sound output to an AIFF file and stops recording when pressed 
   again. The file is saved inside the directory specified for printer output.
F: Toggles between fullscreen and windowed mode (same as F11).
B: Hides the statusbar and shows it when pressed again.
S: Disables sound output and re-enables it when pressed again.
Q: Requests to quit Previous. All unsaved changes will be lost.

Shortcuts can be changed by modifying the preferences file. To modify single 
key shortcuts change or add key names in the section ShortcutsWithoutModifiers. 
To modify shortcuts that are invoked with ctrl and alt change or add key names 
in the section ShortcutsWithModifiers. The appropriate key names can be found 
under https://wiki.libsdl.org/SDL2/SDL_Keycode.

Previous comes with some empty disk images of different types and sizes. The 
suffix of the respective file name indicates the image type:
sd: Hard disk (SCSI disk)
od: Magneto-optical disk
fd: Floppy disk

When working with versions of NeXTstep prior to 2.0 it is recommended to use
the images marked with an 'x'. Only these are automatically detected by early 
versions of BuildDisk.


 8) Contributors
 ---------------

Previous was written by Andreas Grabher, Simon Schubiger and Gilles Fetis.

Many thanks go to the members of the NeXT International Forums for their help. 
Special thanks go to Gavin Thomas Nicol, Piotr Twarecki, Toni Wilen, Michael 
Bosshard, Thomas Huth, Olivier Galibert, Jason Eckhardt, Jason Stevens, Daniel 
L'Hommedieu, Tomaz Slivnik, Vaughan Kaufman, Peter Leonard, Brent Spillner, 
Frank Wegmann, Grzegorz Szwoch, Michael Engel, Izumi Tsutsui and William
Barnett-Lewis!

This emulator would not exist without their help.


 9) Contact
 ----------

If you want to contact the authors of Previous, please have a look at the NeXT 
International Forums (http://www.nextcomputers.org/forums).
