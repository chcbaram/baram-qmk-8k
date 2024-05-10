#ifndef PSSI_H_
#define PSSI_H_

#ifdef __cplusplus
extern "C" {
#endif


#include "hw_def.h"


bool pssiInit(void);
bool pssiUpdate(void);
bool pssiIsBusy(void);
bool pssiReadBuf(void *p_data, uint32_t length);


#ifdef __cplusplus
}
#endif

#endif