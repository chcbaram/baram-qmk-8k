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





bool keysInit(void)
{

  pssiInit();

#if CLI_USE(HW_KEYS)
  cliAdd("keys", cliCmd);
#endif

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

void EXTI0_IRQHandler(void)   {  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);    keysUpdateEvent(); }
void EXTI1_IRQHandler(void)   {  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_1);    keysUpdateEvent(); }
void EXTI2_IRQHandler(void)   {  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_2);    keysUpdateEvent(); }
void EXTI3_IRQHandler(void)   {  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_3);    keysUpdateEvent(); }
void EXTI4_IRQHandler(void)   {  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_4);    keysUpdateEvent(); }
void EXTI5_IRQHandler(void)   {  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_5);    keysUpdateEvent(); }
void EXTI6_IRQHandler(void)   {  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_6);    keysUpdateEvent(); }
void EXTI7_IRQHandler(void)   {  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_7);    keysUpdateEvent(); }
void EXTI8_IRQHandler(void)   {  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_8);    keysUpdateEvent(); }
void EXTI9_IRQHandler(void)   {  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_9);    keysUpdateEvent(); }
void EXTI10_IRQHandler(void)  {  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_10);   keysUpdateEvent(); }
void EXTI11_IRQHandler(void)  {  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_11);   keysUpdateEvent(); }
void EXTI12_IRQHandler(void)  {  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_12);   keysUpdateEvent(); }
void EXTI13_IRQHandler(void)  {  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_13);   keysUpdateEvent(); }
void EXTI14_IRQHandler(void)  {  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_14);   keysUpdateEvent(); }
void EXTI15_IRQHandler(void)  {  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_15);   keysUpdateEvent(); }


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