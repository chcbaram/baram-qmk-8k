#include "ws2812.h"



#ifdef _USE_HW_WS2812
#include "cli.h"

#define BIT_PERIOD      (104) // 1300ns, 80Mhz
#define BIT_HIGH        (56)  // 700ns
#define BIT_LOW         (28)  // 350ns
#define BIT_ZERO        (50)

bool is_init = false;


typedef struct
{
  TIM_HandleTypeDef *h_timer;  
  uint32_t channel;
  uint16_t led_cnt;
} ws2812_t;

static uint8_t bit_buf[BIT_ZERO + 24*(HW_WS2812_MAX_CH+1)];


ws2812_t ws2812;
static TIM_HandleTypeDef htim15;
static DMA_HandleTypeDef handle_GPDMA1_Channel4;


#if CLI_USE(HW_WS2812)
static void cliCmd(cli_args_t *args);
#endif
static bool ws2812InitHw(void);





bool ws2812Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};


  memset(bit_buf, 0, sizeof(bit_buf));
  
  ws2812.h_timer = &htim15;
  ws2812.channel = TIM_CHANNEL_1;

  // Timer 
  //
  __HAL_RCC_GPDMA1_CLK_ENABLE();
  __HAL_RCC_TIM15_CLK_ENABLE();

  htim15.Instance               = TIM15;
  htim15.Init.Prescaler         = 1;
  htim15.Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim15.Init.Period            = BIT_PERIOD-1;
  htim15.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  htim15.Init.RepetitionCounter = 0;
  htim15.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim15) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim15, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim15) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim15, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode       = TIM_OCMODE_PWM1;
  sConfigOC.Pulse        = 0;
  sConfigOC.OCPolarity   = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode   = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState  = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim15, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode  = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel        = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime         = 0;
  sBreakDeadTimeConfig.BreakState       = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity    = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.BreakFilter      = 0;
  sBreakDeadTimeConfig.AutomaticOutput  = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim15, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }

  ws2812InitHw();


  ws2812.led_cnt = WS2812_MAX_CH;
  is_init = true;

  for (int i=0; i<WS2812_MAX_CH; i++)
  {
    ws2812SetColor(i, WS2812_COLOR_OFF);
  }
  ws2812Refresh();

#if CLI_USE(HW_WS2812)
  cliAdd("ws2812", cliCmd);
#endif
  return true;
}

