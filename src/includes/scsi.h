/* SCSI Bus and Disk emulation */

/* SCSI phase */
#define PHASE_DO      0x00 /* data out */
#define PHASE_DI      0x01 /* data in */
#define PHASE_CD      0x02 /* command */
#define PHASE_ST      0x03 /* status */
#define PHASE_MO      0x06 /* message out */
#define PHASE_MI      0x07 /* message in */

typedef struct {
    uint8_t target;
    uint8_t phase;
} SCSIBusStatus;

extern SCSIBusStatus SCSIbus;


/* Command Descriptor Block */
#define SCSI_CDB_MAX_SIZE 12

/* Block size */
#define SCSI_BLOCKSIZE 512
#define SCSI_MAX_BLOCK 1024

/* This buffer temporarily stores data to be written to memory or disk */

typedef struct {
    uint8_t data[SCSI_MAX_BLOCK];
    int limit;
    int size;
    bool disk;
    int64_t time;
} SCSIBuffer;

extern SCSIBuffer scsi_buffer;

extern void SCSI_Reset(void);
extern void SCSI_Insert(uint8_t target);
extern void SCSI_Eject(uint8_t target);

extern uint8_t SCSIdisk_Send_Status(void);
extern uint8_t SCSIdisk_Send_Message(void);
extern uint8_t SCSIdisk_Send_Data(void);
extern void SCSIdisk_Receive_Data(uint8_t val);
extern bool SCSIdisk_Select(uint8_t target);
extern void SCSIdisk_Receive_Command(uint8_t *commandbuf, uint8_t identify);

extern int64_t SCSIdisk_Time(void);
