uint8_t rtc_interface_read(void);
void rtc_interface_write(uint8_t rtdatabit);
void rtc_interface_reset(void);

void rtc_request_power_down(void);
void rtc_stop_pdown_request(void);

void nvram_init(void);
void nvram_checksum(int force);
char * get_rtc_ram_info(void);

void RTC_Reset(void);
