cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

# Set build type to "Release" if user did not specify any build type yet
# Other possible values: Debug, Release, RelWithDebInfo and MinSizeRel
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif(NOT CMAKE_BUILD_TYPE)

IF (CMAKE_BUILD_TYPE STREQUAL "Release")
	set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Target architectures" FORCE)
	set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Target Min 10.13" FORCE)
ENDIF (CMAKE_BUILD_TYPE STREQUAL "Release")

project(Previous)

set(APP_NAME "Previous")

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckStructHasMember)
include(CheckSymbolExists)
include(CheckCCompilerFlag)
include(DistClean)

# set(CMAKE_VERBOSE_MAKEFILE 1)

find_package(PkgConfig)


# ##########################
# Conditional build features
# ##########################

set(ENABLE_DSP_EMU 1
    CACHE BOOL "Enable DSP 56k emulator")
set(ENABLE_TRACING 1
    CACHE BOOL "Enable tracing messages for debugging")
set(ENABLE_RENDERING_THREAD 1
    CACHE BOOL "Enable rendering threads")

if(APPLE)
	set(ENABLE_OSX_BUNDLE 1
	    CACHE BOOL "Built Previous as macOS application bundle")
	set(ADDITIONAL_INCLUDES ${FRAMEWORKS})
	set_source_files_properties(${FRAMEWORKS} PROPERTIES MACOSX_PACKAGE_LOCATION Frameworks)
else()
	set(ENABLE_OSX_BUNDLE 0
	    CACHE BOOL "Built Previous as macOS application bundle")
endif(APPLE)

# ####################
# Check for libraries:
# ####################

find_package(SDL2)
if(NOT SDL2_FOUND)
	message(FATAL_ERROR "SDL2 library not found!")
endif(NOT SDL2_FOUND)

find_package(Math)

find_package(Readline)
if(Readline_FOUND)
	set(HAVE_LIBREADLINE 1)
endif(Readline_FOUND)

find_package(ZLIB)
if(ZLIB_FOUND)
	set(HAVE_LIBZ 1)
	set(HAVE_ZLIB_H 1)
endif(ZLIB_FOUND)

find_package(PNG)
if(PNG_FOUND)
	set(HAVE_LIBPNG 1)
endif(PNG_FOUND)

find_package(PCAP)
if(PCAP_FOUND)
	set(HAVE_PCAP 1)
endif(PCAP_FOUND)

# ################
# CPP Definitions:
# ################

# Test for large file support:
execute_process(COMMAND getconf LFS_CFLAGS
                OUTPUT_VARIABLE DETECTED_LFS_CFLAGS
                ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
if(DETECTED_LFS_CFLAGS)
	add_definitions(${DETECTED_LFS_CFLAGS})
	# message(STATUS "Large filesystem flags: ${DETECTED_LFS_CFLAGS}")
endif(DETECTED_LFS_CFLAGS)

# Additional CFLAGS suggested by the SDL library:
if(PKG_CONFIG_FOUND)
	execute_process(COMMAND ${PKG_CONFIG_EXECUTABLE} --cflags-only-other sdl2
			OUTPUT_VARIABLE SDL2_OTHER_CFLAGS
			ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
endif(PKG_CONFIG_FOUND)

# ###########################
# Check for optional headers:
# ###########################

check_include_files(byteswap.h HAVE_BYTESWAP_H)
check_include_files(strings.h HAVE_STRINGS_H)
check_include_files(limits.h HAVE_LIMITS_H)
check_include_files(sys/syslimits.h HAVE_SYS_SYSLIMITS_H)
check_include_files(sys/types.h HAVE_SYS_TYPES_H)
check_include_files(sys/xattr.h HAVE_SYS_XATTR_H)
check_include_files(tchar.h HAVE_TCHAR_H)
check_include_files(arpa/inet.h HAVE_ARPA_INET_H)
check_include_files(netinet/in.h HAVE_NETINET_IN_H)

# #############################
# Check for optional functions:
# #############################

check_symbol_exists(bswap_16 "byteswap.h" HAVE_BSWAP_16)
check_symbol_exists(bswap_32 "byteswap.h" HAVE_BSWAP_32)
check_symbol_exists(setenv "stdlib.h" HAVE_SETENV)
check_symbol_exists(select "sys/select.h" HAVE_SELECT)
check_symbol_exists(gettimeofday "sys/time.h" HAVE_GETTIMEOFDAY)
check_symbol_exists(nanosleep "time.h" HAVE_NANOSLEEP)
check_symbol_exists(alphasort "dirent.h" HAVE_ALPHASORT)
check_symbol_exists(scandir "dirent.h" HAVE_SCANDIR)
check_symbol_exists(fseeko "stdio.h" HAVE_FSEEKO)
check_symbol_exists(ftello "stdio.h" HAVE_FTELLO)
check_symbol_exists(flock "sys/file.h" HAVE_FLOCK)
check_symbol_exists(strdup "string.h" HAVE_STRDUP)
check_symbol_exists(lsetxattr "sys/xattr.h" HAVE_LXETXATTR)
check_symbol_exists(posix_memalign "stdlib.h" HAVE_POSIX_MEMALIGN)
check_symbol_exists(aligned_alloc "stdlib.h" HAVE_ALIGNED_ALLOC)
check_symbol_exists(_aligned_malloc "stdlib.h" HAVE__ALIGNED_MALLOC)

# ########################################
# Check for non-POSIXness on macOS et al.:
# ########################################
message("-- The following HAVE_STRUCT_* tests are checking for non-POSIX systems such as MacOS; 'Failed' does not indicate a problem")
check_struct_has_member("struct stat" st_atimespec.tv_nsec
    "sys/types.h;sys/stat.h" HAVE_STRUCT_STAT_ST_ATIMESPEC)
check_struct_has_member("struct stat" st_mtimespec.tv_nsec
    "sys/types.h;sys/stat.h" HAVE_STRUCT_STAT_ST_MTIMESPEC)
# Linux glibc provides a _D_EXACT_NAMELEN macro to work around this exact issue, but other supported platforms may not
check_struct_has_member("struct dirent" d_namelen
	"dirent.h" HAVE_STRUCT_DIRENT_D_NAMELEN)


# #############
# Other CFLAGS:
# #############

# GCC/Clang specific flags:
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
	# We want to allow ‘for’-loop initial declarations a la for(int i=0; ...)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
	# Warning flags:
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wcast-qual -Wbad-function-cast -Wpointer-arith")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wmissing-prototypes -Wstrict-prototypes")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wwrite-strings -Wsign-compare")
	#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wextra -Wno-unused-parameter -Wno-empty-body")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wformat-security")
	if(CMAKE_BUILD_TYPE STREQUAL "Debug")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_FORTIFY_SOURCE=2")
	endif()
	# Enable and disable some warnings and flags for Previous:
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 -D__USE_MINGW_ANSI_STDIO=1")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-conversion -Wno-unused -Wno-sign-compare")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-missing-field-initializers")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wtautological-compare -Wno-unused-parameter")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-sign-compare -Wno-conversion -Wno-unused")
endif()

