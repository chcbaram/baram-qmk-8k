#include "ymodem.h"


#ifdef _USE_HW_YMODEM
#include "uart.h"
#include "cli.h"


#define YMODEM_SOH    0x01
#define YMODEM_STX    0x02
#define YMODEM_ACK    0x06
#define YMODEM_NACK   0x15
#define YMODEM_EOT    0x04
#define YMODEM_C      0x43
#define YMODEM_CAN    0x18
#define YMODEM_BS     0x08



enum
{
  YMODEM_STATE_WAIT_HEAD,
  YMODEM_STATE_WAIT_FIRST,
  YMODEM_STATE_WAIT_DATA,
  YMODEM_STATE_WAIT_LAST,
  YMODEM_STATE_WAIT_END,
  YMODEM_STATE_WAIT_CANCEL,
};

enum
{
  YMODEM_PACKET_WAIT_FIRST,
  YMODEM_PACKET_WAIT_SEQ1,
  YMODEM_PACKET_WAIT_SEQ2,
  YMODEM_PACKET_WAIT_DATA,
  YMODEM_PACKET_WAIT_CRCH,
  YMODEM_PACKET_WAIT_CRCL,
};

enum
{
  YMODEM_RESP_NONE,
  YMODEM_RESP_ACK,
  YMODEM_RESP_ACK_C,
};


#ifdef _USE_HW_CLI
static void cliYmodem(cli_args_t *args);
#endif


static uint16_t crc16(uint8_t *data, uint16_t size);
static bool ymodemReceivePacket(ymodem_packet_t *p_packet, uint8_t data_in);




bool ymodemInit(void)
{
#ifdef _USE_HW_CLI
  cliAdd("ymodem", cliYmodem);
#endif

  return true;
}

bool ymodemOpen(ymodem_t *p_modem, uint8_t ch)
{
  bool ret = true;

  p_modem->ch = ch;
  p_modem->is_init = true;

  p_modem->state           = YMODEM_STATE_WAIT_HEAD;
  p_modem->rx_packet.state = YMODEM_PACKET_WAIT_FIRST;
  p_modem->file_buf        = &p_modem->rx_packet.buffer[3];
  p_modem->file_buf_length = 0;
  p_modem->pre_time        = millis();
  p_modem->start_time      = 3000;
  p_modem->ack_mode        = 0;
  p_modem->file_received   = 0;

  p_modem->rx_packet.data = &p_modem->rx_packet.buffer[3];

  return ret;
}

bool ymodemPutch(ymodem_t *p_modem, uint8_t data)
{
  bool ret = true;

  ret = uartWrite(p_modem->ch, &data, 1);

  return ret;
}

bool ymodemGetFileInfo(ymodem_t *p_modem)
{
  bool ret = true;
  bool valid;
  uint16_t size_i;

  valid = false;
  for (int i=0; i<128; i++)
  {
    p_modem->file_name[i] = p_modem->rx_packet.data[i];

    if (p_modem->file_name[i] == 0x00)
    {
      size_i = i + 1;

      if (i > 0)
      {
        valid = true;
      }
      break;
    }
  }


  if (valid == true)
  {
    valid = false;
    for (int i=size_i; i<128; i++)
    {
      if (p_modem->rx_packet.data[i] == 0x20)
      {
        p_modem->rx_packet.data[i] = 0x00;
        valid = true;
        break;
      }
    }
    p_modem->file_length = (uint32_t)strtoul((const char * )&p_modem->rx_packet.data[size_i], (char **)NULL, (int) 0);
  }

  ret = valid;

  return ret;
}

bool ymodemAck(ymodem_t *p_modem)
{
  if (p_modem->ack_mode == YMODEM_RESP_ACK_C)
  {
    ymodemPutch(p_modem, YMODEM_ACK);
    ymodemPutch(p_modem, YMODEM_C);
  }

  if (p_modem->ack_mode == YMODEM_RESP_ACK)
  {
    ymodemPutch(p_modem, YMODEM_ACK);
  }

  p_modem->ack_mode = YMODEM_RESP_NONE;

  return true;
}

