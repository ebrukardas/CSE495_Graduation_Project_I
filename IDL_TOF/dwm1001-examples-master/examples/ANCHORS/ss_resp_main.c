/*!
*  @file    ss_anch_main.c
*  @brief   Two Way Time Of Arrival(Flight) - Anchor
*
*           Bu dosya TOF yapilacak projenin anchor(sabit istasyon) kismidir. 
*           Poll mesaji beklenir, geldiginde RX time stamp'i alinir ve 
*           TX time stamp'i kaydedilerek cevap(anchonse) mesaji yayinlanir.
*/


// ====================== KUTUPHANELER ======================
#include "sdk_config.h" 
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "deca_device_api.h"
#include "deca_regs.h"
#include "port_platform.h"

// ====================== TANIMLAMALAR ======================

// task gecikmesi, millisaniye
#define RNG_DELAY_MS 80

// anchor numarasi (HER ANCHOR YUKLEMESINDE GUNCELLENMELI)
#define BS_IDX 0x02

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
static uint8 rx_poll_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'W', 'A', 'V', 'E', 0xE0, BS_IDX, 0, 0};
static uint8 tx_resp_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'V', 'E', 'W', 'A', 0xE1, 0,      0, 0, 0, 0, 0, 0, 0, 0, 0};

// Tüm mesajlarda ortak olan kismin uzunlugu
#define ALL_MSG_COMMON_LEN 11

// Mesaj indexleri
#define ALL_MSG_SN_IDX 2
#define anch_MSG_POLL_RX_TS_IDX 10
#define anch_MSG_anch_TX_TS_IDX 14
#define anch_MSG_TS_LEN 4	

// transmission sayisi
static uint8 frame_seq_nb = 0;

// RX mesaj ve uzunlugu
#define RX_BUF_LEN 24
static uint8 rx_buffer[RX_BUF_LEN];

// status registerinin kopyasi
static uint32 status_reg = 0;

/* UWB microsecond (uus) to device time unit (dtu, around 15.65 ps) conversion factor.
* 1 uus = 512 / 499.2 µs and 1 µs = 499.2 * 128 dtu. */
#define UUS_TO_DWT_TIME 65536

// TX timeuot u için
#define POLL_RX_TO_anch_TX_DLY_UUS  1100

// gecis gecikmesi
#define anch_TX_TO_FINAL_RX_DLY_UUS 500


typedef signed long long int64;
typedef unsigned long long uint64;
static uint64 poll_rx_ts;

typedef unsigned long long uint64;
static uint64 poll_rx_ts;
static uint64 anch_tx_ts;


static uint64 get_rx_timestamp_u64(void);
static void anch_msg_set_ts(uint8 *ts_field, const uint64 ts);

// ====================== UYGULAMA ======================

int ss_anch_run(void)
{

  dwt_rxenable(DWT_START_RX_IMMEDIATE);

  // Ya timeouta düsecek ya da hata verecek ya da mesaj alinacak
  while (!((status_reg = dwt_read32bitreg(SYS_STATUS_ID)) & (SYS_STATUS_RXFCG | SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR)))
  {};

		// timeout tipi
    #if 0
    int temp = 0;
    if(status_reg & SYS_STATUS_RXRFTO )
    temp =1;
    else if(status_reg & SYS_STATUS_RXPTO )
    temp =2;
    #endif

	// mesaj temiz alindi mi
  if (status_reg & SYS_STATUS_RXFCG)
  {
    uint32 frame_len=0;

    dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RXFCG);

    frame_len = dwt_read32bitreg(RX_FINFO_ID) & RX_FINFO_RXFL_MASK_1023;
    if (frame_len <= RX_BUFFER_LEN)
    {
      dwt_readrxdata(rx_buffer, frame_len, 0);
    }

		// mesaj kontrolü
    rx_buffer[ALL_MSG_SN_IDX] = 0;
    if (memcmp(rx_buffer, rx_poll_msg, ALL_MSG_COMMON_LEN) == 0)
    {
      uint32 anch_tx_time=0;
      int ret=0;

      poll_rx_ts = get_rx_timestamp_u64();

      /* Compute final message transmission time. See NOTE 7 below. */
      anch_tx_time = (poll_rx_ts + (POLL_RX_TO_anch_TX_DLY_UUS * UUS_TO_DWT_TIME)) >> 8;
      dwt_setdelayedtrxtime(anch_tx_time);

      /* anchonse TX timestamp is the transmission time we programmed plus the antenna delay. */
      anch_tx_ts = (((uint64)(anch_tx_time & 0xFFFFFFFEUL)) << 8) + TX_ANT_DLY;

      // gönderilecek mesaja setleri
      anch_msg_set_ts(&tx_resp_msg[anch_MSG_POLL_RX_TS_IDX], poll_rx_ts);
      anch_msg_set_ts(&tx_resp_msg[anch_MSG_anch_TX_TS_IDX], anch_tx_ts);

      //mesaji gönder
      tx_resp_msg[ALL_MSG_SN_IDX] = frame_seq_nb;
      dwt_writetxdata(sizeof(tx_resp_msg), tx_resp_msg, 0);
      dwt_writetxfctrl(sizeof(tx_resp_msg), 0, 1);
      ret = dwt_starttx(DWT_START_TX_DELAYED);

      // transmission baslangici
      if (ret == DWT_SUCCESS)
      {
				while (!(dwt_read32bitreg(SYS_STATUS_ID) & SYS_STATUS_TXFRS))
				{};

				dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_TXFRS);

				frame_seq_nb++;
      }
      else
      {
				dwt_rxreset();
      }
    }
  }
  else // alinamadi
  {
    dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_ERR);
    dwt_rxreset();
  }

  return(1);		
}

/*!
* @fn get_rx_timestamp_u64()
*
* @brief RX time-stampi döndürür
**
* @return  time-stamp
*/
static uint64 get_rx_timestamp_u64(void)
{
  uint8 ts_tab[5];
  uint64 ts = 0;
  int i;
  dwt_readrxtimestamp(ts_tab);
  for (i = 4; i >= 0; i--)
  {
    ts <<= 8;
    ts |= ts_tab[i];
  }
  return ts;
}

/*! 
* @fn final_msg_set_ts()
*
* @brief Mesaj set
*
* @param  ts_field  timestamp
*         ts  timestamp degeri
*
*/
static void anch_msg_set_ts(uint8 *ts_field, const uint64 ts)
{
  int i;
  for (i = 0; i < anch_MSG_TS_LEN; i++)
  {
    ts_field[i] = (ts >> (i * 8)) & 0xFF;
  }
}


/**@brief Task fonksiyonu.
*
* @param[in] pvParameter task icin parametre
*/
void ss_anchor_task_function (void * pvParameter)
{
  UNUSED_PARAMETER(pvParameter);

  dwt_setleds(DWT_LEDS_ENABLE);

  while (true)
  {
    ss_anch_run();
    vTaskDelay(RNG_DELAY_MS);
  }
}