bool ws2812InitHw(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};


  __HAL_RCC_GPIOA_CLK_ENABLE();
  /**TIM15 GPIO Configuration
  PA2     ------> TIM15_CH1
  */
  GPIO_InitStruct.Pin       = GPIO_PIN_2;
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull      = GPIO_NOPULL;
  GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF14_TIM15;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);


  /* TIM15 DMA Init */
  /* GPDMA1_REQUEST_TIM15_CH1 Init */
  handle_GPDMA1_Channel4.Instance                   = GPDMA1_Channel4;
  handle_GPDMA1_Channel4.Init.Request               = GPDMA1_REQUEST_TIM15_CH1;
  handle_GPDMA1_Channel4.Init.BlkHWRequest          = DMA_BREQ_SINGLE_BURST;
  handle_GPDMA1_Channel4.Init.Direction             = DMA_PERIPH_TO_MEMORY;
  handle_GPDMA1_Channel4.Init.SrcInc                = DMA_SINC_INCREMENTED;
  handle_GPDMA1_Channel4.Init.DestInc               = DMA_DINC_FIXED;
  handle_GPDMA1_Channel4.Init.SrcDataWidth          = DMA_SRC_DATAWIDTH_BYTE;
  handle_GPDMA1_Channel4.Init.DestDataWidth         = DMA_DEST_DATAWIDTH_BYTE;
  handle_GPDMA1_Channel4.Init.Priority              = DMA_LOW_PRIORITY_LOW_WEIGHT;
  handle_GPDMA1_Channel4.Init.SrcBurstLength        = 1;
  handle_GPDMA1_Channel4.Init.DestBurstLength       = 1;
  handle_GPDMA1_Channel4.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0 | DMA_DEST_ALLOCATED_PORT0;
  handle_GPDMA1_Channel4.Init.TransferEventMode     = DMA_TCEM_BLOCK_TRANSFER;
  handle_GPDMA1_Channel4.Init.Mode                  = DMA_NORMAL;
  if (HAL_DMA_Init(&handle_GPDMA1_Channel4) != HAL_OK)
  {
    Error_Handler();
  }

  __HAL_LINKDMA(&htim15, hdma[TIM_DMA_ID_CC1], handle_GPDMA1_Channel4);

  if (HAL_DMA_ConfigChannelAttributes(&handle_GPDMA1_Channel4, DMA_CHANNEL_NPRIV) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_NVIC_SetPriority(GPDMA1_Channel4_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(GPDMA1_Channel4_IRQn);

  return true;
}

bool ws2812Refresh(void)
{
  HAL_TIM_PWM_Stop_DMA(ws2812.h_timer, ws2812.channel);
  HAL_TIM_PWM_Start_DMA(ws2812.h_timer, ws2812.channel,  (const uint32_t *)bit_buf, sizeof(bit_buf));
  return true;
}

void ws2812SetColor(uint32_t ch, uint32_t color)
{
  uint8_t r_bit[8];
  uint8_t g_bit[8];
  uint8_t b_bit[8];
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint32_t offset;

  if (ch >= WS2812_MAX_CH)
    return;

  red   = (color >> 16) & 0xFF;
  green = (color >> 8) & 0xFF;
  blue  = (color >> 0) & 0xFF;


  for (int i=0; i<8; i++)
  {
    if (red & (1<<7))
    {
      r_bit[i] = BIT_HIGH;
    }
    else
    {
      r_bit[i] = BIT_LOW;
    }
    red <<= 1;

    if (green & (1<<7))
    {
      g_bit[i] = BIT_HIGH;
    }
    else
    {
      g_bit[i] = BIT_LOW;
    }
    green <<= 1;

    if (blue & (1<<7))
    {
      b_bit[i] = BIT_HIGH;
    }
    else
    {
      b_bit[i] = BIT_LOW;
    }
    blue <<= 1;
  }

  offset = BIT_ZERO;

  memcpy(&bit_buf[offset + ch*24 + 8*0], g_bit, 8*1);
  memcpy(&bit_buf[offset + ch*24 + 8*1], r_bit, 8*1);
  memcpy(&bit_buf[offset + ch*24 + 8*2], b_bit, 8*1);
}

void GPDMA1_Channel4_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&handle_GPDMA1_Channel4);
}

#if CLI_USE(HW_WS2812)
void cliCmd(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 1 && args->isStr(0, "info"))
  {
    cliPrintf("ws2812 led cnt : %d\n", WS2812_MAX_CH);
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "test"))
  {
    uint32_t color[6] = {WS2812_COLOR_RED,
                         WS2812_COLOR_OFF,
                         WS2812_COLOR_GREEN,
                         WS2812_COLOR_OFF,
                         WS2812_COLOR_BLUE,
                         WS2812_COLOR_OFF};

    uint8_t color_idx = 0;

    while(cliKeepLoop())
    {
      ws2812SetColor(0, color[color_idx]);
      ws2812Refresh();
      color_idx = (color_idx + 1) % 6;

      delay(500);
    }

    for (int i=0; i<WS2812_MAX_CH; i++)
    {
      ws2812SetColor(i, WS2812_COLOR_OFF);
    }
    ws2812Refresh();

    ret = true;
  }


  if (args->argc == 5 && args->isStr(0, "color"))
  {
    uint8_t  ch;
    uint8_t red;
    uint8_t green;
    uint8_t blue;

    ch    = (uint8_t)args->getData(1);
    red   = (uint8_t)args->getData(2);
    green = (uint8_t)args->getData(3);
    blue  = (uint8_t)args->getData(4);

    ws2812SetColor(ch, WS2812_COLOR(red, green, blue));
    ws2812Refresh();

    while(cliKeepLoop())
    {
    }
    ws2812SetColor(0, 0);
    ws2812Refresh();
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("ws2812 info\n");
    cliPrintf("ws2812 test\n");
    cliPrintf("ws2812 color ch r g b\n");
  }
}
#endif

#endif