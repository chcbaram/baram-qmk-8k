#ifndef MKEY_H_
#define MKEY_H_

#ifdef __cplusplus
extern "C" {
#endif


#include "hw_def.h"


bool mkeyInit(void);
bool mkeyUpdate(void);
bool mkeyIsBusy(void);
bool mkeyReadBuf(void *p_data, uint32_t length);


#ifdef __cplusplus
}
#endif

#endif
