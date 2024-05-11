#ifndef KEYS_H_
#define KEYS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "hw_def.h"




bool keysInit(void);
bool keysIsBusy(void);
bool keysUpdate(void);
bool keysGetPressed(uint16_t row, uint16_t col);


#ifdef __cplusplus
}
#endif

#endif