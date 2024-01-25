#include "host.h"
#include "audio.h"
#include "main.h"
#include "snd.h"
#include "sysdeps.h"

#include <emscripten.h>

void Audio_Output_Enable(bool bEnable) {

}

void Audio_Output_Init(void) {
    uint32_t sample_size = 16;
    uint32_t channels = 2;
    EM_ASM_({ workerApi.didOpenAudio($0, $1, $2, $3); }, SOUND_OUT_FREQUENCY, sample_size, channels);
}

void Audio_Output_UnInit(void) {

}

void Audio_Output_Queue(uint8_t* data, int len) {
    EM_ASM_({ workerApi.enqueueAudio($0, $1); }, data, len);
}

void Audio_Output_Queue_Clear(void) {

}

uint32_t Audio_Output_Queue_Size(void) {
    int js_audio_buffer_size = EM_ASM_INT_V({ return workerApi.audioBufferSize(); });
    return js_audio_buffer_size;
}

// Audio input is currently unsupported
void Audio_Input_Enable(bool bEnable) {}
void Audio_Input_Init(void) {}
void Audio_Input_UnInit(void) {}
void Audio_Input_Lock(void) {}
int Audio_Input_Read(int16_t* sample) {
    *sample = 0; // silence
    return 0;
}
int Audio_Input_BufSize(void) {
    return 0;
}
void Audio_Input_Unlock(void) {}
