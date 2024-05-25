#include "keys.h"


#ifdef _USE_HW_KEYS
#include "button.h"
#include "qbuffer.h"
#include "cli.h"
#include "scan/pssi.h"



static uint8_t cols_buf[MATRIX_COLS];


#if CLI_USE(HW_KEYS)
static void cliCmd(cli_args_t *args);
#endif


static EXTI_HandleTypeDef hexti_6;


bool keysInit(void)
{

  pssiInit();

#if CLI_USE(HW_KEYS)
  cliAdd("keys", cliCmd);
#endif

  
  EXTI_ConfigTypeDef exticonfig;

  exticonfig.Line = EXTI_LINE_6;
  exticonfig.Mode = EXTI_MODE_INTERRUPT;
  exticonfig.GPIOSel = EXTI_GPIOC;
  exticonfig.Trigger = EXTI_TRIGGER_RISING_FALLING;
  HAL_EXTI_SetConfigLine(&hexti_6,&exticonfig);

  HAL_NVIC_SetPriority(EXTI6_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI6_IRQn);   

  return true;
}

bool keysIsBusy(void)
{
  return pssiIsBusy();
}

bool keysUpdate(void)
{
  bool ret;

  ret = pssiUpdate();
  pssiReadBuf(cols_buf, MATRIX_COLS);
  return ret;
}

bool keysReadBuf(uint8_t *p_data, uint32_t length)
{
  pssiReadBuf(p_data, length);
  return true;
}

bool keysGetPressed(uint16_t row, uint16_t col)
{
  bool    ret = false;
  uint8_t row_bit;

  row_bit = ~cols_buf[col];

  if (row_bit & (1<<row))
  {
    ret = true;
  }

  return ret;
}

void keysUpdateEvent(void)
{
  logPrintf("key event\n");
}

void EXTI6_IRQHandler(void)   { HAL_EXTI_IRQHandler(&hexti_6); keysUpdateEvent(); }


#if CLI_USE(HW_KEYS)
void cliCmd(cli_args_t *args)
{
  bool ret = false;



  if (args->argc == 1 && args->isStr(0, "info"))
  {
    cliShowCursor(false);


    while(cliKeepLoop())
    {
      keysUpdate();
      delay(10);

      cliPrintf("     ");
      for (int cols=0; cols<MATRIX_COLS; cols++)
      {
        cliPrintf("%02d ", cols);
      }
      cliPrintf("\n");

      for (int rows=0; rows<MATRIX_ROWS; rows++)
      {
        cliPrintf("%02d : ", rows);

        for (int cols=0; cols<MATRIX_COLS; cols++)
        {
          if (keysGetPressed(rows, cols))
            cliPrintf("O  ");
          else
            cliPrintf("_  ");
        }
        cliPrintf("\n");
      }
      cliMoveUp(MATRIX_ROWS+1);
    }
    cliMoveDown(MATRIX_ROWS+1);

    cliShowCursor(true);
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("keys info\n");
  }
}
#endif

#endif