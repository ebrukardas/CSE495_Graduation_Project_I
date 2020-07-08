/*! ----------------------------------------------------------------------------
*  @file    ss_init_main.c
*  @brief   Two Way Time Of Arrival(Flight) - Tag
*
*           Bu dosya TOF yapilacak projenin tag(mobil istasyon) kismidir. 
*           Mesaj yayinlar, cevaplarini bekler. TS'ler bu degerler uzerinden ainir
*
*/

// ====================== KUTUPHANELER ======================

#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "deca_device_api.h"
#include "deca_regs.h"
#include "port_platform.h"

// ====================== TANIMLAMALAR ======================

#define APP_NAME "SS TWR INIT v1.3"

// task gecikmesi, millisaniye
#define RNG_DELAY_MS 100

/*!
* Mesaj formati
*     - byte 0/1: frame kontrol
*     - byte 2: sequence sayisi
*     - byte 3/4: PAN ID (0xDECA).
*     - byte 5/6: hedef adres (hard kodlanmis sabitler- id'leri essiz yapmak için)
*     - byte 7/8: kaynak adres (hard kodlanmis sabitler- id'leri essiz yapmak için)
*     - byte 9: fonksiyon kodu
*    Geriye kalani her mesajda degisen bilgiler
*     - byte 10 -> 13: poll mesaji timestampi
*     - byte 14 -> 17: transmission timestamp.
*     Tüm mesajlar ekstra 2 byte saglama bitiyle sonlandirilir.
*
*/
static uint8 tx_poll_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'W', 'A', 'V', 'E', 0xE0, 0x00, 0, 0}; // 10. byte hangi BS ile TOF baslatilacagini belirler
static uint8 rx_resp_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'V', 'E', 'W', 'A', 0xE1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	static uint8 tx_gateway_msg[] = {
    0x99, 0x99, 												// frame kontrol 
    //0x00, 0x00, 0x00, 0x00,						// TAG ID
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // B0 B1 B2 uzakliklari
    0x99, 0x99, 												// frame kontrol 
    0x00, 0x00 													// ranging bytelari
};


#define ALL_MSG_COMMON_LEN 10

#define ALL_MSG_SN_IDX 2
#define RESP_MSG_POLL_RX_TS_IDX 10
#define RESP_MSG_RESP_TX_TS_IDX 14
#define RESP_MSG_TS_LEN 4

static uint8 frame_seq_nb = 0;

#define ANCH_NUM 3

#define RX_BUF_LEN 20
static uint8 rx_buffer[RX_BUF_LEN];


static uint32 status_reg = 0;

#define UUS_TO_DWT_TIME 65536

#define SPEED_OF_LIGHT 299702547

// tof ve uzaklik 
static double tof;
static double distance;

static int Anchor_Idx = 0;
static double distances[3] = {0, 0, 0};

static void resp_msg_get_ts(uint8 *ts_field, uint32 *ts);
static void send_report_to_gateway(void);
static void print_msg(uint8 *msg, uint8 size);


static volatile int tx_count = 0 ;
static volatile int rx_count = 0 ;

// ====================== UYGULAMA ======================

