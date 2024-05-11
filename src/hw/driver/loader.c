#include "loader.h"


#ifdef _USE_HW_LOADER
#include "cli.h"
#include "flash.h"
#include "ymodem.h"
#include "util_core.h"
#include "lcd.h"



#if CLI_USE(HW_LOADER)
static void cliCmd(cli_args_t *args);
#endif

static ymodem_t ymodem;





bool loaderInit(void)
{

  ymodemInit();


#if CLI_USE(HW_LOADER)
  cliAdd("loader", cliCmd);
#endif
  return true;
}

uint32_t loaderDownToFlash(uint32_t addr, firm_tag_t *p_tag_arg,  uint32_t timeout_ms)
{
  uint32_t err_code = LOADER_OK;
  bool keep_loop = true;
  uint32_t pre_time;
  uint16_t crc_data = 0;
  uint32_t receive_len = 0;
  uint32_t flash_addr;
  uint32_t flash_size;
  firm_tag_t firm_tag;
  firm_tag_t *p_tag;


  p_tag = (p_tag_arg != NULL) ? p_tag_arg:&firm_tag;


  ymodemOpen(&ymodem, HW_UART_CH_CLI);
  pre_time = millis();
  while(keep_loop)
  {
    if (ymodemReceive(&ymodem) == true)
    {
      pre_time = millis();

      switch(ymodem.type)
      {
        case YMODEM_TYPE_START:
          crc_data = 0;
          p_tag->magic_number = 0;

          flash_addr = addr;
          flash_size = FLASH_SIZE_TAG + ymodem.file_length;
          if (flashErase(flash_addr, flash_size) != true)
          {
            keep_loop = false;
            err_code = LOADER_ERR_DATA_ERASE;
            break;
          }
          break;

        case YMODEM_TYPE_DATA:
          flash_addr = addr + FLASH_SIZE_TAG + ymodem.file_addr;
          if (flashWrite(flash_addr, ymodem.file_buf, ymodem.file_buf_length) != true)
          {
            keep_loop = false;
            err_code = LOADER_ERR_DATA_WRITE;
            break;
          } 
          crc_data = utilCalcCRC(crc_data, ymodem.file_buf, ymodem.file_buf_length);
          receive_len += ymodem.file_buf_length;
          break;

        case YMODEM_TYPE_END:
          p_tag->magic_number = TAG_MAGIC_NUMBER;
          p_tag->fw_addr      = FLASH_SIZE_TAG;
          p_tag->fw_size      = receive_len;
          p_tag->fw_crc       = crc_data;
          keep_loop = false;

          flash_addr = addr;
          if (flashWrite(flash_addr, (uint8_t *)p_tag, sizeof(firm_tag_t)) != true)
          {
            err_code = LOADER_ERR_DATA_WRITE;
            break;
          } 
          break;

        case YMODEM_TYPE_CANCEL:
          keep_loop = false;
          err_code = LOADER_ERR_CANCEL;
          break;

        case YMODEM_TYPE_ERROR:
          keep_loop = false;
          err_code = LOADER_ERR_ERROR;
          break;

        default:
          break;
      }
      ymodemAck(&ymodem);
    }
    if (millis()-pre_time >= timeout_ms)
    {
      keep_loop = false;
      err_code = LOADER_ERR_TIMEOUT;
    }
  }

  delay(500);
  cliPrintf("\n");
  cliPrintf("file    : %s\n", ymodem.file_name);
  cliPrintf("size    : %d B\n", ymodem.file_length);
  cliPrintf("fw addr : 0x%X\n", addr + p_tag->fw_addr);
  cliPrintf("fw size : %d B\n", p_tag->fw_size );
  cliPrintf("fw crc  : 0x%X\n", p_tag->fw_crc);


  switch(ymodem.type)
  {
    case YMODEM_TYPE_END:
      cliPrintf("DONE - OK\n");
      break;

    case YMODEM_TYPE_CANCEL:
      cliPrintf("DONE - CANCEL\n");
      break;

    case YMODEM_TYPE_ERROR:
      cliPrintf("DONE - ERROR\n");
      break;

    default:
      cliPrintf("DONE - ERROR(%d)\n", err_code);
      break;
  } 

  return err_code;
}

#if CLI_USE(HW_LOADER)
void cliCmd(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 1 && args->isStr(0, "info"))
  {
    firm_ver_t *p_boot = (firm_ver_t *)(FLASH_ADDR_BOOT + FLASH_SIZE_VEC);
    firm_ver_t *p_firm;
    firm_tag_t *p_tag;

    if (p_boot->magic_number == VERSION_MAGIC_NUMBER)
    {
      cliPrintf("BOOT \n");
      cliPrintf("   %s\n", p_boot->name_str);
      cliPrintf("   %s\n", p_boot->version_str);
      cliPrintf("   0x%X\n", p_boot->firm_addr);
    }
    else
    {
      cliPrintf("No Boot Version\n");
    }


    const char    *firm_name[2] = {"FIRM", "UPDATE"};
    const uint32_t firm_addr[2] = {FLASH_ADDR_FIRM, FLASH_ADDR_UPDATE};

    for (int i=0; i<2; i++)
    {
      p_firm = (firm_ver_t *)(firm_addr[i] + FLASH_SIZE_TAG + FLASH_SIZE_VEC);
      p_tag  = (firm_tag_t *)(firm_addr[i]);

      if (p_firm->magic_number == VERSION_MAGIC_NUMBER)
      {
        cliPrintf("%s \n", firm_name[i]);
        cliPrintf("   %s\n", p_firm->name_str);
        cliPrintf("   %s\n", p_firm->version_str);
        cliPrintf("   0x%X\n", p_firm->firm_addr);
      }
      else
      {
        cliPrintf("No %s Version\n", firm_name[i]);
      }

      if (p_tag->magic_number == TAG_MAGIC_NUMBER)
      {
        cliPrintf("TAG \n");
        cliPrintf("   fw_addr : 0x%X\n", p_tag->fw_addr);
        cliPrintf("   fw_size : %d KB\n", p_tag->fw_size/1024);
        cliPrintf("   fw_crc  : 0x%X\n", p_tag->fw_crc);
      }
      else
      {
        cliPrintf("No %s Tag\n", firm_name[i]);
      }
    }


    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "flash"))
  {
    cliPrintf("flash download..\n");
    loaderDownToFlash(FLASH_ADDR_UPDATE, NULL, 30000); 
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "jump"))
  {
    // void (**jump_func)(void) = (void (**)(void))(FLASH_ADDR_FIRM + FLASH_SIZE_TAG + 4); 


    // if (((uint32_t)*jump_func) >= FLASH_ADDR_FIRM && ((uint32_t)*jump_func) < (FLASH_ADDR_FIRM + FLASH_SIZE_FIRM))
    // {
    //   cliPrintf("[  ] Jump()\n");
    //   cliPrintf("     addr : 0x%X\n", (uint32_t)*jump_func);

    //   bspDeInit();

    //   (*jump_func)();
    // }
    // else
    // {
    //   cliPrintf("Jump Address Invalid\n");
    // }
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("loader info\n");
    cliPrintf("loader flash\n");
    cliPrintf("loader jump\n");
  }
}
#endif

#endif