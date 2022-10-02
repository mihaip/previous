extern void     Audio_Output_Enable(bool bEnable);
extern void     Audio_Output_Init(void);
extern void     Audio_Output_UnInit(void);
extern void     Audio_Output_Queue(uint8_t* data, int len);
extern void     Audio_Output_Queue_Clear(void);
extern uint32_t Audio_Output_Queue_Size(void);

extern void     Audio_Input_Enable(bool bEnable);
extern void     Audio_Input_Init(void);
extern void     Audio_Input_UnInit(void);
extern void     Audio_Input_Lock(void);
extern int      Audio_Input_Read(int16_t* sample);
extern int      Audio_Input_BufSize(void);
extern void     Audio_Input_Unlock(void);
