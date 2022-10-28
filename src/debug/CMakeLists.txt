
if(Readline_FOUND)
	include_directories(${READLINE_INCLUDE_DIR})
endif(Readline_FOUND)

include_directories(${SDL2_INCLUDE_DIRS})

add_library(Debug
	    log.c debugui.c breakcond.c debugcpu.c debugInfo.c
	    ${DSPDBG_C} evaluate.c profile.c symbols.c 68kDisass.c)

target_link_libraries(Debug PRIVATE ${SDL2_LIBRARIES})
