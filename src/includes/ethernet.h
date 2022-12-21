extern void EN_TX_Status_Read(void);
extern void EN_TX_Status_Write(void);
extern void EN_TX_Mask_Read(void);
extern void EN_TX_Mask_Write(void);
extern void EN_RX_Status_Read(void);
extern void EN_RX_Status_Write(void);
extern void EN_RX_Mask_Read(void);
extern void EN_RX_Mask_Write(void);
extern void EN_TX_Mode_Read(void);
extern void EN_TX_Mode_Write(void);
extern void EN_RX_Mode_Read(void);
extern void EN_RX_Mode_Write(void);
extern void EN_Reset_Write(void);
extern void EN_NodeID0_Read(void);
extern void EN_NodeID0_Write(void);
extern void EN_NodeID1_Read(void);
extern void EN_NodeID1_Write(void);
extern void EN_NodeID2_Read(void);
extern void EN_NodeID2_Write(void);
extern void EN_NodeID3_Read(void);
extern void EN_NodeID3_Write(void);
extern void EN_NodeID4_Read(void);
extern void EN_NodeID4_Write(void);
extern void EN_NodeID5_Read(void);
extern void EN_NodeID5_Write(void);
extern void EN_CounterLo_Read(void);
extern void EN_CounterHi_Read(void);

#define EN_BUF_MAX (64*1024)

typedef struct {
    uint8_t data[EN_BUF_MAX];
    int size;
    int limit;
} EthernetBuffer;

extern EthernetBuffer enet_tx_buffer;
extern EthernetBuffer enet_rx_buffer;

extern void ENET_IO_Handler(void);
extern void Ethernet_Reset(bool hard);
extern void enet_receive(uint8_t *pkt, int len);

/* Turbo ethernet controller */
extern void EN_RX_NewStatus_Read(void);
extern void EN_Control_Read(void);
extern void EN_RX_SavedNibble_Read(void);
extern void EN_TX_Seq_Read(void);
extern void EN_RX_Seq_Read(void);
