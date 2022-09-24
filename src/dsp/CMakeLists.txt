
include_directories(. ../.. ../debug ../includes) 

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-write-strings")

add_library(DSP
    dsp.c dsp_core.c dsp_cpu.c dsp_disasm.c
)

target_link_libraries(DSP PRIVATE ${SDL2_LIBRARIES})
