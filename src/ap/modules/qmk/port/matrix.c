#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "util.h"
#include "matrix.h"
#include "debounce.h"



/* matrix state(1:on, 0:off) */
static matrix_row_t raw_matrix[MATRIX_ROWS]; // raw values
static matrix_row_t matrix[MATRIX_ROWS];     // debounced values



void matrix_init(void)
{
  memset(matrix, 0, sizeof(matrix));
  memset(raw_matrix, 0, sizeof(raw_matrix));

  debounce_init(MATRIX_ROWS);
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
    // Matrix mask lets you disable switches in the returned matrix data. For example, if you have a
    // switch blocker installed and the switch is always pressed.
#ifdef MATRIX_MASKED
    return matrix[row] & matrix_mask[row];
#else
    return matrix[row];
#endif
}

uint8_t matrix_scan(void)
{
  matrix_row_t curr_matrix[MATRIX_ROWS] = {0};

  bool changed = memcmp(raw_matrix, curr_matrix, sizeof(curr_matrix)) != 0;
  if (changed) memcpy(raw_matrix, curr_matrix, sizeof(curr_matrix));

  changed = debounce(raw_matrix, matrix, MATRIX_ROWS, changed);

  return (uint8_t)changed;
}