bool ymodemReceiveData(ymodem_t *p_modem)
{
  bool ret = false;
  uint32_t buf_length;


  p_modem->rx_data = uartRead(p_modem->ch);

  if (ymodemReceivePacket(&p_modem->rx_packet, p_modem->rx_data) == true)
  {
    if (p_modem->state != YMODEM_STATE_WAIT_HEAD)
    {
      if (p_modem->rx_packet.stx == YMODEM_CAN)
      {
        p_modem->state = YMODEM_STATE_WAIT_CANCEL;
      }
    }

    switch(p_modem->state)
    {
      case YMODEM_STATE_WAIT_HEAD:
        if (p_modem->rx_packet.stx == YMODEM_EOT)
        {
          ymodemPutch(p_modem, YMODEM_NACK);
          p_modem->state = YMODEM_STATE_WAIT_LAST;
        }
        else if (p_modem->rx_packet.seq[0] == 0x00)
        {
          p_modem->file_addr = 0;
          if (ymodemGetFileInfo(p_modem) == true)
          {
            p_modem->ack_mode = YMODEM_RESP_ACK_C;

            p_modem->state = YMODEM_STATE_WAIT_FIRST;
            p_modem->type = YMODEM_TYPE_START;
            ret = true;
          }
          else
          {
            ymodemPutch(p_modem, YMODEM_ACK);
          }
        }
        break;

      case YMODEM_STATE_WAIT_FIRST:
        if (p_modem->rx_packet.stx == YMODEM_EOT)
        {
          ymodemPutch(p_modem, YMODEM_NACK);
          p_modem->state = YMODEM_STATE_WAIT_LAST;
        }
        else if (p_modem->rx_packet.seq[0] == 0x01)
        {
          p_modem->file_addr = 0;
          p_modem->file_received = 0;

          buf_length = (p_modem->file_length - p_modem->file_addr);
          if (buf_length > p_modem->rx_packet.length)
          {
            buf_length = p_modem->rx_packet.length;
          }
          p_modem->file_buf_length = buf_length;
          p_modem->file_received += buf_length;

          p_modem->ack_mode = YMODEM_RESP_ACK;
          p_modem->state = YMODEM_STATE_WAIT_DATA;
          p_modem->type = YMODEM_TYPE_DATA;
          ret = true;
        }
        break;

      case YMODEM_STATE_WAIT_DATA:
        if (p_modem->rx_packet.stx == YMODEM_EOT)
        {
          ymodemPutch(p_modem, YMODEM_NACK);
          p_modem->state = YMODEM_STATE_WAIT_LAST;
        }
        else
        {
          if (p_modem->rx_packet.seq[0] != p_modem->rx_packet.seq_pre[0])
          {
            p_modem->file_addr += (p_modem->file_received - p_modem->file_addr);

            buf_length = (p_modem->file_length - p_modem->file_received);
            if (buf_length > p_modem->rx_packet.length)
            {
              buf_length = p_modem->rx_packet.length;
            }
            p_modem->file_buf_length = buf_length;
            p_modem->file_received += buf_length;

            p_modem->ack_mode = YMODEM_RESP_ACK;
            p_modem->type = YMODEM_TYPE_DATA;
            ret = true;
          }
          else
          {
            ymodemPutch(p_modem, YMODEM_ACK);
          }
        }
        break;

      case YMODEM_STATE_WAIT_LAST:
        p_modem->ack_mode = YMODEM_RESP_ACK_C;

        if (p_modem->rx_packet.stx == YMODEM_EOT)
        {
          p_modem->state = YMODEM_STATE_WAIT_END;
          p_modem->type = YMODEM_TYPE_EOT;
        }
        else
        {
          p_modem->state = YMODEM_STATE_WAIT_HEAD;
          p_modem->type = YMODEM_TYPE_END;
        }
        ret = true;
        break;

      case YMODEM_STATE_WAIT_END:
        p_modem->ack_mode = YMODEM_RESP_ACK;
        p_modem->state = YMODEM_STATE_WAIT_HEAD;
        p_modem->type = YMODEM_TYPE_END;
        ret = true;
        break;

      case YMODEM_STATE_WAIT_CANCEL:
        p_modem->ack_mode = YMODEM_RESP_ACK;
        p_modem->state = YMODEM_STATE_WAIT_HEAD;
        p_modem->type = YMODEM_TYPE_CANCEL;
        ret = true;
        break;
    }
  }

  return ret;
}

bool ymodemReceive(ymodem_t *p_modem)
{
  bool ret = false;

  if (p_modem->is_init != true)
  {
    p_modem->type = YMODEM_TYPE_ERROR;
    return true;
  }

  while (uartAvailable(p_modem->ch) > 0)
  {
    ret = ymodemReceiveData(p_modem);
    if (ret == true)
    {
      break;
    }
  }

  if (p_modem->rx_packet.state == YMODEM_PACKET_WAIT_FIRST)
  {
    if (millis()-p_modem->pre_time >= p_modem->start_time)
    {
      p_modem->pre_time = millis();
      ymodemPutch(p_modem, YMODEM_C);
    }
  }

  return ret;
}

