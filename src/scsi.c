/* SCSI Bus and Disk emulation */
#include "main.h"
#include "ioMem.h"
#include "ioMemTables.h"
#include "configuration.h"
#include "sysdeps.h"
#include "m68000.h"
#include "statusbar.h"
#include "scsi.h"
#include "file.h"

#define LOG_SCSI_LEVEL  LOG_DEBUG    /* Print debugging messages */


#define COMMAND_ReadInt16(a, i) (((unsigned) a[i] << 8) | a[i + 1])
#define COMMAND_ReadInt24(a, i) (((unsigned) a[i] << 16) | ((unsigned) a[i + 1] << 8) | a[i + 2])
#define COMMAND_ReadInt32(a, i) (((unsigned) a[i] << 24) | ((unsigned) a[i + 1] << 16) | ((unsigned) a[i + 2] << 8) | a[i + 3])


#define LUN_DISK 0 // for now only LUN 0 is valid for our phys drives

/* Status Codes */
#define STAT_GOOD           0x00
#define STAT_CHECK_COND     0x02
#define STAT_COND_MET       0x04
#define STAT_BUSY           0x08
#define STAT_INTERMEDIATE   0x10
#define STAT_INTER_COND_MET 0x14
#define STAT_RESERV_CONFL   0x18

/* Messages */
#define MSG_COMPLETE        0x00
#define MSG_SAVE_PTRS       0x02
#define MSG_RESTORE_PTRS    0x03
#define MSG_DISCONNECT      0x04
#define MSG_INITIATOR_ERR   0x05
#define MSG_ABORT           0x06
#define MSG_MSG_REJECT      0x07
#define MSG_NOP             0x08
#define MSG_PARITY_ERR      0x09
#define MSG_LINK_CMD_CMPLT  0x0A
#define MSG_LNKCMDCMPLTFLAG 0x0B
#define MSG_DEVICE_RESET    0x0C

#define MSG_IDENTIFY_MASK   0x80
#define MSG_ID_DISCONN      0x40
#define MSG_LUNMASK         0x07

/* Sense Keys */
#define SK_NOSENSE          0x00
#define SK_RECOVERED        0x01
#define SK_NOTREADY         0x02
#define SK_MEDIA            0x03
#define SK_HARDWARE         0x04
#define SK_ILLEGAL_REQ      0x05
#define SK_UNIT_ATN         0x06
#define SK_DATAPROTECT      0x07
#define SK_ABORTED_CMD      0x0B
#define SK_VOL_OVERFLOW     0x0D
#define SK_MISCOMPARE       0x0E

/* Additional Sense Codes */
#define SC_NO_ERROR         0x00    // 0
#define SC_NO_SECTOR        0x01    // 4
#define SC_WRITE_FAULT      0x03    // 5
#define SC_NOT_READY        0x04    // 2
#define SC_INVALID_CMD      0x20    // 5
#define SC_INVALID_LBA      0x21    // 5
#define SC_INVALID_CDB      0x24    // 5
#define SC_INVALID_LUN      0x25    // 5
#define SC_WRITE_PROTECT    0x27    // 7
#define SC_SAVE_UNSUPP      0x39    // 5


/* SCSI Commands */

/* The following are multi-sector transfers with seek implied */
#define CMD_VERIFY_TRACK    0x05    /* Verify track */
#define CMD_FORMAT_TRACK    0x06    /* Format track */
#define CMD_READ_SECTOR     0x08    /* Read sector */
#define CMD_READ_SECTOR1    0x28    /* Read sector (class 1) */
#define CMD_WRITE_SECTOR    0x0A    /* Write sector */
#define CMD_WRITE_SECTOR1   0x2A    /* Write sector (class 1) */

/* Other codes */
#define CMD_TEST_UNIT_RDY   0x00    /* Test unit ready */
#define CMD_FORMAT_DRIVE    0x04    /* Format the whole drive */
#define CMD_REASSIGN        0x07    /* Reassign defective blocks */
#define CMD_SEEK            0x0B    /* Seek */
#define CMD_CORRECTION      0x0D    /* Correction */
#define CMD_INQUIRY         0x12    /* Inquiry */
#define CMD_MODESELECT      0x15    /* Mode select */
#define CMD_MODESENSE       0x1A    /* Mode sense */
#define CMD_REQ_SENSE       0x03    /* Request sense */
#define CMD_SHIP            0x1B    /* Ship drive */
#define CMD_READ_CAPACITY1  0x25    /* Read capacity (class 1) */


/* Externally accessible */
SCSIBusStatus SCSIbus;
SCSIBuffer scsi_buffer;


/* SCSI disk */
struct {
    SCSI_DEVTYPE devtype;
    FILE* dsk;
    uint64_t size;
    uint32_t blocksize;
    bool readonly;
    uint8_t lun;
    uint8_t status;
    uint8_t message;
    
    struct {
        uint8_t key;
        uint8_t code;
        bool valid;
        uint32_t info;
    } sense;
    
    uint32_t lba;
    uint32_t blockcounter;
    uint32_t lastlba;
    
    int known;
    uint8_t** shadow;
} SCSIdisk[ESP_MAX_DEVS];


/* INQUIRY response data */
#define DEVTYPE_DISK        0x00    /* read/write disks */
#define DEVTYPE_TAPE        0x01    /* tapes and other sequential devices */
#define DEVTYPE_PRINTER     0x02    /* printers */
#define DEVTYPE_PROCESSOR   0x03    /* cpus */
#define DEVTYPE_WORM        0x04    /* write-once optical disks */
#define DEVTYPE_READONLY    0x05    /* cd-roms */
#define DEVTYPE_NOTPRESENT  0x7f    /* logical unit not present */


