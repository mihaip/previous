
include_directories(. ../.. ../debug ../includes) 

add_library(DSP
    dsp.c dsp_core.c dsp_cpu.c dsp_disasm.c
)
