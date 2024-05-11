#include "flash.h"



#ifdef _USE_HW_FLASH
#include "qspi.h"
#include "spi_flash.h"
#include "cli.h"


#define FLASH_ADDR(bank)          (0x8000000 + (bank*FLASH_BANK_SIZE))
#define FLASH_MAX_BANK            2
#define FLASH_MAX_SECTOR          FLASH_PAGE_NB
#define FLASH_WRITE_SIZE          16
#define FLASH_SECTOR_SIZE         FLASH_PAGE_SIZE



#if CLI_USE(HW_FLASH)
static void cliFlash(cli_args_t *args);
#endif
static bool flashInSector(uint8_t bank, uint16_t sector_num, uint32_t addr, uint32_t length);





bool flashInit(void)
{

  logPrintf("[OK] flashInit()\n");

#if CLI_USE(HW_FLASH)
  cliAdd("flash", cliFlash);
#endif
  return true;
}

bool flashInSector(uint8_t bank, uint16_t sector_num, uint32_t addr, uint32_t length)
{
  bool ret = false;

  uint32_t sector_start;
  uint32_t sector_end;
  uint32_t flash_start;
  uint32_t flash_end;


  sector_start = FLASH_ADDR(bank) + (sector_num * FLASH_SECTOR_SIZE);
  sector_end   = sector_start + FLASH_SECTOR_SIZE - 1;
  flash_start  = addr;
  flash_end    = addr + length - 1;


  if (sector_start >= flash_start && sector_start <= flash_end)
  {
    ret = true;
  }

  if (sector_end >= flash_start && sector_end <= flash_end)
  {
    ret = true;
  }

  if (flash_start >= sector_start && flash_start <= sector_end)
  {
    ret = true;
  }

  if (flash_end >= sector_start && flash_end <= sector_end)
  {
    ret = true;
  }

  return ret;
}


bool flashErase(uint32_t addr, uint32_t length)
{
  bool ret = false;

  int32_t start_sector = -1;
  int32_t end_sector = -1;
  uint32_t bank_idx = 0;

#ifdef _USE_HW_QSPI
  if (addr >= qspiGetAddr() && addr < (qspiGetAddr() + qspiGetLength()))
  {
    ret = qspiErase(addr - qspiGetAddr(), length);
    return ret;
  }
#endif
#ifdef _USE_HW_SPI_FLASH
  if (addr >= spiFlashGetAddr() && addr < (spiFlashGetAddr() + spiFlashGetLength()))
  {
    ret = spiFlashErase(addr - spiFlashGetAddr(), length);
    return ret;
  }
#endif

  HAL_FLASH_Unlock();

  for (bank_idx = 0; bank_idx < FLASH_MAX_BANK; bank_idx++)
  {
    start_sector = -1;
    end_sector = -1;

    for (int i=0; i<FLASH_MAX_SECTOR; i++)
    {
      if (flashInSector(bank_idx, i, addr, length) == true)
      {
        if (start_sector < 0)
        {
          start_sector = i;
        }
        end_sector = i;
      }
    }

    if (start_sector >= 0)
    {
      FLASH_EraseInitTypeDef EraseInit;
      uint32_t SectorError;
      HAL_StatusTypeDef status;
      uint32_t bank_num[FLASH_MAX_BANK] = {FLASH_BANK_1, FLASH_BANK_2};

      EraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
      EraseInit.Banks     = bank_num[bank_idx];
      EraseInit.Page      = start_sector;
      EraseInit.NbPages   = (end_sector - start_sector) + 1;

      status = HAL_FLASHEx_Erase(&EraseInit, &SectorError);
      if (status == HAL_OK)
      {
        ret = true;
      }
    }
  }

  HAL_FLASH_Lock();

  return ret;
}

bool flashWrite(uint32_t addr, uint8_t *p_data, uint32_t length)
{
  bool     ret = true;
  uint32_t index;
  uint32_t write_length;
  uint32_t write_addr;
  uint32_t buf_[FLASH_WRITE_SIZE/4];
  uint8_t *buf = (uint8_t *)buf_;
  uint32_t offset;
  HAL_StatusTypeDef status;


#ifdef _USE_HW_QSPI
  if (addr >= qspiGetAddr() && addr < (qspiGetAddr() + qspiGetLength()))
  {
    ret = qspiWrite(addr - qspiGetAddr(), p_data, length);
    return ret;
  }
#endif
#ifdef _USE_HW_SPI_FLASH
  if (addr >= spiFlashGetAddr() && addr < (spiFlashGetAddr() + spiFlashGetLength()))
  {
    ret = spiFlashWrite(addr - spiFlashGetAddr(), p_data, length);
    return ret;
  }
#endif

  HAL_FLASH_Unlock();

  index = 0;
  offset = addr%FLASH_WRITE_SIZE;

  if (offset != 0 || length < FLASH_WRITE_SIZE)
  {
    write_addr = addr - offset;
    memcpy(&buf[0], (void *)write_addr, FLASH_WRITE_SIZE);
    memcpy(&buf[offset], &p_data[0], constrain(FLASH_WRITE_SIZE-offset, 0, length));

    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD, write_addr, (uint32_t)buf);
    if (status != HAL_OK)
    {
      return false;
    }

    if (offset == 0 && length < FLASH_WRITE_SIZE)
    {
      index += length;
    }
    else
    {
      index += (FLASH_WRITE_SIZE - offset);
    }
  }


  while(index < length)
  {
    write_length = constrain(length - index, 0, FLASH_WRITE_SIZE);
    write_addr   = addr + index;

    if (write_length == FLASH_WRITE_SIZE)
    {
      memcpy(&buf[0], &p_data[index], write_length);
    }
    else
    {
      memcpy(&buf[0], (void *)write_addr, write_length);
      memcpy(&buf[0], &p_data[index], write_length);
    }      

    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD, write_addr, (uint32_t)buf);
    if (status != HAL_OK)
    {
      ret = false;
      break;
    }

    index += write_length;
  }

  HAL_FLASH_Lock();

  return ret;
}