static const uint8_t inquiry_bytes[54] =
{
    0x00,             /* 0: device type: see above */
    0x00,             /* 1: &0x7F - device type qualifier 0x00 unsupported, &0x80 - rmb: 0x00 = nonremovable, 0x80 = removable */
    0x01,             /* 2: ANSI SCSI standard (first release) compliant */
    0x01,             /* 3: Response format (format of following data): 0x01 SCSI-1 compliant */
    0x31,             /* 4: additional length of the following data */
    0x00,             /* 5: reserved */
    0x00,             /* 6: reserved */
    0x1C,             /* 7: RelAdr=0, Wbus32=0, Wbus16=0, Sync=1, Linked=1, RSVD=1, CmdQue=0, SftRe=0 */
    'P','r','e','v','i','o','u','s',        /*  8-15: Vendor ASCII */
    'H','D','D',' ',' ',' ',' ',' ',        /* 16-31: Model ASCII */
    ' ',' ',' ',' ',' ',' ',' ',' ',        /*        (unused data is filled with spaces) */
    'B',  0,  0,  0,  0,  0,  0,  0,        /* 32-55: Revision ASCII (null-terminated) */
      0,  0,  0,  0,  0,  0,  0,  0,        /*        (may also contain serial number) */
      0,  0,  0,  0,  0,  0                 /*        (may be shorter than 24 bytes) */
};

struct known_disk {
    char vend[8];
    char name[16];
    char vers[24];
    uint8_t len;
    uint32_t c;
    uint32_t h;
    uint32_t s;
    uint32_t bs;
};

#define KNOWN_SIZE(x,n) (x[n].c * x[n].h * x[n].s * x[n].bs)

static const struct known_disk known_disks[] =
{
    { "MAXTOR", "XT-8380S", "B3C", 31, 1626, 10, 22, 1024 },
    { "MAXTOR", "XT-8760S", "B3C", 31, 1626, 16, 26, 1024 }
};


/* Helpers */
static int SCSI_LookupDisk(int target) {
    int i;
    off_t size = File_Length(ConfigureParams.SCSI.target[target].szImageName);
    
    SCSIdisk[target].size = (size < 0) ? 0 : size;
    
    if (SCSIdisk[target].devtype == DEVTYPE_HARDDISK) {
        for (i = 0; i < ARRAY_SIZE(known_disks); i++) {
            if (KNOWN_SIZE(known_disks, i) == size) {
                if (known_disks[i].bs <= SCSI_MAX_BLOCK) {
                    Log_Printf(LOG_WARN, "[SCSI] Known disk found (%s %s)", known_disks[i].vend, known_disks[i].name);
                    SCSIdisk[target].blocksize = known_disks[i].bs;
                    return i;
                }
            }
        }       
    }
    return -1;
}

static int SCSI_GetCommandLength(uint8_t opcode) {
    uint8_t group_code = (opcode&0xE0)>>5;
    switch (group_code) {
        case 0: return 6;
        case 1: return 10;
        case 5: return 12;
        default:
            Log_Printf(LOG_WARN, "[SCSI] Unimplemented Group Code!");
            return 6;
    }
}

static int SCSI_GetTransferLength(uint8_t opcode, uint8_t *cdb)
{
    return opcode < 0x20?
    // class 0
    cdb[4] :
    // class 1
    COMMAND_ReadInt16(cdb, 7);
}

static uint64_t SCSI_GetOffset(uint8_t opcode, uint8_t *cdb)
{
    return opcode < 0x20?
    // class 0
    (COMMAND_ReadInt24(cdb, 1) & 0x1FFFFF) :
    // class 1
    COMMAND_ReadInt32(cdb, 2);
}

// get reserved count for SCSI reply
static int SCSI_GetCount(uint8_t opcode, uint8_t *cdb)
{
    return opcode < 0x20?
    // class 0
    ((cdb[4]==0)?0x100:cdb[4]) :
    // class 1
    COMMAND_ReadInt16(cdb, 7);
}

static void SCSI_GuessGeometry(uint32_t size, uint32_t *cylinders, uint32_t *heads, uint32_t *sectors)
{
    uint32_t c,h,s;
    
    for (h=16; h>0; h--) {
        for (s=63; s>15; s--) {
            if ((size%(s*h))==0) {
                c=size/(s*h);
                *cylinders=c;
                *heads=h;
                *sectors=s;
                return;
            }
        }
    }
    Log_Printf(LOG_SCSI_LEVEL, "[SCSI] Disk geometry: No valid geometry found! Using default.");
    
    h=16;
    s=63;
    c=size/(s*h);
    if ((size%(s*h))!=0) {
        c+=1;
    }
    *cylinders=c;
    *heads=h;
    *sectors=s;
}

#define SCSI_SEEK_TIME_HD       20000  /* 20 ms max seek time */
#define SCSI_SECTOR_TIME_HD     350    /* 1.4 MB/sec */
#define SCSI_SEEK_TIME_FD       200000 /* 200 ms max seek time */
#define SCSI_SECTOR_TIME_FD     5500   /* 90 kB/sec */
#define SCSI_SEEK_TIME_CD       500000 /* 500 ms max seek time */
#define SCSI_SECTOR_TIME_CD     3250   /* 150 kB/sec */

static int64_t SCSI_GetTime(uint8_t target) {
    int64_t seektime, sectortime;
    int64_t seekoffset, disksize, sectors;
    
    switch (SCSIdisk[target].devtype) {
        case DEVTYPE_HARDDISK:
            seektime = SCSI_SEEK_TIME_HD;
            sectortime = SCSI_SECTOR_TIME_HD;
            break;
        case DEVTYPE_CD:
            seektime = SCSI_SEEK_TIME_CD;
            sectortime = SCSI_SECTOR_TIME_CD;
            break;
        case DEVTYPE_FLOPPY:
            seektime = SCSI_SEEK_TIME_FD;
            sectortime = SCSI_SECTOR_TIME_FD;
            break;
        default:
            return 1000;
    }
    
    if (SCSIdisk[target].lba < SCSIdisk[target].lastlba) {
        seekoffset = SCSIdisk[target].lastlba - SCSIdisk[target].lba;
    } else {
        seekoffset = SCSIdisk[target].lba - SCSIdisk[target].lastlba;
    }
    disksize = SCSIdisk[target].size / SCSIdisk[target].blocksize;
    
    if (disksize < 1) { /* make sure no zero divide occurs */
        disksize = 1;
    }
    seektime *= seekoffset;
    seektime /= disksize;
    
    if (seektime > 500000) {
        seektime = 500000;
    }
    
    sectors = SCSIdisk[target].blockcounter;
    if (sectors < 1) {
        sectors = 1;
    }
    
    sectortime *= sectors;

    return seektime + sectortime;
}

