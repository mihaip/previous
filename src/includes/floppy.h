void FLP_StatA_Read(void);
void FLP_StatB_Read(void);
void FLP_DataOut_Read(void);
void FLP_DataOut_Write(void);
void FLP_MainStatus_Read(void);
void FLP_DataRate_Write(void);
void FLP_FIFO_Read(void);
void FLP_FIFO_Write(void);
void FLP_DataIn_Read(void);
void FLP_Configuration_Write(void);
void FLP_Status_Read(void);
void FLP_Control_Write(void);

void FLP_IO_Handler(void);

void Floppy_Reset(void);
int Floppy_Insert(int drive);
void Floppy_Eject(int drive);

typedef struct {
    uint8_t data[1024];
    uint32_t size;
    uint32_t limit;
} FloppyBuffer;

extern FloppyBuffer flp_buffer;

extern uint8_t floppy_select;
void set_floppy_select(uint8_t sel, bool osp);