int ss_init_run(void)
{

    // Transmission hazirligi
    tx_poll_msg[ALL_MSG_SN_IDX] = frame_seq_nb;
    dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_TXFRS);
    dwt_writetxdata(sizeof(tx_poll_msg), tx_poll_msg, 0); /* Zero offset in TX buffer. */
    dwt_writetxfctrl(sizeof(tx_poll_msg), 0, 1); /* Zero offset in TX buffer, ranging. */

    dwt_starttx(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED);
    tx_count++;
    //printf("Transmission # : %d\r\n",tx_count);

		// Ya timeouta düsecek ya da hata verecek ya da mesaj alinacak
    while (!((status_reg = dwt_read32bitreg(SYS_STATUS_ID)) & (SYS_STATUS_RXFCG | SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR)))
    {};

    frame_seq_nb++;

		// mesaj temiz alindi mi
    if (status_reg & SYS_STATUS_RXFCG)
    {		
        uint32 frame_len;

        dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RXFCG);

        frame_len = dwt_read32bitreg(RX_FINFO_ID) & RX_FINFO_RXFLEN_MASK;

        if (frame_len <= RX_BUF_LEN)
        {
            dwt_readrxdata(rx_buffer, frame_len, 0);
        }

				// mesaj kontrolü
        rx_buffer[ALL_MSG_SN_IDX] = 0;
        if (memcmp(rx_buffer, rx_resp_msg, ALL_MSG_COMMON_LEN) == 0)
        {	
            rx_count++;
            //printf("Reception # : %d\r\n",rx_count);
            uint32 poll_tx_ts, resp_rx_ts, poll_rx_ts, resp_tx_ts;
            int32 rtd_init, rtd_resp;
            float clockOffsetRatio ;

            poll_tx_ts = dwt_readtxtimestamplo32();
            resp_rx_ts = dwt_readrxtimestamplo32();

            // Carrier integrator ve clock offset 
            clockOffsetRatio = dwt_readcarrierintegrator() * (FREQ_OFFSET_MULTIPLIER * HERTZ_TO_PPM_MULTIPLIER_CHAN_5 / 1.0e6) ;

            resp_msg_get_ts(&rx_buffer[RESP_MSG_POLL_RX_TS_IDX], &poll_rx_ts);
            resp_msg_get_ts(&rx_buffer[RESP_MSG_RESP_TX_TS_IDX], &resp_tx_ts);

            // tof hesaplama
            rtd_init = resp_rx_ts - poll_tx_ts;
            rtd_resp = resp_tx_ts - poll_rx_ts;
					
					printf("Timestamp: %ld\r\n", rtd_init-rtd_resp);

            tof = ((rtd_init - rtd_resp * (1.0f - clockOffsetRatio)) / 2.0f) * DWT_TIME_UNITS; // Specifying 1.0f and 2.0f are floats to clear warning 
            distances[Anchor_Idx] = (tof * SPEED_OF_LIGHT);
            printf("%d -> Distance : %d\r\n", Anchor_Idx, (int)(distances[Anchor_Idx]*100) );
        }
    }
    else
    {
        dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR);

        dwt_rxreset();
    }


}

static void send_report_to_gateway(void)
{
    printf("TX => %d\r\nRX => %d\r\n", tx_count, rx_count);
    
    tx_gateway_msg[2] = (int)(distances[0]*100)>>8;
    tx_gateway_msg[3] = (int)(distances[0]*100);
    
    tx_gateway_msg[4] = (int)(distances[1]*100)>>8;
    tx_gateway_msg[5] = (int)(distances[1]*100);
    
    tx_gateway_msg[6] = (int)(distances[2]*100)>>8;
    tx_gateway_msg[7] = (int)(distances[2]*100);
    
    dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_TXFRS);
    dwt_writetxdata(sizeof(tx_gateway_msg), tx_gateway_msg, 0); /* Zero offset in TX buffer. */
    dwt_writetxfctrl(sizeof(tx_gateway_msg), 0, 1); /* Zero offset in TX buffer, ranging. */


    dwt_starttx(DWT_START_TX_IMMEDIATE);

    while (!((status_reg = dwt_read32bitreg(SYS_STATUS_ID)) & (SYS_STATUS_TXBERR | SYS_STATUS_TXERR | SYS_STATUS_TXFRB | SYS_STATUS_TXFRS | SYS_STATUS_TXPHS | SYS_STATUS_TXPRS | SYS_STATUS_TXPUTE)))
    {};
    
     print_msg(tx_gateway_msg, sizeof(tx_gateway_msg));
}

static void print_msg(uint8 *msg, uint8 size)
{
    int i = 0;
    for(i = 0; i<size; ++i)
    {
        printf("%02X ", msg[i]);
    }
    printf("\r\n");
}



/*! 
* @fn resp_msg_get_ts()
*
* @brief TS okur
*
* @param  ts_field  okunacak ts
*
*/
static void resp_msg_get_ts(uint8 *ts_field, uint32 *ts)
{
    int i;
    *ts = 0;
    for (i = 0; i < RESP_MSG_TS_LEN; i++)
    {
        *ts += ts_field[i] << (i * 8);
    }
}


/**@brief Task fonksiyonu.
*
* @param[in] pvParameter  task icin parametre
*/
void ss_tag_task_function (void * pvParameter)
{
    UNUSED_PARAMETER(pvParameter);

    dwt_setleds(DWT_LEDS_ENABLE);

    while (true)
    {
        for(Anchor_Idx = 0; Anchor_Idx<ANCH_NUM; ++Anchor_Idx)
        {
            tx_poll_msg[10] = Anchor_Idx;
            ss_init_run();
            vTaskDelay(100);
        }
        
        send_report_to_gateway();
        vTaskDelay(200);
    }
}