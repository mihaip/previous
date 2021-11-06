#define SOUND_OUT_FREQUENCY  44100            /* Sound playback frequency */
#define SOUND_IN_FREQUENCY   8012             /* Sound recording frequency */
#define SOUND_BUFFER_SAMPLES 512

extern Uint8* snd_buffer;
extern int    snd_buffer_len;

void SND_Out_Handler(void);
void SND_In_Handler(void);
void Sound_Reset(void);
void Sound_Pause(bool pause);

void snd_start_output(Uint8 mode);
void snd_stop_output(void);
void snd_start_input(Uint8 mode);
void snd_stop_input(void);
bool snd_output_active(void);
bool snd_input_active(void);

void snd_send_sample(Uint32 data);
void snd_gpo_access(Uint8 data);
void snd_vol_access(Uint8 data);
