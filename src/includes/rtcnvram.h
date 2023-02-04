extern uint8_t rtc_interface_read(void);
extern void rtc_interface_write(uint8_t rtdatabit);
extern void rtc_interface_start(void);
extern void rtc_interface_reset(void);

extern void rtc_request_power_down(void);
extern void rtc_stop_pdown_request(void);

extern void nvram_init(void);
extern void nvram_checksum(int force);
extern void NVRAM_Info(FILE *fp, uint32_t dummy);

extern void RTC_Reset(void);
