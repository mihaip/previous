#include "main.h"
#include "configuration.h"
#include "m68000.h"
#include "sysdeps.h"
#include "cycInt.h"
#include "audio.h"
#include "snd.h"
#include "kms.h"

#define LOG_SND_LEVEL   LOG_DEBUG
#define LOG_VOL_LEVEL   LOG_DEBUG

#define ENABLE_LOWPASS  1 /* experimental */

/* Initialize the audio system */
static bool   sndout_inited;
static bool   sound_output_active = false;
static bool   sndin_inited;
static bool   sound_input_active = false;

uint8_t*      snd_buffer = NULL;
int           snd_buffer_len = 0;

static void sound_init(void) {
    if (!sndout_inited && ConfigureParams.Sound.bEnableSound) {
        Log_Printf(LOG_WARN, "[Sound] Initializing output device.");
        Audio_Output_Init();
        sndout_inited = true;
    }
    if (!sndin_inited && sound_input_active && ConfigureParams.Sound.bEnableSound) {
        Log_Printf(LOG_WARN, "[Sound] Initializing input device.");
        Audio_Input_Init();
        sndin_inited = true;
    }
    if (sound_output_active && sndout_inited) {
        Log_Printf(LOG_WARN, "[Sound] Starting output.");
        Audio_Output_Enable(true);
    }
    if (sound_input_active && sndin_inited) {
        Log_Printf(LOG_WARN, "[Sound] Starting input.");
        Audio_Input_Enable(true);
    }
}

static void sound_uninit(void) {
    if (sndout_inited) {
        Log_Printf(LOG_WARN, "[Sound] Uninitializing output device.");
        sndout_inited = false;
        Audio_Output_UnInit();
    }
    if (sndin_inited) {
        Log_Printf(LOG_WARN, "[Sound] Uninitializing input device.");
        sndin_inited = false;
        Audio_Input_UnInit();
    }
}

void Sound_Reset(void) {
    if (snd_buffer)
        free(snd_buffer);
    snd_buffer = NULL;
    snd_buffer_len = 0;
}

void Sound_Pause(bool pause) {
    if (pause) {
        sound_uninit();
    } else {
        sound_init();
    }
}

/* Start and stop sound output */
struct {
    uint8_t mode;
    uint8_t mute;
    uint8_t lowpass;
    uint8_t volume[2]; /* 0 = left, 1 = right */
} sndout_state;

/* Maximum volume (really is attenuation) */
#define SND_MAX_VOL 43

/* Valid modes */
#define SND_MODE_NORMAL 0x00
#define SND_MODE_DBL_RP 0x10
#define SND_MODE_DBL_ZF 0x30

/* Function prototypes */
int  snd_send_samples(uint8_t* bufffer, int len);
void snd_make_normal_samples(uint8_t *buf, int len);
void snd_make_double_samples(uint8_t *buf, int len, bool repeat);
void snd_adjust_volume_and_lowpass(uint8_t *buf, int len);
void sndout_queue_put(uint8_t *buf, int len);

void snd_start_output(uint8_t mode) {
    sndout_state.mode = mode;
    /* Starting SDL Audio */
    if (sndout_inited) {
        Audio_Output_Enable(true);
    } else {
        Log_Printf(LOG_SND_LEVEL, "[Sound] Not starting. Sound output device not initialized.");
    }
    /* Starting sound output loop */
    if (!sound_output_active) {
        Log_Printf(LOG_SND_LEVEL, "[Sound] Starting output loop.");
        sound_output_active = true;
        Audio_Output_Queue_Clear();
        CycInt_AddRelativeInterruptCycles(10, INTERRUPT_SND_OUT);
    } else { /* Even re-enable loop if we are already active. This lowers the delay. */
        Log_Printf(LOG_DEBUG, "[Sound] Restarting output loop.");
        CycInt_AddRelativeInterruptCycles(10, INTERRUPT_SND_OUT);
    }
}

void snd_stop_output(void) {
    sound_output_active=false;
}

