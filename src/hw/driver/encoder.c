#include "encoder.h"


#ifdef _USE_HW_ENCODER
#include "cli.h"



typedef struct 
{
  uint16_t gpio_a;
  uint16_t gpio_b;

  int32_t  count;
  int32_t  dir;
} encoder_tbl_t;



#ifdef _USE_HW_CLI
static void cliEncoder(cli_args_t *args);
#endif
static void encoderISR(uint8_t ch, uint16_t gpio_pin);
static bool encoderInitGpio(void);


static encoder_tbl_t encoder_tbl[ENCODER_MAX_CH] = 
{
  {GPIO_PIN_0, GPIO_PIN_1, 0,  1},
};

static bool is_init = false;





bool encoderInit(void)
{
  bool ret = true;

  for (int i=0; i<ENCODER_MAX_CH; i++)
  {
    encoder_tbl[i].count = 0;
  }

  ret = encoderInitGpio();
  
  is_init = ret;
  logPrintf("[%s] encoderInit()\n", ret ? "OK" : "NG");

#ifdef _USE_HW_CLI
  cliAdd("encoder", cliEncoder);
#endif  

  return true;
}

bool encoderInitGpio(void)
{
  GPIO_InitTypeDef   GPIO_InitStructure = {0,};


  __HAL_RCC_GPIOC_CLK_ENABLE();


  GPIO_InitStructure.Mode  = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStructure.Pull  = GPIO_NOPULL;
  GPIO_InitStructure.Pin   = GPIO_PIN_0;
  GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStructure);

  HAL_NVIC_SetPriority(EXTI0_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);  

  GPIO_InitStructure.Pin  = GPIO_PIN_1;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStructure);

  HAL_NVIC_SetPriority(EXTI1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);  

  return true;
}

bool encoderReset(void)
{
  for (int i=0; i<ENCODER_MAX_CH; i++)
  {
    encoder_tbl[i].count = 0;
  }
  return true;
}

bool encoderClearCount(uint8_t ch)
{
  if (ch >= ENCODER_MAX_CH) return false;

  encoder_tbl[ch].count = 0;

  return true;
}

int32_t encoderGetCount(uint8_t ch)
{
  int32_t count;

  if (ch >= ENCODER_MAX_CH)
    return 0;

  count = encoder_tbl[ch].count * encoder_tbl[ch].dir;

  return count;
}

bool encoderSetCount(uint8_t ch, int32_t count)
{
  if (ch >= ENCODER_MAX_CH)
    return 0;

  encoder_tbl[ch].count = count * encoder_tbl[ch].dir;
  
  return true;
}

void encoderISR(uint8_t ch, uint16_t gpio_pin)
{
  encoder_tbl_t *p_enc = &encoder_tbl[ch];
  uint8_t pin_a;
  uint8_t pin_b;

  pin_a = HAL_GPIO_ReadPin(GPIOC, p_enc->gpio_a);
  pin_b = HAL_GPIO_ReadPin(GPIOC, p_enc->gpio_b);


  if (gpio_pin == p_enc->gpio_a)
  {
    if (pin_a != pin_b)
      p_enc->count--;
    else
      p_enc->count++;
  }
  else
  {
    if (pin_a != pin_b)
      p_enc->count++;
    else
      p_enc->count--;
  }
}

void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin)
{
  encoderISR(_DEF_CH1, GPIO_Pin);
}

void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
  encoderISR(_DEF_CH1, GPIO_Pin);
}

void EXTI0_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}

void EXTI1_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_1);
}

#ifdef _USE_HW_CLI
void cliEncoder(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 1 && args->isStr(0, "info"))
  {
    while(cliKeepLoop())
    {
      for (int i=0; i<ENCODER_MAX_CH; i++)
      {
        cliPrintf("%8d ", encoderGetCount(i));
        cliPrintf("%d:%d",
                  HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_0),
                  HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_1));
      }
      cliPrintf("\n");
      delay(50);
    }
    ret = true;
  }
  
  if (ret != true)
  {
    cliPrintf("encoder info\n");
  }
}
#endif

#endif