static int SCSI_GetModePage(uint8_t* page, uint8_t pagecode) {
    uint8_t target = SCSIbus.target;
    
    int pagesize = 0;
    
    switch (pagecode) {
        case 0x00: // operating page
            pagesize = 4;
            page[0] = 0x00; // &0x80: page savable? (not supported!), &0x7F: page code = 0x00
            page[1] = 0x02; // page length = 2
            page[2] = 0x80; // &0x80: usage bit = 1, &0x10: disable unit attention = 0
            page[3] = 0x00; // &0x7F: device type qualifier = 0x00, see inquiry!
            break;
            
        case 0x01: // error recovery page
            pagesize = 8;
            page[0] = 0x01; // &0x80: page savable? (not supported!), &0x7F: page code = 0x01
            page[1] = 0x06; // page length = 6
            page[2] = 0x00; // AWRE, ARRE, TB, RC, EER, PER, DTE, DCR
            page[3] = 0x1B; // retry count
            page[4] = 0x0B; // correction span in bits
            page[5] = 0x00; // head offset count
            page[6] = 0x00; // data strobe offset count
            page[7] = 0xFF; // recovery time limit
            break;
            
        case 0x03: // format device page
            
            Log_Printf(LOG_WARN, "[SCSI] Mode sense: Format device page not supported!");

            pagesize = 24;
            memset(page, 0, pagesize);
            break;
            
        case 0x04: // rigid disc geometry page
        {
            uint32_t c, h, s;
            
            int i = SCSIdisk[target].known;
            uint32_t num_sectors = SCSIdisk[target].size / SCSIdisk[target].blocksize;
            
            if (i < 0) {
                SCSI_GuessGeometry(num_sectors, &c, &h, &s);
            } else {
                c = known_disks[i].c;
                h = known_disks[i].h;
                s = known_disks[i].s;
            }
            
            Log_Printf(LOG_SCSI_LEVEL, "[SCSI] Disk geometry: %i cylinders, %i heads, %i sectors", c, h, s);
            
            pagesize = 20;
            page[0] = 0x04; // &0x80: page savable? (not supported!), &0x7F: page code = 0x04
            page[1] = 0x12;
            page[2] = (c >> 16) & 0xFF;
            page[3] = (c >> 8) & 0xFF;
            page[4] = c & 0xFF;
            page[5] = h;
            page[6] = 0x00; // 6,7,8: starting cylinder - write precomp (not supported)
            page[7] = 0x00;
            page[8] = 0x00;
            page[9] = 0x00; // 9,10,11: starting cylinder - reduced write current (not supported)
            page[10] = 0x00;
            page[11] = 0x00;
            page[12] = 0x00; // 12,13: drive step rate (not supported)
            page[13] = 0x00;
            page[14] = 0x00; // 14,15,16: loading zone cylinder (not supported)
            page[15] = 0x00;
            page[16] = 0x00;
            page[17] = 0x00; // &0x03: rotational position locking
            page[18] = 0x00; // rotational position lock offset
            page[19] = 0x00; // reserved
        }
            break;
            
        case 0x02: // disconnect/reconnect page
        case 0x08: // caching page
        case 0x0C: // notch page
        case 0x0D: // power condition page
        case 0x38: // cache control page
        case 0x3C: // soft ID page (EEPROM)
            Log_Printf(LOG_WARN, "[SCSI] Mode sense: Page %02x not yet emulated!", pagecode);
            break;
            
        default:
            Log_Printf(LOG_WARN, "[SCSI] Mode sense: Invalid page code: %02x!", pagecode);
            break;
    }
    return pagesize;
}


/* SCSI commands */
static void SCSI_TestUnitReady(uint8_t *cdb) {
    uint8_t target = SCSIbus.target;
    
    if (SCSIdisk[target].devtype!=DEVTYPE_NONE &&
        SCSIdisk[target].devtype!=DEVTYPE_HARDDISK &&
        SCSIdisk[target].dsk==NULL) { /* Empty drive */
        SCSIdisk[target].status = STAT_CHECK_COND;
        SCSIdisk[target].sense.key = SK_NOTREADY;
        SCSIdisk[target].sense.code = SC_NOT_READY;
        SCSIdisk[target].sense.valid = false;
    } else {
        SCSIdisk[target].status = STAT_GOOD;
        SCSIdisk[target].sense.key = SK_NOSENSE;
        SCSIdisk[target].sense.code = SC_NO_ERROR;
        SCSIdisk[target].sense.valid = false;
    }
    SCSIbus.phase = PHASE_ST;
}

static void SCSI_ReadCapacity(uint8_t *cdb) {
    uint8_t target = SCSIbus.target;
    
    uint32_t blocksize = SCSIdisk[target].blocksize;
    uint32_t sectors = (SCSIdisk[target].size / blocksize) - 1; /* last LBA */
    
    Log_Printf(LOG_SCSI_LEVEL, "[SCSI] Read capacity: %d sectors (blocksize: %d)", sectors, blocksize);
    
    scsi_buffer.data[0] = (sectors >> 24) & 0xFF;
    scsi_buffer.data[1] = (sectors >> 16) & 0xFF;
    scsi_buffer.data[2] = (sectors >> 8) & 0xFF;
    scsi_buffer.data[3] = sectors & 0xFF;
    scsi_buffer.data[4] = (blocksize >> 24) & 0xFF;
    scsi_buffer.data[5] = (blocksize >> 16) & 0xFF;
    scsi_buffer.data[6] = (blocksize >> 8) & 0xFF;
    scsi_buffer.data[7] = blocksize & 0xFF;
    
    scsi_buffer.size = scsi_buffer.limit = 8;
    scsi_buffer.disk = false;
    scsi_buffer.time = 100;
    
    SCSIdisk[target].status = STAT_GOOD;
    SCSIdisk[target].sense.key = SK_NOSENSE;
    SCSIdisk[target].sense.code = SC_NO_ERROR;
    SCSIdisk[target].sense.valid = false;
    
    SCSIbus.phase = PHASE_DI;
}