bool flashRead(uint32_t addr, uint8_t *p_data, uint32_t length)
{
  bool ret = true;
  uint8_t *p_byte = (uint8_t *)addr;


#ifdef _USE_HW_QSPI
  if (addr >= qspiGetAddr() && addr < (qspiGetAddr() + qspiGetLength()))
  {
    ret = qspiRead(addr - qspiGetAddr(), p_data, length);
    return ret;
  }
#endif
#ifdef _USE_HW_SPI_FLASH
  if (addr >= spiFlashGetAddr() && addr < (spiFlashGetAddr() + spiFlashGetLength()))
  {
    ret = spiFlashRead(addr - spiFlashGetAddr(), p_data, length);
    return ret;
  }
#endif

  for (int i=0; i<length; i++)
  {
    p_data[i] = p_byte[i];
  }

  return ret;
}




#if CLI_USE(HW_FLASH)
void cliFlash(cli_args_t *args)
{
  bool ret = false;
  uint32_t i;
  uint32_t addr;
  uint32_t length;
  uint32_t pre_time;
  bool flash_ret;


  if (args->argc == 1 && args->isStr(0, "info"))
  {
    cliPrintf("flash addr b1: 0x%X\n", FLASH_ADDR(0));
    cliPrintf("flash addr b2: 0x%X\n", FLASH_ADDR(1));    
    ret = true;
  }

  if(args->argc == 3 && args->isStr(0, "read"))
  {
    uint8_t data;

    addr   = (uint32_t)args->getData(1);
    length = (uint32_t)args->getData(2);

    for (i=0; i<length; i++)
    {
      flash_ret = flashRead(addr+i, &data, 1);

      if (flash_ret == true)
      {
        cliPrintf( "addr : 0x%X\t 0x%02X\n", addr+i, data);
      }
      else
      {
        cliPrintf( "addr : 0x%X\t Fail\n", addr+i);
      }
    }

    ret = true;
  }
    
  if(args->argc == 3 && args->isStr(0, "erase"))
  {
    addr   = (uint32_t)args->getData(1);
    length = (uint32_t)args->getData(2);

    pre_time = millis();
    flash_ret = flashErase(addr, length);

    cliPrintf( "addr : 0x%X\t len : %d %d ms\n", addr, length, (millis()-pre_time));
    if (flash_ret)
    {
      cliPrintf("OK\n");
    }
    else
    {
      cliPrintf("FAIL\n");
    }

    ret = true;
  }
    
  if(args->argc == 3 && args->isStr(0, "write"))
  {
    uint32_t data;

    addr = (uint32_t)args->getData(1);
    data = (uint32_t)args->getData(2);

    pre_time = millis();
    flash_ret = flashWrite(addr, (uint8_t *)&data, 4);

    cliPrintf( "addr : 0x%X\t 0x%X %dms\n", addr, data, millis()-pre_time);
    if (flash_ret)
    {
      cliPrintf("OK\n");
    }
    else
    {
      cliPrintf("FAIL\n");
    }

    ret = true;
  }

  if(args->argc == 3 && args->isStr(0, "check"))
  {
    uint32_t data = 0;
    uint32_t block = 4;


    addr    = (uint32_t)args->getData(1);
    length  = (uint32_t)args->getData(2);
    length -= (length % block);

    do
    {
      cliPrintf("flashErase()..");
      if (flashErase(addr, length) == false)
      {
        cliPrintf("Fail\n");
        break;
      }
      cliPrintf("OK\n");

      cliPrintf("flashWrite()..");
      for (uint32_t i=0; i<length; i+=block)
      {
        data = i;
        if (flashWrite(addr + i, (uint8_t *)&data, block) == false)
        {
          cliPrintf("Fail %d\n", i);
          break;
        }
      }
      cliPrintf("OK\n");

      cliPrintf("flashRead() ..");
      for (uint32_t i=0; i<length; i+=block)
      {
        data = 0;
        if (flashRead(addr + i, (uint8_t *)&data, block) == false)
        {
          cliPrintf("Fail %d\n", i);
          break;
        }
        if (data != i)
        {
          cliPrintf("Check Fail %d\n", i);
          break;
        }
      }  
      cliPrintf("OK\n");


      cliPrintf("flashErase()..");
      if (flashErase(addr, length) == false)
      {
        cliPrintf("Fail\n");
        break;
      }
      cliPrintf("OK\n");  
    } while (0);
    
    ret = true;
  }


  if (ret == false)
  {
    cliPrintf( "flash info\n");
    cliPrintf( "flash read  [addr] [length]\n");
    cliPrintf( "flash erase [addr] [length]\n");
    cliPrintf( "flash write [addr] [data]\n");
    cliPrintf( "flash check [addr] [length]\n");
  }
}
#endif

#endif