void snd_start_input(uint8_t mode) {
    
    /* Starting SDL Audio */
    if (sndin_inited) {
        Audio_Input_Enable(true);
    } else if (ConfigureParams.Sound.bEnableSound) {
        sndin_inited = true;
        Audio_Input_Init();
        Audio_Input_Enable(true);
    }
    /* Starting sound input loop */
    if (!sound_input_active) {
        Log_Printf(LOG_SND_LEVEL, "[Sound] Starting input loop.");
        sound_input_active = true;
        CycInt_AddRelativeInterruptCycles(10, INTERRUPT_SND_IN);
    } else { /* Even re-enable loop if we are already active. This lowers the delay. */
        Log_Printf(LOG_DEBUG, "[Sound] Restarting input loop.");
        CycInt_AddRelativeInterruptCycles(10, INTERRUPT_SND_IN);
    }
}

void snd_stop_input(void) {
    sound_input_active=false;
    sndin_inited = false;
    Audio_Input_UnInit();
}

/* Sound IO loops */

/*
 At a playback rate of 44.1kHz a sample takes about 23 microseconds.
 Assuming that the emulation runs at least 1/3 as fast as a real m68k
 checking the sound queue every 8 microseconds should be ok.
*/
static const int SND_CHECK_DELAY = 8;
void SND_Out_Handler(void) {
    CycInt_AcknowledgeInterrupt();
    
    if (!sound_output_active) {
        return;
    }

    if (sndout_inited && Audio_Output_Queue_Size() > SOUND_BUFFER_SAMPLES * 2) {
        CycInt_AddRelativeInterruptUs(SND_CHECK_DELAY * SOUND_BUFFER_SAMPLES, 0, INTERRUPT_SND_OUT);
        return;
    }
    
    kms_send_sndout_request();
    
    if (snd_buffer_len) {
        snd_buffer_len = snd_send_samples(snd_buffer, snd_buffer_len);
        snd_buffer_len = (snd_buffer_len / 4) + 1;
        CycInt_AddRelativeInterruptUs(SND_CHECK_DELAY * snd_buffer_len, 0, INTERRUPT_SND_OUT);
    } else {
        kms_send_sndout_underrun();
        /* Call do_dma_sndout_intr() a little bit later */
        CycInt_AddRelativeInterruptUs(100, 0, INTERRUPT_SND_OUT);
    }
}

bool snd_output_active() {
    return sound_output_active;
}

/* This functions generates 8-bit ulaw samples from 16 bit pcm audio */
#define BIAS 0x84               /* define the add-in bias for 16 bit samples */
#define CLIP 32635

static uint8_t snd_make_ulaw(int16_t sample) {
    static int16_t exp_lut[256] = {
        0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
    };
    int16_t sign, exponent, mantissa;
    uint8_t ulawbyte;
    
    /** get the sample into sign-magnitude **/
    sign = (sample >> 8) & 0x80;        /* set aside the sign */
    if (sign != 0) {
        sample = -sample;         /* get magnitude */
    }
    /* sample can be zero because we can overflow in the inversion,
     * checking against the unsigned version solves this */
    if (((uint16_t) sample) > CLIP)
        sample = CLIP;            /* clip the magnitude */
    
    /** convert from 16 bit linear to ulaw **/
    sample = sample + BIAS;
    exponent = exp_lut[(sample >> 7) & 0xFF];
    mantissa = (sample >> (exponent + 3)) & 0x0F;
    ulawbyte = ~(sign | (exponent << 4) | mantissa);
    
    return ulawbyte;
}

/*
 Sound is recorded at 8012 Hz. One sample (byte) takes about 124 microseconds.
*/
static const int SNDIN_SAMPLE_TIME = 124;
void SND_In_Handler(void) {
    int16_t sample;
    uint32_t foursamples;
    int size = 0;
    
    CycInt_AcknowledgeInterrupt();
    
    if (!sound_input_active) {
        return;
    }
    if (!kms_can_receive_codec()) {
        return;
    }

    Audio_Input_Lock();
    
    /* Process 256 samples at a time and then sync */
    while (size<256) {
        if (Audio_Input_Read(&sample) < 0) {
            Log_Printf(LOG_WARN, "[Sound] Waiting for sound input data");
            size = 256;
            break;
        }
        
        /* Shift in sample (oldest first) */
        foursamples = (foursamples<<8) | snd_make_ulaw(sample);
        
        size++;

        /* After accumulating 4 samples, send them to KMS */
        if ((size&3)==0) {
            if (kms_send_codec_receive(foursamples)) {
                break;
            }
        }
    }
    
    /* If we accumulated too much data write it fast */
    if (Audio_Input_BufSize() > 8192) { /* this is 4096 ulaw samples equaling about 0.5 seconds */
        Log_Printf(LOG_WARN, "[Sound] Writing input data fast");
        size = 16; /* Short delay */
    }
    
    Audio_Input_Unlock();

    if (kms_can_receive_codec()) {
        CycInt_AddRelativeInterruptUs(size*SNDIN_SAMPLE_TIME, 0, INTERRUPT_SND_IN);
    }
}

