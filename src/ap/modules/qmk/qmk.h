#ifndef QMK_H_
#define QMK_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ap_def.h"


#include "quantum.h"


bool qmkInit(void);
void qmkUpdate(void);


#ifdef __cplusplus
}
#endif

#endif