bool ymodemReceivePacket(ymodem_packet_t *p_packet, uint8_t data_in)
{
  bool ret = false;


  switch(p_packet->state)
  {
    case YMODEM_PACKET_WAIT_FIRST:
      if (data_in == YMODEM_SOH)
      {
        p_packet->length = 128;
        p_packet->stx = data_in;
        p_packet->state = YMODEM_PACKET_WAIT_SEQ1;
      }
      if (data_in == YMODEM_STX)
      {
        p_packet->length = 1024;
        p_packet->stx = data_in;
        p_packet->state = YMODEM_PACKET_WAIT_SEQ1;
      }
      if (data_in == YMODEM_EOT)
      {
        p_packet->stx = data_in;
        ret = true;
      }
      if (data_in == YMODEM_CAN)
      {
        p_packet->stx = data_in;
        ret = true;
      }
      break;

    case YMODEM_PACKET_WAIT_SEQ1:
      p_packet->seq_pre[0] = p_packet->seq[0];
      p_packet->seq[0] = data_in;
      p_packet->state = YMODEM_PACKET_WAIT_SEQ2;
      break;

    case YMODEM_PACKET_WAIT_SEQ2:
      p_packet->seq_pre[1] = p_packet->seq[1];
      p_packet->seq[1] = data_in;
      if (p_packet->seq[0] == (uint8_t)(~data_in))
      {
        p_packet->index = 0;
        p_packet->state = YMODEM_PACKET_WAIT_DATA;
      }
      else
      {
        p_packet->state = YMODEM_PACKET_WAIT_FIRST;
      }
      break;

    case YMODEM_PACKET_WAIT_DATA:
      p_packet->data[p_packet->index] = data_in;
      p_packet->index++;
      if (p_packet->index >= p_packet->length)
      {
        p_packet->state = YMODEM_PACKET_WAIT_CRCH;
      }
      break;

    case YMODEM_PACKET_WAIT_CRCH:
      p_packet->crc_recv = (data_in<<8);
      p_packet->state = YMODEM_PACKET_WAIT_CRCL;
      break;

    case YMODEM_PACKET_WAIT_CRCL:
      p_packet->crc_recv |= (data_in<<0);
      p_packet->state = YMODEM_PACKET_WAIT_FIRST;

      p_packet->crc = crc16(p_packet->data, p_packet->length);

      if (p_packet->crc == p_packet->crc_recv)
      {
        ret = true;
      }
      break;
  }

  return ret;
}


#define CRC_POLY 0x1021

uint16_t crc_update(uint16_t crc_in, int incr)
{
  uint16_t xor = crc_in >> 15;
  uint16_t out = crc_in << 1;

  if (incr)
  {
    out++;
  }

  if (xor)
  {
    out ^= CRC_POLY;
  }

  return out;
}

uint16_t crc16(uint8_t *data, uint16_t size)
{
  uint16_t crc, i;

  for (crc = 0; size > 0; size--, data++)
  {
    for (i = 0x80; i; i >>= 1)
    {
      crc = crc_update(crc, *data & i);
    }
  }

  for (i = 0; i < 16; i++)
  {
    crc = crc_update(crc, 0);
  }

  return crc;
}


#ifdef _USE_HW_CLI
static ymodem_t ymodem;

void cliYmodem(cli_args_t *args)
{
  bool ret = false;
  
  bool keep_loop;
  uint8_t log_ch = _DEF_UART2;


  if (args->argc == 1 && args->isStr(0, "down"))
  {
    ymodemOpen(&ymodem, _DEF_UART1);

    keep_loop = true;

    while(keep_loop)
    {
      if (ymodemReceive(&ymodem) == true)
      {
        switch(ymodem.type)
        {
          case YMODEM_TYPE_START:
            uartPrintf(log_ch, "YMODEM_TYPE_START %s %d\n", ymodem.file_name, ymodem.file_length);
            break;

          case YMODEM_TYPE_DATA:
            uartPrintf(log_ch, "YMODEM_TYPE_DATA %d %d %%\n", ymodem.rx_packet.seq[0], ymodem.file_received*100 / ymodem.file_length);
            break;

          case YMODEM_TYPE_END:
            uartPrintf(log_ch, "YMODEM_TYPE_END \n");
            keep_loop = false;
            break;

          case YMODEM_TYPE_CANCEL:
            uartPrintf(log_ch, "YMODEM_TYPE_CANCEL \n");
            keep_loop = false;
            break;

          case YMODEM_TYPE_ERROR:
            uartPrintf(log_ch, "YMODEM_TYPE_ERROR \n");
            keep_loop = false;
            break;

          default:
            uartPrintf(log_ch, "YMODEM_TYPE_RETRY \n");
            break;
        }

        ymodemAck(&ymodem);
      }
    }
    ret = true;
  }


  if (ret != true)
  {
    cliPrintf("ymodem down\n");
  }
}
#endif


#endif