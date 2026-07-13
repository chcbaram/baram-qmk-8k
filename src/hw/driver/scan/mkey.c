#include "mkey.h"
#include "cli.h"
#include "cli_gui.h"
#include "spi.h"


#include QMK_KEYMAP_CONFIG_H


#define MKEY_BUF_MAX        32

// TIM3 자유진행 주기 = SCK 주기 = SYSCLK(160M)/BaudPrescaler(128) = 128 TIM count → ARR=127.
// (SPI BaudRatePrescaler 변경 시 함께 조정)
#ifndef MKEY_TIM_ARR
#define MKEY_TIM_ARR        127
#endif

// 샘플 지연 D = 주기(128) 내 CCR2. 기본 90%=115(시프트 경계 φ에서 최대로 멀어 안전).
// 50%=64, 60%=77, 75%=96, 90%=115.
#ifndef MKEY_SAMPLE_CCR
#define MKEY_SAMPLE_CCR     115
#endif

// 프레임 재무장(CH1_TCF=워드완료) 스큐로 캡처가 컬럼 한 칸 회전됨 → 순환 보정.
// 키가 오른쪽으로 밀리면 +1, 왼쪽이면 -1(=MATRIX_COLS-1).
#ifndef MKEY_COL_SHIFT
#define MKEY_COL_SHIFT      1
#endif


static void cliCmd(cli_args_t *args);
static bool mkeyInitTim(void);
static bool mkeyInitDma(void);
static bool mkeySpiStart(void);
static void mkeyDecode(uint8_t *p_data, uint32_t length);

static uint8_t spi_ch = _DEF_SPI1;
static bool is_enable = true;
static bool is_busy = false;


static TIM_HandleTypeDef  htim3;
static DMA_NodeTypeDef    Node_GPDMA1_Channel3;
static DMA_QListTypeDef   List_GPDMA1_Channel3;
static DMA_HandleTypeDef  handle_GPDMA1_Channel3;

// 캡처 원본: SCK 에지당 GPIOC->IDR 1워드. 인덱스=컬럼.
static uint32_t mkey_idr_buf[MKEY_BUF_MAX];
static uint32_t mkey_dma_pre_time = 0;
static uint32_t mkey_dma_exe_time = 0;
static bool     mkey_dma_req      = false;
static uint32_t mkey_dma_cnt      = 0;
static uint16_t spi_buf = (uint16_t)(~(1 << 15));
static volatile uint32_t mkey_sink = 0;   // 측정 루프 최적화 제거 방지




bool mkeyInit(void)
{
  bool ret = true;


  ret &= spiBegin(spi_ch);

  mkeyInitTim();
  mkeyInitDma();

  memset(mkey_idr_buf, 0xFF, sizeof(mkey_idr_buf));

  if (HAL_DMAEx_List_Start(&handle_GPDMA1_Channel3) != HAL_OK)
  {
    Error_Handler();
  }

  mkeySpiStart();
  delay(2);

  logPrintf("[%s] mkeyInit()\n", ret ? "OK":"NG");

  cliAdd("mkey", cliCmd);

  return true;
}

bool mkeyIsBusy(void)
{
  is_busy = mkey_dma_req;
  return is_busy;
}

bool mkeyUpdate(void)
{
  if (!is_enable)
    return false;

  is_busy = mkey_dma_req;

  if (!is_busy)
  {
    mkey_dma_req = true;
  }

  return is_busy;
}

bool mkeyReadBuf(void *p_data, uint32_t length)
{
  if (length > MATRIX_COLS)
    return false;

  mkeyDecode((uint8_t *)p_data, length);
  return true;
}

bool mkeySpiStart(void)
{
  mkey_dma_pre_time = micros();
  spiDmaTxStart(spi_ch, (uint8_t *)&spi_buf, 1);
  return true;
}

// GPIOC->IDR 워드에서 로우 비트를 뽑아 컬럼-major/로우-비트/active-low 바이트로 재조립.
// 60MX(5-row, 단일포트): row0..4 = PC6,7,8,9,11. 미사용 상위비트=1(안눌림).
static void mkeyDecode(uint8_t *p_data, uint32_t length)
{
  for (uint32_t col = 0; col < length; col++)
  {
    uint32_t src = col + MKEY_COL_SHIFT;
    uint32_t idr;

    if (src >= MATRIX_COLS)                 // % 대신 compare-sub (SHIFT < COLS 보장)
      src -= MATRIX_COLS;

    idr = mkey_idr_buf[src];

    // GPIOC IDR: row0..3 = bit6..9(연속), row4 = bit11 → 출력 bit0..4, 상위(row5..7)=1
    p_data[col] = 0xE0 | ((idr >> 6) & 0x0F) | ((idr >> 7) & 0x10);
  }
}

