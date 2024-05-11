/*
 * ymodem.h
 *
 *  Created on: 2021. 1. 24.
 *      Author: baram
 */

#ifndef SRC_COMMON_HW_INCLUDE_YMODEM_H_
#define SRC_COMMON_HW_INCLUDE_YMODEM_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "hw_def.h"

#ifdef _USE_HW_YMODEM



#define YMODEM_BUF_LENGTH         (1024+5)



typedef enum
{
  YMODEM_TYPE_START,
  YMODEM_TYPE_DATA,
  YMODEM_TYPE_END,
  YMODEM_TYPE_CANCEL,
  YMODEM_TYPE_ERROR,
  YMODEM_TYPE_EOT,
} YmodemType;


typedef struct
{
  uint8_t   state;
  uint16_t  index;

  uint8_t   stx;
  uint8_t   seq[2];
  uint8_t   seq_pre[2];
  uint8_t   *data;
  uint16_t  length;
  uint16_t  crc;
  uint16_t  crc_recv;
  uint8_t   buffer[YMODEM_BUF_LENGTH];
} ymodem_packet_t;


typedef struct
{
  uint8_t   ch;
  bool      is_init;
  uint32_t  baud;

  YmodemType type;
  uint8_t   rx_data;
  uint8_t   state;
  uint32_t  pre_time;
  uint32_t  start_time;
  uint32_t  index;
  uint8_t   error;
  uint8_t   ack_mode;

  char      file_name[128];
  uint32_t  file_addr;
  uint32_t  file_length;
  uint32_t  file_received;

  uint8_t  *file_buf;
  uint16_t  file_buf_length;


  ymodem_packet_t  rx_packet;
} ymodem_t;



bool ymodemInit(void);
bool ymodemOpen(ymodem_t *p_modem, uint8_t ch);
bool ymodemReceive(ymodem_t *p_modem);
bool ymodemAck(ymodem_t *p_modem);

#endif

#ifdef __cplusplus
 }
#endif

#endif /* SRC_COMMON_HW_INCLUDE_YMODEM_H_ */