static void scsi_write_sector(void) {
    uint8_t target = SCSIbus.target;
    uint64_t offset = 0;
    
    Log_Printf(LOG_SCSI_LEVEL, "[SCSI] Writing block at offset %i (%i blocks remaining).",
               SCSIdisk[target].lba,SCSIdisk[target].blockcounter-1);
    
    offset = ((uint64_t)SCSIdisk[target].lba)*SCSIdisk[target].blocksize;
    
    if (offset < SCSIdisk[target].size) {
        if (ConfigureParams.SCSI.nWriteProtection != WRITEPROT_ON) {
            File_Write(scsi_buffer.data, SCSIdisk[target].blocksize, offset, SCSIdisk[target].dsk);
        } else {
            Log_Printf(LOG_SCSI_LEVEL, "[SCSI] WARNING: File write disabled!");
            if(SCSIdisk[target].shadow) {
                if(!(SCSIdisk[target].shadow[SCSIdisk[target].lba]))
                    SCSIdisk[target].shadow[SCSIdisk[target].lba] = malloc(SCSIdisk[target].blocksize);
                memcpy(SCSIdisk[target].shadow[SCSIdisk[target].lba], scsi_buffer.data, SCSIdisk[target].blocksize);
            } else {
                uint32_t blocks = SCSIdisk[target].size / SCSIdisk[target].blocksize;
                SCSIdisk[target].shadow = malloc(sizeof(uint8_t*) * blocks);
                for(int i = blocks; --i >= 0;)
                    SCSIdisk[target].shadow[i] = NULL;
            }
        }
        scsi_buffer.size = 0;
        scsi_buffer.limit = SCSIdisk[target].blocksize;
        
        SCSIdisk[target].lba++;
        SCSIdisk[target].blockcounter--;
        if (SCSIdisk[target].blockcounter==0) {
            SCSIbus.phase = PHASE_ST;
        }
    } else {
        SCSIdisk[target].status = STAT_CHECK_COND;
        SCSIdisk[target].sense.key = SK_ILLEGAL_REQ;
        SCSIdisk[target].sense.code = SC_INVALID_LBA;
        SCSIdisk[target].sense.info = SCSIdisk[target].lba;
        SCSIdisk[target].sense.valid = true;
        
        SCSIbus.phase = PHASE_ST;
    }
}
static void SCSI_WriteSector(uint8_t *cdb) {
    uint8_t target = SCSIbus.target;
    
    SCSIdisk[target].lastlba = SCSIdisk[target].lba;
    SCSIdisk[target].lba = SCSI_GetOffset(cdb[0], cdb);
    SCSIdisk[target].blockcounter = SCSI_GetCount(cdb[0], cdb);
    
    if (SCSIdisk[target].readonly) {
        Log_Printf(LOG_SCSI_LEVEL, "[SCSI] Write sector: Disk is write protected! Check condition.");
        SCSIdisk[target].status = STAT_CHECK_COND;
        SCSIdisk[target].sense.key = SK_DATAPROTECT;
        SCSIdisk[target].sense.code = SC_WRITE_PROTECT;
        SCSIdisk[target].sense.valid = false;
        SCSIbus.phase = PHASE_ST;
        return;
    }
    scsi_buffer.size = 0;
    scsi_buffer.limit = SCSIdisk[target].blocksize;
    scsi_buffer.disk = true;
    scsi_buffer.time = SCSI_GetTime(target);
    
    SCSIdisk[target].status = STAT_GOOD;
    SCSIdisk[target].sense.key = SK_NOSENSE;
    SCSIdisk[target].sense.code = SC_NO_ERROR;
    SCSIdisk[target].sense.valid = false;
    
    SCSIbus.phase = SCSIdisk[target].blockcounter ? PHASE_DO : PHASE_ST;
    
    Log_Printf(LOG_SCSI_LEVEL, "[SCSI] Write sector: %i block(s) at offset %i (blocksize: %i byte)",
               SCSIdisk[target].blockcounter, SCSIdisk[target].lba, SCSIdisk[target].blocksize);
}

static void scsi_read_sector(void) {
    uint8_t target = SCSIbus.target;
    uint64_t offset = 0;
    
    if (SCSIdisk[target].blockcounter==0) {
        SCSIbus.phase = PHASE_ST;
        return;
    }
    
    Log_Printf(LOG_SCSI_LEVEL, "[SCSI] Reading block at offset %i (%i blocks remaining).",
               SCSIdisk[target].lba,SCSIdisk[target].blockcounter-1);
    
    offset = ((uint64_t)SCSIdisk[target].lba)*SCSIdisk[target].blocksize;
    
    if (offset < SCSIdisk[target].size) {
        if (SCSIdisk[target].shadow && SCSIdisk[target].shadow[SCSIdisk[target].lba]) {
            memcpy(scsi_buffer.data, SCSIdisk[target].shadow[SCSIdisk[target].lba], SCSIdisk[target].blocksize);
        } else {
            File_Read(scsi_buffer.data, SCSIdisk[target].blocksize, offset, SCSIdisk[target].dsk);
        }
        scsi_buffer.size = scsi_buffer.limit = SCSIdisk[target].blocksize;
        
        SCSIdisk[target].lba++;
        SCSIdisk[target].blockcounter--;
    } else {
        SCSIdisk[target].status = STAT_CHECK_COND;
        SCSIdisk[target].sense.key = SK_ILLEGAL_REQ;
        SCSIdisk[target].sense.code = SC_INVALID_LBA;
        SCSIdisk[target].sense.info = SCSIdisk[target].lba;
        SCSIdisk[target].sense.valid = true;
        
        SCSIbus.phase = PHASE_ST;
    }
}
static void SCSI_ReadSector(uint8_t *cdb) {
    uint8_t target = SCSIbus.target;
    
    SCSIdisk[target].lastlba = SCSIdisk[target].lba;
    SCSIdisk[target].lba = SCSI_GetOffset(cdb[0], cdb);
    SCSIdisk[target].blockcounter = SCSI_GetCount(cdb[0], cdb);
    scsi_buffer.size = scsi_buffer.limit = 0;
    scsi_buffer.disk = true;
    scsi_buffer.time = SCSI_GetTime(target);
    
    SCSIdisk[target].status = STAT_GOOD;
    SCSIdisk[target].sense.key = SK_NOSENSE;
    SCSIdisk[target].sense.code = SC_NO_ERROR;
    SCSIdisk[target].sense.valid = false;

    SCSIbus.phase = SCSIdisk[target].blockcounter ? PHASE_DI : PHASE_ST;
    
    Log_Printf(LOG_SCSI_LEVEL, "[SCSI] Read sector: %i block(s) at offset %i (blocksize: %i byte)",
               SCSIdisk[target].blockcounter, SCSIdisk[target].lba, SCSIdisk[target].blocksize);
    
    scsi_read_sector();
}

