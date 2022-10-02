void Audio_Output_Enable(bool bEnable);
void Audio_Output_Init(void);
void Audio_Output_UnInit(void);
void Audio_Output_Queue(uint8_t* data, int len);
void Audio_Output_Queue_Clear(void);
uint32_t Audio_Output_Queue_Size(void);

void Audio_Input_Enable(bool bEnable);
void Audio_Input_Init(void);
void Audio_Input_UnInit(void);
void Audio_Input_Lock(void);
int  Audio_Input_Read(int16_t* sample);
int  Audio_Input_BufSize(void);
void Audio_Input_Unlock(void);
