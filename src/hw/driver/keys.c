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

  if (!pssiIsBusy())
  {
    pssiReadBuf(cols_buf, MATRIX_COLS);
  }
  return ret;
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