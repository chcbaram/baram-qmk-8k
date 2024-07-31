#include "quantum.h"


#define EEPROM_WRITE_Q_BUF_MAX  (TOTAL_EEPROM_BYTE_COUNT + 1)


typedef struct
{
  uint16_t addr;
  uint8_t  data;
} eeprom_write_t;

static uint8_t        eeprom_buf[TOTAL_EEPROM_BYTE_COUNT];
static qbuffer_t      write_q;
static eeprom_write_t write_buf[EEPROM_WRITE_Q_BUF_MAX];
static bool           is_req_clean = false;


void eeprom_init(void)
{
  eepromRead(0, eeprom_buf, TOTAL_EEPROM_BYTE_COUNT);
  qbufferCreateBySize(&write_q, (uint8_t *)write_buf, sizeof(eeprom_write_t), EEPROM_WRITE_Q_BUF_MAX); 
}

void eeprom_update(void)
{
  eeprom_write_t write_byte;

  if (qbufferAvailable(&write_q) > 0)
  {
    qbufferRead(&write_q, (uint8_t *)&write_byte, 1);
    if (eepromWriteByte(write_byte.addr, write_byte.data))
    {
      #if 0
      logPrintf("eepromWriteByte() OK %d:0x%02X\n", write_byte.addr, write_byte.data);
      #endif
    }
    else
    {
      logPrintf("eepromWriteByte() Fail\n");
    }
  }
}

void eeprom_task(void)
{
  eeprom_update();

  if (is_req_clean)
  {
    eeconfig_disable();
    soft_reset_keyboard();
  }
}

void eeprom_req_clean(void)
{
  is_req_clean = true;
}

uint8_t  eeprom_read_byte(const uint8_t *addr)
{
  return eeprom_buf[(uint32_t)addr];
}

uint16_t eeprom_read_word(const uint16_t *addr)
{
  uint16_t ret = 0;

  ret  = eeprom_buf[((uint32_t)addr) + 0] << 0;
  ret |= eeprom_buf[((uint32_t)addr) + 1] << 8;

  return ret;
}

uint32_t eeprom_read_dword(const uint32_t *addr)
{
  uint32_t ret = 0;
  const uint8_t *p = (const uint8_t *)addr;

  ret  = eeprom_read_byte(p + 0) << 0;
  ret |= eeprom_read_byte(p + 1) << 8;
  ret |= eeprom_read_byte(p + 2) << 16;
  ret |= eeprom_read_byte(p + 3) << 24;

  return ret;
};

void eeprom_read_block(void *buf, const void *addr, uint32_t len)
{
  const uint8_t *p    = (const uint8_t *)addr;
  uint8_t       *dest = (uint8_t *)buf;
  while (len--)
  {
    *dest++ = eeprom_read_byte(p++);
  }
}

void eeprom_write_byte(uint8_t *addr, uint8_t value)
{
  eeprom_write_t write_byte;

  eeprom_buf[(uint32_t)addr] = value;

  write_byte.addr = (uint32_t)addr;
  write_byte.data = value;
  qbufferWrite(&write_q, (uint8_t *)&write_byte, 1);
}

void eeprom_write_word(uint16_t *addr, uint16_t value)
{
	uint8_t *p = (uint8_t *)addr;
	eeprom_write_byte(p++, value);
	eeprom_write_byte(p, value >> 8);
}

void eeprom_write_dword(uint32_t *addr, uint32_t value)
{
	uint8_t *p = (uint8_t *)addr;
	eeprom_write_byte(p++, value);
	eeprom_write_byte(p++, value >> 8);
	eeprom_write_byte(p++, value >> 16);
	eeprom_write_byte(p, value >> 24); 
}

void eeprom_write_block(const void *buf, void *addr, size_t len)
{
  uint8_t       *p   = (uint8_t *)addr;
  const uint8_t *src = (const uint8_t *)buf;
  while (len--)
  {
    eeprom_write_byte(p++, *src++);
  }
}

void eeprom_update_byte(uint8_t *addr, uint8_t value)
{
  uint8_t orig = eeprom_read_byte(addr);
  if (orig != value)
  {
    eeprom_write_byte(addr, value);
  }
}

void eeprom_update_word(uint16_t *addr, uint16_t value)
{
  uint16_t orig = eeprom_read_word(addr);
  if (orig != value)
  {
    eeprom_write_word(addr, value);
  }
}

void eeprom_update_dword(uint32_t *addr, uint32_t value)
{
  uint32_t orig = eeprom_read_dword(addr);
  if (orig != value)
  {
    eeprom_write_dword(addr, value);
  }
}

void eeprom_update_block(const void *buf, void *addr, size_t len)
{
  uint8_t read_buf[len];
  eeprom_read_block(read_buf, addr, len);
  if (memcmp(buf, read_buf, len) != 0)
  {
    eeprom_write_block(buf, addr, len);
  }
}