void Audio_Output_Enable(bool bEnable);
void Audio_Output_Init(void);
void Audio_Output_UnInit(void);
void Audio_Output_Queue(Uint8* data, int len);
void Audio_Output_Queue_Clear(void);
Uint32 Audio_Output_Queue_Size(void);

void Audio_Input_Enable(bool bEnable);
void Audio_Input_Init(void);
void Audio_Input_UnInit(void);
void Audio_Input_Lock(void);
int  Audio_Input_Read(void);
int  Audio_Input_BufSize(void);
void Audio_Input_Unlock(void);
