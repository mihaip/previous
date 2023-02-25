check_include_files(sys/filio.h HAVE_SYS_FILIO_H)
if(HAVE_SYS_FILIO_H)
	add_definitions(-DHAVE_SYS_FILIO_H)
endif(HAVE_SYS_FILIO_H)

check_include_files(sys/ioctl.h HAVE_SYS_IOCTL_H)
if(HAVE_SYS_IOCTL_H)
	add_definitions(-DHAVE_SYS_IOCTL_H)
endif(HAVE_SYS_IOCTL_H)

check_include_files(unistd.h HAVE_UNISTD_H)
if(HAVE_UNISTD_H)
	add_definitions(-DHAVE_UNISTD_H)
endif(HAVE_UNISTD_H)

add_definitions(-DHAVE_STRERROR)

# Supress some warnings for SLIRP
if(CMAKE_COMPILER_IS_GNUCC)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-sign-compare -Wno-strict-overflow")
endif(CMAKE_COMPILER_IS_GNUCC)

include_directories(../ditool ${SDL2_INCLUDE_DIRS})

add_library(Slirp
            bootp.c cksum.c if.c ip_icmp.c ip_input.c ip_output.c 
            mbuf.c misc.c ntp.c rip.c sbuf.c slirp.c slirpdebug.c socket.c 
            tcp_input.c tcp_output.c tcp_subr.c tcp_timer.c tftp.c udp.c

            nfs/compat.cpp
            nfs/XDRStream.cpp nfs/CSocket.cpp nfs/TCPServerSocket.cpp nfs/UDPServerSocket.cpp
            nfs/nfsd.cpp nfs/RPCServer.cpp nfs/VDNS.cpp
            nfs/RPCProg.cpp nfs/PortmapProg.cpp nfs/MountProg.cpp nfs/NFSProg.cpp nfs/NFS2Prog.cpp nfs/BootparamProg.cpp nfs/NetInfoProg.cpp nfs/NetInfoBindProg.cpp
            nfs/FileTableNFSD.cpp ../ditool/DiskImage.cpp ../ditool/Partition.cpp ../ditool/UFS.cpp ../ditool/VirtualFS.cpp
			)

target_link_libraries(Slirp PRIVATE ${SDL2_LIBRARIES})
