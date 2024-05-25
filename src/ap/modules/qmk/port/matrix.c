#include "matrix.h"
#include "debounce.h"
#include "keyboard.h"
#include "util.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "cli.h"
#include "usb.h"
#include "keys.h"


/* matrix state(1:on, 0:off) */
static matrix_row_t raw_matrix[MATRIX_ROWS]; // raw values
static matrix_row_t matrix[MATRIX_ROWS];     // debounced values
static bool         is_info_enable = false;
static uint32_t     key_scan_time  = 0;

static void cliCmd(cli_args_t *args);
static void matrix_info(void);





void matrix_init(void)
{
  memset(matrix, 0, sizeof(matrix));
  memset(raw_matrix, 0, sizeof(raw_matrix));

  debounce_init(MATRIX_ROWS);

  cliAdd("matrix", cliCmd);
}

void matrix_print(void)
{
}

bool matrix_can_read(void) 
{
  return true;
}

matrix_row_t matrix_get_row(uint8_t row)
{
  return matrix[row];
}

uint8_t matrix_scan(void)
{
  matrix_row_t curr_matrix[MATRIX_ROWS] = {0};
  uint32_t pre_time;  
  bool changed;


  pre_time = micros();

  #if 1
  uint8_t curr_cols[MATRIX_COLS];
  uint32_t row_data;

  keysReadBuf(curr_cols, MATRIX_COLS);

  for (uint32_t rows=0; rows<MATRIX_ROWS; rows++)
  {
    row_data = 0; 
    for (uint32_t cols=0; cols<MATRIX_COLS; cols++)
    {
      if ((curr_cols[cols] & (1<<rows)) == 0x00)
      {
        row_data |= (1<<cols);
      }
    }
    curr_matrix[rows] = row_data;
  }
  #else
  keysUpdate();
  for (uint32_t rows=0; rows<MATRIX_ROWS; rows++)
  {
    for (uint32_t cols=0; cols<MATRIX_COLS; cols++)
    {
      curr_matrix[rows] |= (keysGetPressed(rows, cols)<<cols);
    }
  }
  #endif

  key_scan_time = micros() - pre_time;


  changed = memcmp(raw_matrix, curr_matrix, sizeof(curr_matrix)) != 0;
  if (changed)
  {
    memcpy(raw_matrix, curr_matrix, sizeof(curr_matrix));
  }

  changed = debounce(raw_matrix, matrix, MATRIX_ROWS, changed);
  if (changed)
  {
    usbHidSetTimeLog(0, pre_time);
  }
  matrix_info();

  return (uint8_t)changed;
}

void matrix_info(void)
{
#ifdef DEBUG_MATRIX_SCAN_RATE
  static uint32_t pre_time = 0;

  if (is_info_enable)
  {
    if (millis()-pre_time >= 1000)
    {
      pre_time = millis();
      usb_hid_rate_info_t hid_info;

      usbHidGetRateInfo(&hid_info);
      
      logPrintf("Scan Rate : %d.%d KHz\n", get_matrix_scan_rate()/1000, get_matrix_scan_rate()%1000);
      logPrintf("Poll Rate : %d Hz, %d us(max), %d us(min)\n",
                hid_info.freq_hz,
                hid_info.time_max,
                hid_info.time_min);
      logPrintf("Scan Time : %d us\n", key_scan_time);
    }
  }
#endif
}

void cliCmd(cli_args_t *args)
{
  bool ret = false;

  if (args->argc == 1 && args->isStr(0, "info"))
  {
    cliPrintf("is_info_enable : %s\n", is_info_enable ? "on":"off");

    usb_hid_rate_info_t hid_info;

    usbHidGetRateInfo(&hid_info);
    
    logPrintf("Scan Rate : %d.%d KHz\n", get_matrix_scan_rate()/1000, get_matrix_scan_rate()%1000);
    logPrintf("Poll Rate : %d Hz, %d us(max), %d us(min)\n",
              hid_info.freq_hz,
              hid_info.time_max,
              hid_info.time_min);
    logPrintf("Scan Time : %d us\n", key_scan_time);

    ret = true;
  }

  if (args->argc == 2 && args->isStr(0, "row"))
  {
    uint16_t data;

    data = args->getData(1);

    cliPrintf("row 0:0x%X\n", data);
    matrix[0] = data;
    delay(50);

    ret = true;
  }

  if (args->argc == 2 && args->isStr(0, "info"))
  {
    if (args->isStr(1, "on"))
    {
      is_info_enable = true;
    }
    if (args->isStr(1, "off"))
    {
      is_info_enable = false;
    }
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("matrix info\n");
    cliPrintf("matrix row data\n");
    #ifdef DEBUG_MATRIX_SCAN_RATE
    cliPrintf("matrix info on:off\n");
    #endif    
  }
}