CHECK_C_COMPILER_FLAG("-Wimplicit-fallthrough=2" WARN_FALLTRHOUGH_AVAILABLE)
if(WARN_FALLTRHOUGH_AVAILABLE)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wimplicit-fallthrough=2")
endif(WARN_FALLTRHOUGH_AVAILABLE)

CHECK_C_COMPILER_FLAG("-Wshadow=local" WARN_SHADOW_LOCAL_AVAILABLE)
if(WARN_SHADOW_LOCAL_AVAILABLE)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wshadow=local")
endif(WARN_SHADOW_LOCAL_AVAILABLE)

if(ENABLE_WERROR)
	if (ENABLE_TRACING)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
	else()
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -Wno-error=unused-function")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error=unused-but-set-variable")
	endif()
endif(ENABLE_WERROR)

# -Wpointer-bool-conversion is not in GCC yet (as of 10.2)
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpointer-bool-conversion -Wtautological-compare -Wno-unused-parameter ${CMAKE_CXX_FLAGS}")
endif()

# Building Previous w/o optimization is no fun...
IF (CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(CMAKE_C_FLAGS "-O ${CMAKE_C_FLAGS}")
ENDIF (CMAKE_BUILD_TYPE STREQUAL "Debug")

# ####################
# Paths configuration:
# ####################

if(NOT BINDIR)
	set(BINDIR bin)
endif()

if(NOT DATADIR)
	set(DATADIR share/previous)
endif()

if(NOT BIN2DATADIR)
	if(WIN32)
		set(BIN2DATADIR "."
		    CACHE STRING "Relative path from bindir to datadir")
	elseif(ENABLE_OSX_BUNDLE)
		set(BIN2DATADIR "../Resources"
		    CACHE STRING "Relative path from bindir to datadir")
	else()
		set(BIN2DATADIR "../share/previous"
		    CACHE STRING "Relative path from bindir to datadir")
	endif(WIN32)
	mark_as_advanced(BIN2DATADIR)
endif()

if(NOT MANDIR)
	set(MANDIR share/man/man1)
endif()

if(NOT DOCDIR)
	set(DOCDIR share/doc/previous)
endif()

if(NOT ETCDIR)
	if(WIN32)
		set(ETCDIR .)
	else()
		set(ETCDIR /etc)
	endif()
endif()

if(NOT ICONDIR)
	set(ICONDIR share/icons/hicolor)
endif()

if(ENABLE_OSX_BUNDLE)
	# put the config files in the app's bundle
	add_definitions(-DCONFDIR=\"../Resources\")
else()
	add_definitions(-DCONFDIR=\"${ETCDIR}\")
endif()

# #########################################
# Create config.h and recurse into subdirs:
# #########################################

configure_file(${CMAKE_SOURCE_DIR}/cmake/config-cmake.h
		${CMAKE_BINARY_DIR}/config.h)

add_subdirectory(src)

# ###################################################################
# Print a summary of the optional libraries with a short explanation:
# ###################################################################

message( "
Libraries summary :
-------------------
")

message("  - sdl :\tusing SDL2 ${SDL2_VERSION}")

if(PNG_FOUND)
  message( "  - png :\tfound, allows to print to files using png" )
else()
  message( "  - png :\tnot found, install it to print to files using png" )
endif(PNG_FOUND)

if(PCAP_FOUND)
  message( "  - pcap :\tfound, allows networking without NAT" )
else()
  message( "  - pcap :\tnot found, install it to use networking without NAT" )
endif(PCAP_FOUND)

if(HAVE_SYS_XATTR_H)
  message( "  - xattr.h :\tfound, allows netbooting from a folder" )
else()
  message( "  - xattr.h :\tnot found, no netboot possible" )
endif(HAVE_SYS_XATTR_H)

message( "" )