static void SCSI_Inquiry(uint8_t *cdb) {
    uint8_t target = SCSIbus.target;
    
    int l = 0;
    int i = SCSIdisk[target].known;
    int len = SCSI_GetTransferLength(cdb[0], cdb);
    
    memset(scsi_buffer.data, 0, sizeof(scsi_buffer.data));
    
    if (i < 0) {
        l = sizeof(inquiry_bytes);
        memcpy(scsi_buffer.data, inquiry_bytes, sizeof(inquiry_bytes));
        
        switch (SCSIdisk[target].devtype) {
            case DEVTYPE_HARDDISK:
                scsi_buffer.data[0] = DEVTYPE_DISK;
                scsi_buffer.data[1] &= ~0x80;
                scsi_buffer.data[16] = 'H';
                scsi_buffer.data[17] = 'D';
                scsi_buffer.data[18] = 'D';
                scsi_buffer.data[19] = ' ';
                scsi_buffer.data[20] = ' ';
                scsi_buffer.data[21] = ' ';
                Log_Printf(LOG_SCSI_LEVEL, "[SCSI] Disk is HDD");
                break;
            case DEVTYPE_CD:
                scsi_buffer.data[0] = DEVTYPE_READONLY;
                scsi_buffer.data[1] |= 0x80;
                scsi_buffer.data[16] = 'C';
                scsi_buffer.data[17] = 'D';
                scsi_buffer.data[18] = '-';
                scsi_buffer.data[19] = 'R';
                scsi_buffer.data[20] = 'O';
                scsi_buffer.data[21] = 'M';
                Log_Printf(LOG_SCSI_LEVEL, "[SCSI] Disk is CD-ROM");
                break;
            case DEVTYPE_FLOPPY:
                scsi_buffer.data[0] = DEVTYPE_DISK;
                scsi_buffer.data[1] |= 0x80;
                scsi_buffer.data[16] = 'F';
                scsi_buffer.data[17] = 'L';
                scsi_buffer.data[18] = 'O';
                scsi_buffer.data[19] = 'P';
                scsi_buffer.data[20] = 'P';
                scsi_buffer.data[21] = 'Y';
                Log_Printf(LOG_SCSI_LEVEL, "[SCSI] Disk is Floppy");
                break;
                
            default:
                break;
        }
    } else {
        l = 5 + known_disks[i].len;
        scsi_buffer.data[0] = DEVTYPE_DISK;
        scsi_buffer.data[1] = 0x00;
        scsi_buffer.data[2] = 0x01;
        scsi_buffer.data[3] = 0x01;
        scsi_buffer.data[4] = known_disks[i].len;
        scsi_buffer.data[5] = scsi_buffer.data[6] = scsi_buffer.data[7] = 0x00;
        memset(scsi_buffer.data +  8, 0x20, 8 + 16);
        memcpy(scsi_buffer.data +  8, known_disks[i].vend, strlen(known_disks[i].vend));
        memcpy(scsi_buffer.data + 16, known_disks[i].name, strlen(known_disks[i].name));
        memcpy(scsi_buffer.data + 32, known_disks[i].vers, strlen(known_disks[i].vers));
    }
    
    if (SCSIdisk[target].lun != LUN_DISK) {
        scsi_buffer.data[0] = DEVTYPE_NOTPRESENT;
    }
    
    if (l > len) {
        Log_Printf(LOG_WARN, "[SCSI] Inquiry: Allocated length is short (len = %d, max = %d)!", l, len);
    } else if (l < len) {
        len = l;
    }
    Log_Printf(LOG_SCSI_LEVEL, "[SCSI] Inquiry: Data length: %d", len);
    Log_Printf(LOG_SCSI_LEVEL, "[SCSI] Inquiry Data: %c,%c,%c,%c,%c,%c,%c,%c",scsi_buffer.data[8],
               scsi_buffer.data[9],scsi_buffer.data[10],scsi_buffer.data[11],scsi_buffer.data[12],
               scsi_buffer.data[13],scsi_buffer.data[14],scsi_buffer.data[15]);
    
    scsi_buffer.size = scsi_buffer.limit = len;
    scsi_buffer.disk = false;
    scsi_buffer.time = 100;

    SCSIdisk[target].status = STAT_GOOD;
    SCSIdisk[target].sense.key = SK_NOSENSE;
    SCSIdisk[target].sense.code = SC_NO_ERROR;
    SCSIdisk[target].sense.valid = false;
    
    SCSIbus.phase = scsi_buffer.size ? PHASE_DI : PHASE_ST;
}

static void SCSI_StartStop(uint8_t *cdb) {
    uint8_t target = SCSIbus.target;
    
    switch (cdb[4]&0x03) {
        case 0:
            Log_Printf(LOG_SCSI_LEVEL, "[SCSI] Stop disk %i", target);
            break;
        case 1:
            Log_Printf(LOG_SCSI_LEVEL, "[SCSI] Start disk %i", target);
            break;
        case 2:
            Log_Printf(LOG_WARN, "[SCSI] Eject disk %i", target);
            if (SCSIdisk[target].devtype != DEVTYPE_HARDDISK) {
                SCSI_Eject(target);
                ConfigureParams.SCSI.target[target].bDiskInserted = false;
                ConfigureParams.SCSI.target[target].szImageName[0] = '\0';
                Statusbar_AddMessage("Ejecting SCSI media.", 0);
            }
            break;
        default:
            Log_Printf(LOG_WARN, "[SCSI] Invalid start/stop");
            break;
    }
    
    SCSIdisk[target].status = STAT_GOOD;
    SCSIdisk[target].sense.key = SK_NOSENSE;
    SCSIdisk[target].sense.code = SC_NO_ERROR;
    SCSIdisk[target].sense.valid = false;
    
    SCSIbus.phase = PHASE_ST;
}