bool mkeyInitTim(void)
{
  TIM_OC_InitTypeDef sOC = {0};

  __HAL_RCC_TIM3_CLK_ENABLE();

  // 자유진행(SCK 슬레이브 아님): SCK와 동일 SYSCLK라 주파수 완전일치.
  // 주기=SCK주기(128count=800ns). 연속 SCK면 프레임=16주기 → 프레임당 CC2 정확히 16개.
  // 노이즈가 SCK 에지를 추가해도 TIM3는 무관 → 캡처수 불변 → 다음 프레임 자가치유.
  htim3.Instance               = TIM3;
  htim3.Init.Prescaler         = 0;                          // 160MHz → 6.25ns/count
  htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim3.Init.Period            = MKEY_TIM_ARR;               // 128count 주기
  htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_OC_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }

  // CCR2 = 샘플 지연 D. 컴페어 매치 → CC2 DMA request (핀 출력 없음).
  sOC.OCMode     = TIM_OCMODE_TIMING;
  sOC.Pulse      = MKEY_SAMPLE_CCR;
  sOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_OC_ConfigChannel(&htim3, &sOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }

  __HAL_TIM_ENABLE_DMA(&htim3, TIM_DMA_CC2);
  HAL_TIM_Base_Start(&htim3);

  return true;
}

bool mkeyInitDma(void)
{
  DMA_NodeConfTypeDef    NodeConfig    = {0};
  DMA_TriggerConfTypeDef TriggerConfig = {0};
  GPIO_InitTypeDef       GPIO_InitStruct = {0};

  __HAL_RCC_GPDMA1_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  // (TIM3는 자유진행이라 SCK 입력핀 PA6 불필요)
  // PC6/7/8/9/11 = 로우 입력 (외부 1kΩ 풀업)
  GPIO_InitStruct.Pin   = GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_11;
  GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  // 캡처 노드: TIM3_CH2 request 페이싱, GPIOC->IDR(fixed word) → buf(inc word), block=MATRIX_COLS
  NodeConfig.NodeType                       = DMA_GPDMA_LINEAR_NODE;
  NodeConfig.Init.Request                   = GPDMA1_REQUEST_TIM3_CH2;
  NodeConfig.Init.BlkHWRequest              = DMA_BREQ_SINGLE_BURST;
  NodeConfig.Init.Direction                 = DMA_PERIPH_TO_MEMORY;
  NodeConfig.Init.SrcInc                    = DMA_SINC_FIXED;
  NodeConfig.Init.DestInc                   = DMA_DINC_INCREMENTED;
  NodeConfig.Init.SrcDataWidth              = DMA_SRC_DATAWIDTH_WORD;
  NodeConfig.Init.DestDataWidth             = DMA_DEST_DATAWIDTH_WORD;
  NodeConfig.Init.SrcBurstLength            = 1;
  NodeConfig.Init.DestBurstLength           = 1;
  NodeConfig.Init.TransferAllocatedPort     = DMA_SRC_ALLOCATED_PORT0|DMA_DEST_ALLOCATED_PORT1;
  NodeConfig.Init.TransferEventMode         = DMA_TCEM_BLOCK_TRANSFER;
  NodeConfig.Init.Mode                      = DMA_NORMAL;
  NodeConfig.TriggerConfig.TriggerPolarity  = DMA_TRIG_POLARITY_MASKED;
  NodeConfig.DataHandlingConfig.DataExchange  = DMA_EXCHANGE_NONE;
  NodeConfig.DataHandlingConfig.DataAlignment = DMA_DATA_RIGHTALIGN_ZEROPADDED;
  NodeConfig.SrcAddress                     = (uint32_t)&GPIOC->IDR;
  NodeConfig.DstAddress                     = (uint32_t)mkey_idr_buf;
  NodeConfig.DataSize                       = MATRIX_COLS * 4;   // words → bytes
  if (HAL_DMAEx_List_BuildNode(&NodeConfig, &Node_GPDMA1_Channel3) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_DMAEx_List_InsertNode(&List_GPDMA1_Channel3, NULL, &Node_GPDMA1_Channel3) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_DMAEx_List_SetCircularMode(&List_GPDMA1_Channel3) != HAL_OK)
  {
    Error_Handler();
  }

  handle_GPDMA1_Channel3.Instance                         = GPDMA1_Channel3;
  handle_GPDMA1_Channel3.InitLinkedList.Priority          = DMA_LOW_PRIORITY_LOW_WEIGHT;
  handle_GPDMA1_Channel3.InitLinkedList.LinkStepMode      = DMA_LSM_FULL_EXECUTION;
  handle_GPDMA1_Channel3.InitLinkedList.LinkAllocatedPort = DMA_LINK_ALLOCATED_PORT1;
  handle_GPDMA1_Channel3.InitLinkedList.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
  handle_GPDMA1_Channel3.InitLinkedList.LinkedListMode    = DMA_LINKEDLIST_CIRCULAR;
  if (HAL_DMAEx_List_Init(&handle_GPDMA1_Channel3) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_DMAEx_List_LinkQ(&handle_GPDMA1_Channel3, &List_GPDMA1_Channel3) != HAL_OK)
  {
    Error_Handler();
  }

  // 프레임 트리거: SPI TX-DMA(Ch1) 완료 = 프레임 경계 → block 재무장(자가치유)
  TriggerConfig.TriggerMode      = DMA_TRIGM_BLOCK_TRANSFER;
  TriggerConfig.TriggerPolarity  = DMA_TRIG_POLARITY_RISING;
  TriggerConfig.TriggerSelection = GPDMA1_TRIGGER_GPDMA1_CH1_TCF;
  if (HAL_DMAEx_ConfigTrigger(&handle_GPDMA1_Channel3, &TriggerConfig) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_DMA_ConfigChannelAttributes(&handle_GPDMA1_Channel3, DMA_CHANNEL_NPRIV) != HAL_OK)
  {
    Error_Handler();
  }

  return true;
}

