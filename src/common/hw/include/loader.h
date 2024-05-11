#ifndef LOADER_H_
#define LOADER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "hw_def.h"


enum 
{
  LOADER_OK,
  LOADER_ERR_DATA_ERASE,
  LOADER_ERR_DATA_WRITE,
  LOADER_ERR_END_WRITE,
  LOADER_ERR_CANCEL,
  LOADER_ERR_TIMEOUT,
  LOADER_ERR_ERROR,
};


bool loaderInit(void);

#ifdef __cplusplus
}
#endif

#endif