bool snd_input_active() {
    return sound_input_active;
}


/* These functions put samples to a buffer for further processing */
void snd_make_double_samples(uint8_t *buffer, int len, bool repeat) {
    for (int i=len - 4; i >= 0; i -= 4) {
        buffer[i*2+7] = repeat ? buffer[i+3] : 0; /* repeat or zero-fill */
        buffer[i*2+6] = repeat ? buffer[i+2] : 0; /* repeat or zero-fill */
        buffer[i*2+5] = repeat ? buffer[i+1] : 0; /* repeat or zero-fill */
        buffer[i*2+4] = repeat ? buffer[i+0] : 0; /* repeat or zero-fill */
        buffer[i*2+3] =          buffer[i+3];
        buffer[i*2+2] =          buffer[i+2];
        buffer[i*2+1] =          buffer[i+1];
        buffer[i*2+0] =          buffer[i+0];
    }
}


void snd_make_normal_samples(uint8_t *buffer, int len) {
    // do nothing
}


/* This function processes and sends out our samples */
int snd_send_samples(uint8_t* buffer, int len) {
    switch (sndout_state.mode) {
        case SND_MODE_NORMAL:
            snd_make_normal_samples(buffer, len);
            snd_adjust_volume_and_lowpass(buffer, len);
            Audio_Output_Queue(buffer, len);
            return len;
        case SND_MODE_DBL_RP:
            snd_make_double_samples(buffer, len, true);
            snd_adjust_volume_and_lowpass(buffer, 2*len);
            Audio_Output_Queue(buffer, len);
            Audio_Output_Queue(buffer+len, len);
            return 2*len;
        case SND_MODE_DBL_ZF:
            snd_make_double_samples(buffer, len, false);
            snd_adjust_volume_and_lowpass(buffer, 2*len);
            Audio_Output_Queue(buffer, len);
            Audio_Output_Queue(buffer+len, len);
            return 2*len;
        default:
            Log_Printf(LOG_WARN, "[Sound] Error: Unknown sound output mode!");
            return 0;
    }
}

/* This function processes and sends one single sample */
void snd_send_sample(uint32_t data) {
    uint8_t buf[8];
    
    if (!sound_output_active)
        return;
    
    buf[0] = data<<24;
    buf[1] = data<<16;
    buf[2] = data<<8;
    buf[3] = data;
    
    snd_send_samples(buf, 4);
}

#if ENABLE_LOWPASS
/* This is a third-order Butterworth low-pass filter (alpha value 0.1) */
static int16_t snd_lowpass_filter(int16_t sample, int channel) {
    static double v[2][4] = { {0.0,0.0,0.0,0.0}, {0.0,0.0,0.0,0.0} };
    double result;
    
    v[channel][0] = v[channel][1];
    v[channel][1] = v[channel][2];
    v[channel][2] = v[channel][3];
    v[channel][3] = ( 0.01809893300751444500 * sample)
                  + ( 0.27805991763454640520 * v[channel][0])
                  + (-1.18289326203783096148 * v[channel][1])
                  + ( 1.76004188034316899625 * v[channel][2]);
    result = (v[channel][0] + v[channel][3]) + 3 * (v[channel][1] + v[channel][2]);
    
    if (result >  32767.0) return  32767;
    if (result < -32768.0) return -32768;
    
    return (int16_t)result;
}
#endif

/* This function returns a factor for adding volume adjustment to samples */
static double snd_get_volume_factor(int channel) {
    double gain = sndout_state.volume[channel] * -2.0;
    
    switch (sndout_state.volume[channel]) {
        case 0:           return 1.0;
        case SND_MAX_VOL: return 0.0;
        default:          return pow(10.0, gain*0.05);
    }
}

