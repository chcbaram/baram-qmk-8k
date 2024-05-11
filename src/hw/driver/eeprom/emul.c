#include "eeprom.h"
#include "cli.h"


#if defined(_USE_HW_EEPROM) && defined(EEPROM_CHIP_EMUL)
#include "eeprom_emul.h"

#if PAGES_NUMBER > HW_EEPROM_MAX_PAGES
#error "EEPROM PAGES_NUMBER OVER"
#endif


#if CLI_USE(HW_EEPROM)
void cliEeprom(cli_args_t *args);
#endif
static void eepromInitMPU(void);



#define EEPROM_MAX_SIZE   NB_OF_VARIABLES


static bool is_init = false;
static __IO bool is_erasing = false;


bool eepromInit()
{
  EE_Status ee_ret = EE_OK;


  eepromInitMPU();
  
  /* Enable ICACHE after testing SR BUSYF and BSYENDF */
  while((ICACHE->SR & 0x1) != 0x0) {;}
  while((ICACHE->SR & 0x2) == 0x0) {;}
  ICACHE->CR |= 0x1; 



  HAL_NVIC_SetPriority(FLASH_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(FLASH_IRQn);

  HAL_FLASH_Unlock();
  ee_ret = EE_Init(EE_FORCED_ERASE);
  HAL_FLASH_Lock();
  logPrintf("[%s] eepromInit()\n", ee_ret == EE_OK ? "OK":"E_");
  logPrintf("     chip  : emul\n");
  
  is_init = ee_ret == EE_OK ? true:false;

#if CLI_USE(HW_EEPROM)
  cliAdd("eeprom", cliEeprom);
#endif

  return is_init;
}

bool eepromIsInit(void)
{
  return is_init;
}

bool eepromValid(uint32_t addr)
{
  if (addr >= EEPROM_MAX_SIZE)
  {
    return false;
  }

  return is_init;
}

bool eepromReadByte(uint32_t addr, uint8_t *p_data)
{
  bool ret = true;
  EE_Status ee_ret = EE_OK;
  uint16_t ee_addr;

  if (addr >= EEPROM_MAX_SIZE)
    return false;
  if (is_init != true)
    return false;
  ee_addr = addr + 1;

  HAL_FLASH_Unlock();
  // ee_ret = EE_ReadVariable8bits(ee_addr, p_data);
  uint64_t data[2];
  
  ee_ret = EE_ReadVariable96bits(ee_addr, data);
  p_data[0] = data[0];
  HAL_FLASH_Lock();
  if (ee_ret != EE_OK)
  {
    if (ee_ret == EE_NO_DATA)
      *p_data = 0;
    else
      ret = false;
  }
  return ret;
}

bool eepromWriteByte(uint32_t addr, uint8_t data_in)
{
  bool ret = true;
  EE_Status ee_ret = EE_OK;
  uint16_t ee_addr;
  uint32_t pre_time = millis();

  if (addr >= EEPROM_MAX_SIZE)
    return false;
  if (is_init != true)
    return false;

  ee_addr = addr + 1;

  HAL_FLASH_Unlock();
  uint64_t data[2];
  
  data[0] = data_in;
  data[1] = 0;

  ee_ret = EE_WriteVariable96bits(ee_addr, data);
  if (ee_ret != EE_OK)
  {
    if ((ee_ret & EE_STATUSMASK_CLEANUP) == EE_STATUSMASK_CLEANUP) 
    {
      is_erasing = true;
      ee_ret = EE_CleanUp_IT();
      while (is_erasing == true) 
      { 
        if (millis()-pre_time >= 500)
        {
          ret = false;
          break;
        }
      }      
    }    
    else
    {
      ret = false;
    }
  }  
  HAL_FLASH_Lock();

  return ret;
}

bool eepromRead(uint32_t addr, uint8_t *p_data, uint32_t length)
{
  bool ret = true;
  uint32_t i;


  for (i=0; i<length; i++)
  {
    if (eepromReadByte(addr + i, &p_data[i]) != true)
    {
      ret = false;
      break;
    }
  }

  return ret;
}

bool eepromWrite(uint32_t addr, uint8_t *p_data, uint32_t length)
{
  bool ret = false;
  uint32_t i;


  for (i=0; i<length; i++)
  {
    ret = eepromWriteByte(addr + i, p_data[i]);
    if (ret == false)
    {
      break;
    }
  }

  return ret;
}

uint32_t eepromGetLength(void)
{
  return EEPROM_MAX_SIZE;
}

bool eepromFormat(void)
{
  bool ret = true;
  EE_Status ee_ret = EE_OK;

  HAL_FLASH_Unlock();
  ee_ret = EE_Format(EE_FORCED_ERASE);
  HAL_FLASH_Lock();
  if (ee_ret != EE_OK)
  {
    ret = false;
  }  
  return ret;
}

void FLASH_IRQHandler(void)
{
  HAL_FLASH_IRQHandler();
}

void HAL_FLASH_EndOfOperationCallback(uint32_t ReturnValue)
{
  /* Call CleanUp callback when all requested pages have been erased */
  if (ReturnValue == 0xFFFFFFFF)
  {
    EE_EndOfCleanup_UserCallback();
  }
}

void EE_EndOfCleanup_UserCallback(void)
{
  is_erasing = false;
}

void eepromInitMPU(void)
{
  /* MPU registers address definition */
  volatile uint32_t *mpu_type  = (void *)0xE000ED90;
  volatile uint32_t *mpu_ctrl  = (void *)0xE000ED94;
  volatile uint32_t *mpu_rnr   = (void *)0xE000ED98;
  volatile uint32_t *mpu_rbar  = (void *)0xE000ED9C;
  volatile uint32_t *mpu_rlar  = (void *)0xE000EDA0;
  volatile uint32_t *mpu_mair0 = (void *)0xE000EDC0;

  /* Check that MPU is implemented and recover the number of regions available */
  uint32_t mpu_regions_nb = ((*mpu_type) >> 8) & 0xff;

  /* If the MPU is implemented */
  if (mpu_regions_nb != 0)
  {
    /* Set RNR to configure the region with the highest number which also has the highest priority */
    *mpu_rnr = (mpu_regions_nb - 1) & 0x000000FF;

    /* Set RBAR to get the region configured starting at FLASH_USER_START_ADDR, being non-shareable, r/w by any privilege level, and non executable */
    *mpu_rbar &= 0x00000000;
    *mpu_rbar  = (START_PAGE_ADDRESS | (0x0 << 3) | (0x1 << 1) | 0x1);

    /* Set RLAR to get the region configured ending at FLASH_USER_END_ADDR, being associated to the Attribute Index 0 and enabled */
    *mpu_rlar &= 0x00000000;
    *mpu_rlar  = (END_EEPROM_ADDRESS | (0x0 << 1) | 0x1);

    /* Set MAIR0 so that the region configured is inner and outer non-cacheable */
    *mpu_mair0 &= 0xFFFFFF00;
    *mpu_mair0 |= ((0x4 << 4) | 0x4);

    /* Enable MPU + PRIVDEFENA=1 for the MPU rules to be effective + HFNMIENA=0 to ease debug */
    *mpu_ctrl &= 0xFFFFFFF8;
    *mpu_ctrl |= 0x5;
  }

  return;
}


#if CLI_USE(HW_EEPROM)
void cliEeprom(cli_args_t *args)
{
  bool ret = true;
  uint32_t i;
  uint32_t addr;
  uint32_t length;
  uint8_t  data;
  uint32_t pre_time;
  bool eep_ret;


  if (args->argc == 1)
  {
    if(args->isStr(0, "info") == true)
    {
      cliPrintf("eeprom init   : %d\n", eepromIsInit());
      cliPrintf("eeprom length : %d bytes\n", eepromGetLength());
    }
    else if(args->isStr(0, "format") == true)
    {
      if (eepromFormat() == true)
      {
        cliPrintf("format OK\n");
      }
      else
      {
        cliPrintf("format Fail\n");
      }
    }
    else
    {
      ret = false;
    }
  }
  else if (args->argc == 3)
  {
    if(args->isStr(0, "read") == true)
    {
      bool ee_ret;
      addr   = (uint32_t)args->getData(1);
      length = (uint32_t)args->getData(2);

      if (length > eepromGetLength())
      {
        cliPrintf( "length error\n");
      }
      for (i=0; i<length; i++)
      {
        ee_ret = eepromReadByte(addr+i, &data);
        cliPrintf( "addr : %d\t 0x%02X, ret %d\n", addr+i, data, ee_ret);
      }
    }
    else if(args->isStr(0, "write") == true)
    {
      addr = (uint32_t)args->getData(1);
      data = (uint8_t )args->getData(2);

      pre_time = millis();
      eep_ret = eepromWriteByte(addr, data);

      cliPrintf( "addr : %d\t 0x%02X %dms\n", addr, data, millis()-pre_time);
      if (eep_ret)
      {
        cliPrintf("OK\n");
      }
      else
      {
        cliPrintf("FAIL\n");
      }
    }
    else
    {
      ret = false;
    }
  }
  else
  {
    ret = false;
  }


  if (ret == false)
  {
    cliPrintf( "eeprom info\n");
    cliPrintf( "eeprom format\n");
    cliPrintf( "eeprom read  [addr] [length]\n");
    cliPrintf( "eeprom write [addr] [data]\n");
  }

}
#endif /* _USE_HW_CMDIF_EEPROM */


#endif /* _USE_HW_EEPROM */