static void SCSI_RequestSense(uint8_t *cdb) {
    uint8_t target = SCSIbus.target;
    
    int len = SCSI_GetTransferLength(cdb[0], cdb);
    
    if (len<4 && len!=0) {
        Log_Printf(LOG_WARN, "[SCSI] Request sense: Strange request size (%d)!",len);
    }
    
    /* Limit to sane length */
    if (len <= 0) {
        len = 4;
    } else if (len > 22) {
        len = 22;
    }
    
    Log_Printf(LOG_WARN, "[SCSI] Request sense: size = %d, code = %02x, key = %02x", 
               len, SCSIdisk[target].sense.code, SCSIdisk[target].sense.key);
    
    memset(scsi_buffer.data, 0, len);
    
    scsi_buffer.data[0] = 0x70;
    scsi_buffer.data[2] = SCSIdisk[target].sense.key;
    scsi_buffer.data[7] = 14;
    scsi_buffer.data[12] = SCSIdisk[target].sense.code;
    if (SCSIdisk[target].sense.valid) {
        scsi_buffer.data[0] |= 0x80;
        scsi_buffer.data[3] = (SCSIdisk[target].sense.info >> 24) & 0xFF;
        scsi_buffer.data[4] = (SCSIdisk[target].sense.info >> 16) & 0xFF;
        scsi_buffer.data[5] = (SCSIdisk[target].sense.info >> 8) & 0xFF;
        scsi_buffer.data[6] = SCSIdisk[target].sense.info & 0xFF;
    }
    
    scsi_buffer.size = scsi_buffer.limit = len;
    scsi_buffer.disk = false;
    scsi_buffer.time = 100;
    
    SCSIdisk[target].status = STAT_GOOD;
    
    SCSIbus.phase = PHASE_DI;
}

static void SCSI_ModeSense(uint8_t *cdb) {
    uint8_t target = SCSIbus.target;
    
    int len = 0;
    int maxlen = SCSI_GetTransferLength(cdb[0], cdb);
    int pagesize = 0;
    
    uint32_t blocksize = SCSIdisk[target].blocksize;
    uint32_t sectors = SCSIdisk[target].size / blocksize;
    
    uint8_t pagecontrol = (cdb[2] & 0xC0) >> 6;
    uint8_t pagecode = cdb[2] & 0x3F;
    uint8_t dbd = cdb[1] & 0x08; // disable block descriptor
    
    Log_Printf(LOG_WARN, "[SCSI] Mode sense: page = %02x, page_control = %i, %s", pagecode, pagecontrol, dbd == 0x08 ? "block descriptor disabled" : "block descriptor enabled");
    
    /* Header */
    scsi_buffer.data[0] = 0x00; // length of following data
    scsi_buffer.data[1] = 0x00; // medium type (always 0)
    scsi_buffer.data[2] = SCSIdisk[target].readonly ? 0x80 : 0x00; // if media is read-only 0x80, else 0x00
    scsi_buffer.data[3] = 0x08; // block descriptor length
    len = 4;
    
    /* Block descriptor data */
    if (!dbd) {
        scsi_buffer.data[len+0] = 0x00;                     // Density code
        scsi_buffer.data[len+1] = (sectors >> 16) & 0xFF;   // Number of blocks, high
        scsi_buffer.data[len+2] = (sectors >> 8) & 0xFF;    // Number of blocks, med
        scsi_buffer.data[len+3] = sectors & 0xFF;           // Number of blocks, low
        scsi_buffer.data[len+4] = 0x00;                     // Reserved
        scsi_buffer.data[len+5] = (blocksize >> 16) & 0xFF; // Block size in bytes, high
        scsi_buffer.data[len+6] = (blocksize >> 8) & 0xFF;  // Block size in bytes, med
        scsi_buffer.data[len+7] = blocksize & 0xFF;         // Block size in bytes, low
        len += 8;
        Log_Printf(LOG_WARN, "[SCSI] Mode sense: Block descriptor data: %s, size = %i blocks, blocksize = %i byte",
                   SCSIdisk[target].readonly ? "disk is read-only" : "disk is read/write" , sectors, SCSIdisk[target].blocksize);
    }

    /* Check page control */
    switch (pagecontrol) {
        case 0: // current values (use default)
        case 2: // default values
            break;
        case 1: // changeable values (not supported)
            SCSIdisk[target].status = STAT_CHECK_COND;
            SCSIdisk[target].sense.key = SK_ILLEGAL_REQ;
            SCSIdisk[target].sense.code = SC_INVALID_CDB;
            SCSIdisk[target].sense.valid = false;
            SCSIbus.phase = PHASE_ST;
            return;
        case 3: // saved values (not supported)
            SCSIdisk[target].status = STAT_CHECK_COND;
            SCSIdisk[target].sense.key = SK_ILLEGAL_REQ;
            SCSIdisk[target].sense.code = SC_SAVE_UNSUPP;
            SCSIdisk[target].sense.valid = false;
            SCSIbus.phase = PHASE_ST;
            return;
        default:
            break;
    }

    /* Mode Pages */
    if (pagecode == 0x3F) {  // return all pages
        for (pagecode = 0x01; pagecode < 0x3F; pagecode++) {
            pagesize = SCSI_GetModePage(scsi_buffer.data+len, pagecode);
            len += pagesize;
        }
        pagesize = SCSI_GetModePage(scsi_buffer.data+len, 0x00);
        len += pagesize;
    } else {                 // return only single requested page
        pagesize = SCSI_GetModePage(scsi_buffer.data+len, pagecode);
        len += pagesize;
        
        if (pagesize == 0) { // unsupported page
            SCSIdisk[target].status = STAT_CHECK_COND;
            SCSIdisk[target].sense.key = SK_ILLEGAL_REQ;
            SCSIdisk[target].sense.code = SC_INVALID_CDB;
            SCSIdisk[target].sense.valid = false;
            SCSIbus.phase = PHASE_ST;
            return;
        }
    }
    
    scsi_buffer.data[0] = len - 1; // set reply length

    if (len > maxlen) {
        Log_Printf(LOG_WARN, "[SCSI] Mode sense: Allocated length is short (len = %d, max = %d)!", len, maxlen);
        len = maxlen;
    }
    
    scsi_buffer.size = scsi_buffer.limit = len;
    scsi_buffer.disk = false;
    scsi_buffer.time = 100;
    
    SCSIdisk[target].status = STAT_GOOD;
    SCSIdisk[target].sense.key = SK_NOSENSE;
    SCSIdisk[target].sense.code = SC_NO_ERROR;
    SCSIdisk[target].sense.valid = false;
    
    SCSIbus.phase = scsi_buffer.size ? PHASE_DI : PHASE_ST;
}