/* This function adjusts sound output volume */
void snd_adjust_volume_and_lowpass(uint8_t *buf, int len) {
    int i;
    int16_t ldata, rdata;
    double ladjust, radjust;
    if (sndout_state.mute) {
        for (i=0; i<len; i++) {
            buf[i] = 0;
        }
    } else if (sndout_state.volume[0] || sndout_state.volume[1] || sndout_state.lowpass) {
        ladjust = snd_get_volume_factor(0);
        radjust = snd_get_volume_factor(1);
        
        for (i=0; i<len; i+=4) {
            ldata = ((int16_t)buf[i+0]<<8)|buf[i+1];
            rdata = ((int16_t)buf[i+2]<<8)|buf[i+3];
#if ENABLE_LOWPASS
            if (sndout_state.lowpass) {
                ldata = snd_lowpass_filter(ldata, 0);
                rdata = snd_lowpass_filter(rdata, 1);
            }
#endif
            ldata *= ladjust;
            rdata *= radjust;
            buf[i+0] = ldata>>8;
            buf[i+1] = ldata;
            buf[i+2] = rdata>>8;
            buf[i+3] = rdata;
        }
    }
}


/* Internal volume control register access (shifted in left to right)
 *
 * xxx ---- ----  start bits (all 1)
 * --- xx-- ----  channel (0x80 = right, 0x40 = left)
 * --- --xx xxxx  volume
 */

uint16_t tmp_vol;
int bit_num;

static void snd_shift_volume_reg(uint8_t databit) {
    Log_Printf(LOG_VOL_LEVEL, "[Sound] Interface shift bit %i (%i).",bit_num,databit?1:0);
    
    tmp_vol <<= 1;
    tmp_vol |= (databit?1:0);
    
    bit_num++;
}

static void snd_volume_interface_reset(void) {
    Log_Printf(LOG_VOL_LEVEL, "[Sound] Interface reset.");
    
    bit_num = 0;
    tmp_vol = 0;
}

static void snd_save_volume_reg(void) {
    uint8_t chan_lr, vol_data;
    
    if (bit_num!=11 || ((tmp_vol>>8)&7)!=7) {
        Log_Printf(LOG_WARN, "[Sound] Bad volume transfer (%i bits, start %i).",bit_num,(tmp_vol>>8)&7);
        return;
    }
    chan_lr = (tmp_vol&0xC0)>>6;
    vol_data = tmp_vol&0x3F;
    
    if (vol_data>SND_MAX_VOL) {
        Log_Printf(LOG_WARN, "[Sound] Gain limit exceeded (-%d dB).",vol_data*2);
        vol_data=SND_MAX_VOL;
    }
    if (chan_lr&1) {
        Log_Printf(LOG_WARN, "[Sound] Setting gain of left channel to -%d dB",vol_data*2);
        sndout_state.volume[0] = vol_data;
    }
    if (chan_lr&2) {
        Log_Printf(LOG_WARN, "[Sound] Setting gain of right channel to -%d dB",vol_data*2);
        sndout_state.volume[1] = vol_data;
    }
}

void snd_vol_access(uint8_t data) {
    Log_Printf(LOG_VOL_LEVEL, "[Sound] Volume access: %02X",data);
    
    bit_num = 11;
    tmp_vol = 0x700 | data;
    snd_save_volume_reg();
    snd_volume_interface_reset();
}

/* This function fills the internal volume register */
#define SND_SPEAKER_ENABLE  0x10
#define SND_LOWPASS_ENABLE  0x08

#define SND_INTFC_CLOCK     0x04
#define SND_INTFC_DATA      0x02
#define SND_INTFC_STROBE    0x01

uint8_t old_data;

void snd_gpo_access(uint8_t data) {
    Log_Printf(LOG_VOL_LEVEL, "[Sound] Control logic access: %02X",data);
    
    sndout_state.mute = data&SND_SPEAKER_ENABLE;
    sndout_state.lowpass = data&SND_LOWPASS_ENABLE;
    
    if (data&SND_INTFC_STROBE) {
        snd_save_volume_reg();
    } else if (old_data&SND_INTFC_STROBE) {
        snd_volume_interface_reset();
    } else if ((data&SND_INTFC_CLOCK) && !(old_data&SND_INTFC_CLOCK)) {
        snd_shift_volume_reg(data&SND_INTFC_DATA);
    }
    old_data = data;
}
