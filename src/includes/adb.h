uint32_t adb_lget(uint32_t addr);
uint16_t adb_wget(uint32_t addr);
uint8_t adb_bget(uint32_t addr);

void adb_lput(uint32_t addr, uint32_t l);
void adb_wput(uint32_t addr, uint16_t w);
void adb_bput(uint32_t addr, uint8_t b);

void ADB_Reset(void);