static void SCSI_FormatDrive(uint8_t *cdb) {
    uint8_t target = SCSIbus.target;

    uint8_t format_data = cdb[1]&0x10;
    
    Log_Printf(LOG_WARN, "[SCSI] Format drive command with parameters %02X",cdb[1]&0x1F);
    
    if (format_data) {
        Log_Printf(LOG_WARN, "[SCSI] Format drive with format data unsupported!");
        abort();
    } else {
        SCSIdisk[target].status = STAT_GOOD;
        SCSIdisk[target].sense.key = SK_NOSENSE;
        SCSIdisk[target].sense.code = SC_NO_ERROR;
        SCSIdisk[target].sense.valid = false;
        
        SCSIbus.phase = PHASE_ST;
    }
}

static void SCSI_ReassignBlocks(uint8_t *cdb) {
    uint8_t target = SCSIbus.target;
    
    Log_Printf(LOG_SCSI_LEVEL, "[SCSI] Reassign blocks");
    
    SCSIdisk[target].status = STAT_GOOD;
    SCSIdisk[target].sense.key = SK_NOSENSE;
    SCSIdisk[target].sense.code = SC_NO_ERROR;
    SCSIdisk[target].sense.valid = false;
    
    SCSIbus.phase = PHASE_ST;
}

static void SCSI_Command(uint8_t *cdb) {
    uint8_t opcode = cdb[0];
    uint8_t target = SCSIbus.target;
    
    /* First check for lun-independent commands */
    switch (opcode) {
        case CMD_INQUIRY:
            Log_Printf(LOG_SCSI_LEVEL, "SCSI command: Inquiry");
            SCSI_Inquiry(cdb);
            break;
        case CMD_REQ_SENSE:
            Log_Printf(LOG_SCSI_LEVEL, "SCSI command: Request sense");
            SCSI_RequestSense(cdb);
            break;
            /* Check if the specified lun is valid for our disk */
        default:
            if (SCSIdisk[target].lun!=LUN_DISK) {
                Log_Printf(LOG_SCSI_LEVEL, "SCSI command: Invalid lun! Check condition.");
                SCSIdisk[target].status = STAT_CHECK_COND;
                SCSIdisk[target].sense.key = SK_ILLEGAL_REQ;
                SCSIdisk[target].sense.code = SC_INVALID_LUN;
                SCSIdisk[target].sense.valid = false;
                SCSIdisk[target].message = MSG_COMPLETE;
                SCSIbus.phase = PHASE_ST;
                return;
            }
            
            /* Then check for lun-dependent commands */
            switch(opcode) {
                case CMD_TEST_UNIT_RDY:
                    Log_Printf(LOG_SCSI_LEVEL, "SCSI command: Test unit ready");
                    SCSI_TestUnitReady(cdb);
                    break;
                case CMD_READ_CAPACITY1:
                    Log_Printf(LOG_SCSI_LEVEL, "SCSI command: Read capacity");
                    SCSI_ReadCapacity(cdb);
                    break;
                case CMD_READ_SECTOR:
                case CMD_READ_SECTOR1:
                    Log_Printf(LOG_SCSI_LEVEL, "SCSI command: Read sector");
                    SCSI_ReadSector(cdb);
                    break;
                case CMD_WRITE_SECTOR:
                case CMD_WRITE_SECTOR1:
                    Log_Printf(LOG_SCSI_LEVEL, "SCSI command: Write sector");
                    SCSI_WriteSector(cdb);
                    break;
                case CMD_SEEK:
                    Log_Printf(LOG_WARN, "SCSI command: Seek");
                    abort();
                    break;
                case CMD_SHIP:
                    Log_Printf(LOG_SCSI_LEVEL, "SCSI command: Ship");
                    SCSI_StartStop(cdb);
                    break;
                case CMD_MODESELECT:
                    Log_Printf(LOG_WARN, "SCSI command: Mode select");
                    abort();
                    break;
                case CMD_MODESENSE:
                    Log_Printf(LOG_SCSI_LEVEL, "SCSI command: Mode sense");
                    SCSI_ModeSense(cdb);
                    break;
                case CMD_FORMAT_DRIVE:
                    Log_Printf(LOG_SCSI_LEVEL, "SCSI command: Format drive");
                    SCSI_FormatDrive(cdb);
                    break;
                case CMD_REASSIGN:
                    Log_Printf(LOG_SCSI_LEVEL, "SCSI command: Reassign blocks");
                    SCSI_ReassignBlocks(cdb);
                    break;
                    /* as of yet unsupported commands */
                case CMD_VERIFY_TRACK:
                case CMD_FORMAT_TRACK:
                case CMD_CORRECTION:
                default:
                    Log_Printf(LOG_WARN, "SCSI command: Unknown Command (%02X)",opcode);
                    SCSIdisk[target].status = STAT_CHECK_COND;
                    SCSIdisk[target].sense.key = SK_ILLEGAL_REQ;
                    SCSIdisk[target].sense.code = SC_INVALID_CMD;
                    SCSIdisk[target].sense.valid = false;
                    SCSIbus.phase = PHASE_ST;
                    break;
            }
            break;
    }
    
    SCSIdisk[target].message = MSG_COMPLETE;
    
    /* Update the led each time a command is processed */
    Statusbar_BlinkLed(DEVICE_LED_SCSI);
}