void GPDMA1_Channel3_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&handle_GPDMA1_Channel3);

  mkey_dma_req = false;
  mkey_dma_cnt++;
  mkey_dma_exe_time = micros() - mkey_dma_pre_time;
}

void cliCmd(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 1 && args->isStr(0, "info"))
  {
    mkey_dma_cnt = 0;
    delay(1000);
    cliPrintf("mkey_cnt : %d\n", mkey_dma_cnt);
    cliPrintf("ccr2     : %d\n", MKEY_SAMPLE_CCR);
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "time"))
  {
    uint8_t  cols_buf[MATRIX_COLS];
    uint32_t curr_matrix[MATRIX_ROWS];
    uint32_t c0, t_read, t_unpack;
    uint32_t mhz = SystemCoreClock / 1000000;

    // DWT 사이클 카운터 enable (6.25ns @160MHz)
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL       |= DWT_CTRL_CYCCNTENA_Msk;

    // stage1: 캡처버퍼 디코드 (IDR->cols_buf), 1000회 평균
    c0 = DWT->CYCCNT;
    for (int i = 0; i < 1000; i++)
    {
      mkeyReadBuf(cols_buf, MATRIX_COLS);
      mkey_sink += cols_buf[0];
    }
    t_read = (DWT->CYCCNT - c0) / 1000;

    // stage2: 매트릭스 언팩 (cols_buf->row 비트), matrix.c(:66-82)와 동일
    c0 = DWT->CYCCNT;
    for (int i = 0; i < 1000; i++)
    {
      for (uint32_t r = 0; r < MATRIX_ROWS; r++)
      {
        uint32_t row_data = 0;
        for (uint32_t c = 0; c < MATRIX_COLS; c++)
        {
          if ((cols_buf[c] & (1 << r)) == 0)
            row_data |= (1 << c);
        }
        curr_matrix[r] = row_data;
      }
      mkey_sink += curr_matrix[0];
    }
    t_unpack = (DWT->CYCCNT - c0) / 1000;

    cliPrintf("rows x cols       : %d x %d\n", MATRIX_ROWS, MATRIX_COLS);
    cliPrintf("read  (IDR->cols) : %4d cyc (%d ns)\n", t_read,   t_read   * 1000 / mhz);
    cliPrintf("unpack(cols->row) : %4d cyc (%d ns)\n", t_unpack, t_unpack * 1000 / mhz);
    cliPrintf("matrix decode tot : %4d cyc (%d ns)\n", t_read+t_unpack, (t_read+t_unpack) * 1000 / mhz);
    ret = true;
  }

  if (args->argc >= 1 && args->isStr(0, "ccr"))
  {
    if (args->argc == 2)
    {
      int32_t v = args->getData(1);
      if (v < 1)            v = 1;
      if (v > MKEY_TIM_ARR) v = MKEY_TIM_ARR;
      __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, (uint32_t)v);   // 라이브 샘플위치 변경
    }
    uint32_t cur = __HAL_TIM_GET_COMPARE(&htim3, TIM_CHANNEL_2);
    cliPrintf("ccr2 : %d / %d  (%d%%)\n", cur, MKEY_TIM_ARR + 1,
              cur * 100 / (MKEY_TIM_ARR + 1));
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "spi"))
  {
    uint8_t cols_buf[MATRIX_COLS];

    cliShowCursor(false);

    while(cliKeepLoop())
    {
      mkeyReadBuf(cols_buf, MATRIX_COLS);
      delay(10);

      cliPrintf("     ");
      for (int cols=0; cols<MATRIX_COLS; cols++)
      {
        cliPrintf("%02d ", cols);
      }
      cliPrintf("\n");

      for (int rows=0; rows<MATRIX_ROWS; rows++)
      {
        cliPrintf("%02d : ", rows);

        for (int cols=0; cols<MATRIX_COLS; cols++)
        {
          if (cols_buf[cols] & (1<<rows))
            cliPrintf("_  ");
          else
            cliPrintf("O  ");
        }
        cliPrintf("\n");
      }
      cliMoveUp(MATRIX_ROWS+1);
    }
    cliMoveDown(MATRIX_ROWS+1);

    cliShowCursor(true);

    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("mkey info\n");
    cliPrintf("mkey time\n");
    cliPrintf("mkey ccr [1~%d]\n", MKEY_TIM_ARR);
    cliPrintf("mkey spi\n");
  }
}
