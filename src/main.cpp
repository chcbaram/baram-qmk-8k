#include "main.h"




int main(void)
{
  bspInit();
  
  hwInit();
  apInit();
  apMain();

  return 0;
}