/* SCSI disk access */
bool SCSIdisk_Select(uint8_t target) {
    
    /* If there is no disk drive present, return timeout true */
    if (SCSIdisk[target].devtype==DEVTYPE_NONE) {
        Log_Printf(LOG_SCSI_LEVEL, "[SCSI] Selection timeout, target = %i", target);
        SCSIbus.phase = PHASE_ST;
        return true;
    } else {
        SCSIbus.target = target;
        return false;
    }
}

void SCSIdisk_Receive_Data(uint8_t val) {
    /* Receive one byte. If the transfer is complete, set status phase
     * and write the buffer contents to the disk. */
    scsi_buffer.data[scsi_buffer.size]=val;
    scsi_buffer.size++;
    if (scsi_buffer.size==scsi_buffer.limit) {
        if (scsi_buffer.disk==true) {
            scsi_write_sector();  /* sets status phase if done or error */
        } else {
            SCSIbus.phase = PHASE_ST;
        }
    }
}

uint8_t SCSIdisk_Send_Data(void) {
    /* Send one byte. If the transfer is complete, set status phase */
    uint8_t val=scsi_buffer.data[scsi_buffer.limit-scsi_buffer.size];
    scsi_buffer.size--;
    if (scsi_buffer.size==0) {
        if (scsi_buffer.disk==true) {
            scsi_read_sector(); /* sets status phase if done or error */
        } else {
            SCSIbus.phase = PHASE_ST;
        }
    }
    return val;
}

uint8_t SCSIdisk_Send_Status(void) {
    SCSIbus.phase = PHASE_MI;
    return SCSIdisk[SCSIbus.target].status;
}

uint8_t SCSIdisk_Send_Message(void) {
    return SCSIdisk[SCSIbus.target].message;
}

void SCSIdisk_Receive_Command(uint8_t *cdb, uint8_t identify) {
    uint8_t lun = 0;
    
    /* Get logical unit number */
    if (identify&MSG_IDENTIFY_MASK) { /* if identify message is valid */
        lun = identify&MSG_LUNMASK; /* use lun from identify message */
    } else {
        lun = (cdb[1]&0xE0)>>5; /* use lun specified in CDB */
    }
    
    SCSIdisk[SCSIbus.target].lun = lun;
    
    Log_Printf(LOG_SCSI_LEVEL, "SCSI command: Opcode = $%02x, target = %i, lun = %i", cdb[0], SCSIbus.target,lun);
    
    SCSI_Command(cdb);
}

int64_t SCSIdisk_Time(void) {
    int64_t scsitime = scsi_buffer.time;
    
    if (scsitime < 100) {
        scsitime = 100;
    }
    
    scsi_buffer.time = 0;
    
    return scsitime;
}


/* Insert/Eject SCSI disks */
void SCSI_Insert(uint8_t i) {
    SCSIdisk[i].devtype = ConfigureParams.SCSI.target[i].nDeviceType;
    if (SCSIdisk[i].devtype == DEVTYPE_HARDDISK) {
        ConfigureParams.SCSI.target[i].bDiskInserted = true;
    }
    
    SCSIdisk[i].lun = SCSIdisk[i].status = SCSIdisk[i].message = 0;
    SCSIdisk[i].sense.code = SCSIdisk[i].sense.key = SCSIdisk[i].sense.info = 0;
    SCSIdisk[i].sense.valid = false;
    SCSIdisk[i].lba = SCSIdisk[i].lastlba = SCSIdisk[i].blockcounter = 0;
    SCSIdisk[i].blocksize = SCSI_BLOCKSIZE;
    
    SCSIdisk[i].shadow = NULL;
    
    if (SCSIdisk[i].devtype != DEVTYPE_NONE && ConfigureParams.SCSI.target[i].bDiskInserted) {
        Log_Printf(LOG_WARN, "SCSI disk %i: Insert %s", i, ConfigureParams.SCSI.target[i].szImageName);
        
        SCSIdisk[i].known = SCSI_LookupDisk(i); /* Sets size and blocksize */
        
        if (ConfigureParams.SCSI.target[i].nDeviceType == DEVTYPE_CD) {
            ConfigureParams.SCSI.target[i].bWriteProtected = true;
        }
        if (!ConfigureParams.SCSI.target[i].bWriteProtected) {
            SCSIdisk[i].dsk = File_Open(ConfigureParams.SCSI.target[i].szImageName, "rb+");
            SCSIdisk[i].readonly = false;
        }
        if (ConfigureParams.SCSI.target[i].bWriteProtected || SCSIdisk[i].dsk == NULL) {
            SCSIdisk[i].dsk = File_Open(ConfigureParams.SCSI.target[i].szImageName, "rb");
            SCSIdisk[i].readonly = true;
        }
        if (SCSIdisk[i].dsk == NULL || SCSIdisk[i].size == 0) {
            Log_Printf(LOG_WARN, "SCSI disk %i: Cannot open image file %s",
                       i, ConfigureParams.SCSI.target[i].szImageName);
            SCSI_Eject(i);
            if (SCSIdisk[i].devtype == DEVTYPE_HARDDISK) {
                SCSIdisk[i].devtype = DEVTYPE_NONE;
            }
            Statusbar_AddMessage("Cannot open SCSI disk.", 0);
        }
    }
}

void SCSI_Eject(uint8_t i) {    
    SCSIdisk[i].dsk = File_Close(SCSIdisk[i].dsk);
    SCSIdisk[i].size = 0;
    SCSIdisk[i].readonly = false;
}


/* Initialize/Uninitialize SCSI disks */
static void SCSI_Init(void) {
    Log_Printf(LOG_WARN, "Loading SCSI disks:");
    
    int i;
    for (i = 0; i < ESP_MAX_DEVS; i++) {
        SCSI_Insert(i);
    }
}

static void SCSI_Uninit(void) {
    int i;
    for (i = 0; i < ESP_MAX_DEVS; i++) {
        if (SCSIdisk[i].dsk) {
            SCSI_Eject(i);
        }
    }
}

void SCSI_Reset(void) {
    SCSI_Uninit();
    SCSI_Init();
}
