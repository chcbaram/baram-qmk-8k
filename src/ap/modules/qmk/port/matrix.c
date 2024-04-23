#include "matrix.h"
#include "debounce.h"
#include "util.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "cli.h"
#include "key_scan/pssi.h"



/* matrix state(1:on, 0:off) */
static matrix_row_t raw_matrix[MATRIX_ROWS]; // raw values
static matrix_row_t matrix[MATRIX_ROWS];     // debounced values
static void         cliCmd(cli_args_t *args);






void matrix_init(void)
{
  memset(matrix, 0, sizeof(matrix));
  memset(raw_matrix, 0, sizeof(raw_matrix));

  debounce_init(MATRIX_ROWS);

  cliAdd("matrix", cliCmd);
}

void matrix_print(void)
{
  // print_matrix_header();

  // for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
  //     print_hex8(row);
  //     print(": ");
  //     print_matrix_row(row);
  //     print("\n");
  // }
}

matrix_row_t matrix_get_row(uint8_t row)
{
  return matrix[row];
}

uint8_t matrix_scan(void)
{
  matrix_row_t curr_matrix[MATRIX_ROWS] = {0};
  uint8_t cols_buf[MATRIX_COLS];


  pssiUpdate();
  pssiReadBuf(cols_buf, MATRIX_COLS);

  for (int rows=0; rows<MATRIX_ROWS; rows++)
  {
    for (int cols=0; cols<MATRIX_COLS; cols++)
    {
      uint8_t bit_data;

      bit_data = ~cols_buf[cols];

      if (bit_data & (1<<rows))
      {
        curr_matrix[rows] |= (1<<cols);
      }
    }
  }

  bool changed = memcmp(raw_matrix, curr_matrix, sizeof(curr_matrix)) != 0;
  if (changed) memcpy(raw_matrix, curr_matrix, sizeof(curr_matrix));

  changed = debounce(raw_matrix, matrix, MATRIX_ROWS, changed);

  return (uint8_t)changed;
}

void cliCmd(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 2 && args->isStr(0, "row"))
  {
    uint16_t data;

    data = args->getData(1);

    cliPrintf("row 0:0x%X\n", data);
    matrix[0] = data;
    delay(50);

    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("matrix row data\n");
  }
}