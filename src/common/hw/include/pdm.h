#ifndef PDM_H_
#define PDM_H_

#ifdef __cplusplus
extern "C" {
#endif


#include "hw_def.h"

#ifdef _USE_HW_PDM


#define PDM_MIC_MAX_CH  HW_PDM_MIC_MAX_CH


typedef struct
{
  int16_t R;
  int16_t L;
} pcm_data_t;

bool pdmInit(void);
bool pdmIsInit(void);
bool pdmBegin(void);
bool pdmEnd(void);

uint32_t pdmAvailable(void);
uint32_t pdmGetSampleRate(void);
bool pdmRead(pcm_data_t *p_buf, uint32_t length);

bool pdmRecordStart(pcm_data_t *p_buf, uint32_t length);
bool pdmRecordStop(void);
bool pdmRecordIsBusy(void);
bool pdmRecordIsDone(void);
uint32_t pdmRecordGetLength(void);
uint32_t pdmGetTimeToLengh(uint32_t ms);

bool pdmDirEnable(void);
bool pdmDirDisable(void);
int32_t pdmDirGetAngle(void);

#endif


#ifdef __cplusplus
}
#endif

#endif