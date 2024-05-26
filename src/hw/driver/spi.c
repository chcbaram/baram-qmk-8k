#include "spi.h"



#ifdef _USE_HW_SPI
#include "stm32u5xx_ll_spi.h"

#define SPI_TX_DMA_MAX_LENGTH   0xFFFF




typedef struct
{
  bool is_open;
  bool is_tx_done;
  bool is_rx_done;
  bool is_error;

  void (*func_tx)(void);

  SPI_HandleTypeDef *h_spi;
  DMA_HandleTypeDef *h_dma_tx;
  DMA_HandleTypeDef *h_dma_rx;
} spi_t;



static spi_t spi_tbl[SPI_MAX_CH];

static SPI_HandleTypeDef hspi1;
static DMA_NodeTypeDef Node_GPDMA1_Channel1;
static DMA_QListTypeDef List_GPDMA1_Channel1;
static DMA_HandleTypeDef handle_GPDMA1_Channel1;





bool spiInit(void)
{
  bool ret = true;


  for (int i=0; i<SPI_MAX_CH; i++)
  {
    spi_tbl[i].is_open = false;
    spi_tbl[i].is_tx_done = true;
    spi_tbl[i].is_rx_done = true;
    spi_tbl[i].is_error = false;
    spi_tbl[i].func_tx = NULL;
    spi_tbl[i].h_dma_rx = NULL;
    spi_tbl[i].h_dma_tx = NULL;
  }

  return ret;
}

bool spiBegin(uint8_t ch)
{
  bool ret = false;
  spi_t *p_spi = &spi_tbl[ch];

  switch(ch)
  {
    case _DEF_SPI1:
      p_spi->h_spi = &hspi1;
      p_spi->h_dma_tx = &handle_GPDMA1_Channel1;

      p_spi->h_spi->Instance              = SPI1;
      p_spi->h_spi->Init.Mode             = SPI_MODE_MASTER;
      p_spi->h_spi->Init.Direction        = SPI_DIRECTION_1LINE;
      p_spi->h_spi->Init.DataSize         = SPI_DATASIZE_16BIT;
      p_spi->h_spi->Init.CLKPolarity      = SPI_POLARITY_LOW;
      p_spi->h_spi->Init.CLKPhase         = SPI_PHASE_1EDGE;
      p_spi->h_spi->Init.NSS              = SPI_NSS_HARD_OUTPUT;
      p_spi->h_spi->Init.BaudRatePrescaler= SPI_BAUDRATEPRESCALER_128; // 128=1.25Mhz, 64:2.5Mhz 
      p_spi->h_spi->Init.FirstBit         = SPI_FIRSTBIT_MSB;
      p_spi->h_spi->Init.TIMode           = SPI_TIMODE_DISABLE;
      p_spi->h_spi->Init.CRCCalculation   = SPI_CRCCALCULATION_DISABLE;
      p_spi->h_spi->Init.CRCPolynomial    = 0;

      p_spi->h_spi->Init.NSSPMode                   = SPI_NSS_PULSE_ENABLE;
      p_spi->h_spi->Init.NSSPolarity                = SPI_NSS_POLARITY_HIGH;
      p_spi->h_spi->Init.FifoThreshold              = SPI_FIFO_THRESHOLD_01DATA;
      p_spi->h_spi->Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
      p_spi->h_spi->Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
      p_spi->h_spi->Init.MasterSSIdleness           = SPI_MASTER_SS_IDLENESS_02CYCLE;
      p_spi->h_spi->Init.MasterInterDataIdleness    = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
      p_spi->h_spi->Init.MasterReceiverAutoSusp     = SPI_MASTER_RX_AUTOSUSP_DISABLE;
      p_spi->h_spi->Init.MasterKeepIOState          = SPI_MASTER_KEEP_IO_STATE_DISABLE;
      p_spi->h_spi->Init.IOSwap                     = SPI_IO_SWAP_DISABLE;
      p_spi->h_spi->Init.ReadyMasterManagement      = SPI_RDY_MASTER_MANAGEMENT_INTERNALLY;
      p_spi->h_spi->Init.ReadyPolarity              = SPI_RDY_POLARITY_HIGH;

      __HAL_RCC_GPDMA1_CLK_ENABLE();

      HAL_SPI_DeInit(p_spi->h_spi);
      if (HAL_SPI_Init(p_spi->h_spi) == HAL_OK)
      {
        p_spi->is_open = true;
        ret = true;
      }

      break;
  }

  return ret;
}

void spiSetDataMode(uint8_t ch, uint8_t dataMode)
{
  spi_t  *p_spi = &spi_tbl[ch];


  if (p_spi->is_open == false) return;


  switch( dataMode )
  {
    // CPOL=0, CPHA=0
    case SPI_MODE0:
      p_spi->h_spi->Init.CLKPolarity = SPI_POLARITY_LOW;
      p_spi->h_spi->Init.CLKPhase    = SPI_PHASE_1EDGE;
      HAL_SPI_Init(p_spi->h_spi);
      break;

    // CPOL=0, CPHA=1
    case SPI_MODE1:
      p_spi->h_spi->Init.CLKPolarity = SPI_POLARITY_LOW;
      p_spi->h_spi->Init.CLKPhase    = SPI_PHASE_2EDGE;
      HAL_SPI_Init(p_spi->h_spi);
      break;

    // CPOL=1, CPHA=0
    case SPI_MODE2:
      p_spi->h_spi->Init.CLKPolarity = SPI_POLARITY_HIGH;
      p_spi->h_spi->Init.CLKPhase    = SPI_PHASE_1EDGE;
      HAL_SPI_Init(p_spi->h_spi);
      break;

    // CPOL=1, CPHA=1
    case SPI_MODE3:
      p_spi->h_spi->Init.CLKPolarity = SPI_POLARITY_HIGH;
      p_spi->h_spi->Init.CLKPhase    = SPI_PHASE_2EDGE;
      HAL_SPI_Init(p_spi->h_spi);
      break;
  }
}

void spiSetBitWidth(uint8_t ch, uint8_t bit_width)
{
  spi_t  *p_spi = &spi_tbl[ch];

  if (p_spi->is_open == false) return;

  

  switch(bit_width)
  {
    case 9:
      p_spi->h_spi->Init.DataSize = SPI_DATASIZE_9BIT;
      LL_SPI_SetDataWidth(p_spi->h_spi->Instance, LL_SPI_DATAWIDTH_9BIT);
      break;

    case 16:
      p_spi->h_spi->Init.DataSize = SPI_DATASIZE_16BIT;
      LL_SPI_SetDataWidth(p_spi->h_spi->Instance, LL_SPI_DATAWIDTH_16BIT);
      break;

    default:
      p_spi->h_spi->Init.DataSize = SPI_DATASIZE_8BIT;
      LL_SPI_SetDataWidth(p_spi->h_spi->Instance, LL_SPI_DATAWIDTH_8BIT);
      break;
  }
}

uint8_t spiTransfer8(uint8_t ch, uint8_t data)
{
  uint8_t ret;
  spi_t  *p_spi = &spi_tbl[ch];


  if (p_spi->is_open == false) return 0;

  HAL_SPI_TransmitReceive(p_spi->h_spi, &data, &ret, 1, 10);

  return ret;
}

uint16_t spiTransfer16(uint8_t ch, uint16_t data)
{
  uint8_t tBuf[2];
  uint8_t rBuf[2];
  uint16_t ret;
  spi_t  *p_spi = &spi_tbl[ch];


  if (p_spi->is_open == false) return 0;

  if (p_spi->h_spi->Init.DataSize == SPI_DATASIZE_8BIT)
  {
    tBuf[1] = (uint8_t)data;
    tBuf[0] = (uint8_t)(data>>8);
    HAL_SPI_TransmitReceive(p_spi->h_spi, (uint8_t *)&tBuf, (uint8_t *)&rBuf, 2, 10);

    ret = rBuf[0];
    ret <<= 8;
    ret += rBuf[1];
  }
  else
  {
    HAL_SPI_TransmitReceive(p_spi->h_spi, (uint8_t *)&data, (uint8_t *)&ret, 1, 10);
  }

  return ret;
}

bool spiTransfer(uint8_t ch, uint8_t *tx_buf, uint8_t *rx_buf, uint32_t length, uint32_t timeout)
{
  bool ret = true;
  HAL_StatusTypeDef status;
  spi_t  *p_spi = &spi_tbl[ch];

  if (p_spi->is_open == false) return false;

  if (rx_buf == NULL)
  {
    status =  HAL_SPI_Transmit(p_spi->h_spi, tx_buf, length, timeout);
  }
  else if (tx_buf == NULL)
  {
    status =  HAL_SPI_Receive(p_spi->h_spi, rx_buf, length, timeout);
  }
  else
  {
    status =  HAL_SPI_TransmitReceive(p_spi->h_spi, tx_buf, rx_buf, length, timeout);
  }

  if (status != HAL_OK)
  {
    return false;
  }

  return ret;
}

bool spiTransferDMA(uint8_t ch, uint8_t *tx_buf, uint8_t *rx_buf, uint32_t length, uint32_t timeout)
{
  bool ret = false;
  HAL_StatusTypeDef status;
  spi_t  *p_spi = &spi_tbl[ch];
  bool is_dma = false;

  if (p_spi->is_open == false) return false;

  if (rx_buf == NULL)
  {
    status = HAL_SPI_Transmit(p_spi->h_spi, tx_buf, length, timeout);
  }
  else if (tx_buf == NULL)
  {
    p_spi->is_rx_done = false;
    status = HAL_SPI_Receive_DMA(p_spi->h_spi, rx_buf, length);
    is_dma = true;
  }
  else
  {
    status = HAL_SPI_TransmitReceive(p_spi->h_spi, tx_buf, rx_buf, length, timeout);
  }

  if (status == HAL_OK)
  {
    uint32_t pre_time;

    ret = true;
    pre_time = millis();
    if (is_dma == true)
    {
      while(1)
      {
        if(p_spi->is_rx_done == true)
          break;

        if((millis()-pre_time) >= timeout)
        {
          ret = false;
          break;
        }
      }
    }
  }

  return ret;
}

void spiDmaTxStart(uint8_t spi_ch, uint8_t *p_buf, uint32_t length)
{
  spi_t  *p_spi = &spi_tbl[spi_ch];

  if (p_spi->is_open == false) return;

  p_spi->is_tx_done = false;
  HAL_SPI_Transmit_DMA(p_spi->h_spi, p_buf, length);
}

bool spiDmaTxTransfer(uint8_t ch, void *buf, uint32_t length, uint32_t timeout)
{
  bool ret = true;
  uint32_t t_time;


  spiDmaTxStart(ch, (uint8_t *)buf, length);

  t_time = millis();

  if (timeout == 0) return true;

  while(1)
  {
    if(spiDmaTxIsDone(ch))
    {
      break;
    }
    if((millis()-t_time) > timeout)
    {
      ret = false;
      break;
    }
  }

  return ret;
}

bool spiDmaTxIsDone(uint8_t ch)
{
  spi_t  *p_spi = &spi_tbl[ch];

  if (p_spi->is_open == false)     return true;

  return p_spi->is_tx_done;
}

void spiAttachTxInterrupt(uint8_t ch, void (*func)())
{
  spi_t  *p_spi = &spi_tbl[ch];


  if (p_spi->is_open == false)     return;

  p_spi->func_tx = func;
}


void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi->Instance == spi_tbl[_DEF_SPI1].h_spi->Instance)
  {
    spi_tbl[_DEF_SPI1].is_rx_done = true;
  }  
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi->Instance == spi_tbl[_DEF_SPI1].h_spi->Instance)
  {
    spi_tbl[_DEF_SPI1].is_error = true;
  }
}

void SPI1_IRQHandler(void)
{
  HAL_SPI_IRQHandler(&hspi1);
}

void GPDMA1_Channel1_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&handle_GPDMA1_Channel1);
}



void HAL_SPI_MspInit(SPI_HandleTypeDef* spiHandle)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  DMA_NodeConfTypeDef NodeConfig= {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  if(spiHandle->Instance==SPI1)
  {
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_SPI1;
    PeriphClkInit.Spi1ClockSelection = RCC_SPI1CLKSOURCE_SYSCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
      Error_Handler();
    }

    /* SPI1 clock enable */
    __HAL_RCC_SPI1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**SPI1 GPIO Configuration
    PA1     ------> SPI1_SCK
    PA7     ------> SPI1_MOSI
    PB0     ------> SPI1_NSS
    */
    GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* SPI1 DMA Init */
    /* GPDMA1_REQUEST_SPI1_TX Init */
    NodeConfig.NodeType = DMA_GPDMA_LINEAR_NODE;
    NodeConfig.Init.Request = GPDMA1_REQUEST_SPI1_TX;
    NodeConfig.Init.BlkHWRequest = DMA_BREQ_SINGLE_BURST;
    NodeConfig.Init.Direction = DMA_MEMORY_TO_PERIPH;
    NodeConfig.Init.SrcInc = DMA_SINC_FIXED;
    NodeConfig.Init.DestInc = DMA_DINC_FIXED;
    NodeConfig.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_HALFWORD;
    NodeConfig.Init.DestDataWidth = DMA_DEST_DATAWIDTH_HALFWORD;
    NodeConfig.Init.SrcBurstLength = 1;
    NodeConfig.Init.DestBurstLength = 1;
    NodeConfig.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT1|DMA_DEST_ALLOCATED_PORT0;
    NodeConfig.Init.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
    NodeConfig.Init.Mode = DMA_NORMAL;
    NodeConfig.TriggerConfig.TriggerPolarity = DMA_TRIG_POLARITY_MASKED;
    NodeConfig.DataHandlingConfig.DataExchange = DMA_EXCHANGE_NONE;
    NodeConfig.DataHandlingConfig.DataAlignment = DMA_DATA_RIGHTALIGN_ZEROPADDED;
    if (HAL_DMAEx_List_BuildNode(&NodeConfig, &Node_GPDMA1_Channel1) != HAL_OK)
    {
      Error_Handler();
    }

    if (HAL_DMAEx_List_InsertNode(&List_GPDMA1_Channel1, NULL, &Node_GPDMA1_Channel1) != HAL_OK)
    {
      Error_Handler();
    }

    if (HAL_DMAEx_List_SetCircularMode(&List_GPDMA1_Channel1) != HAL_OK)
    {
      Error_Handler();
    }

    handle_GPDMA1_Channel1.Instance = GPDMA1_Channel1;
    handle_GPDMA1_Channel1.InitLinkedList.Priority = DMA_LOW_PRIORITY_LOW_WEIGHT;
    handle_GPDMA1_Channel1.InitLinkedList.LinkStepMode = DMA_LSM_FULL_EXECUTION;
    handle_GPDMA1_Channel1.InitLinkedList.LinkAllocatedPort = DMA_LINK_ALLOCATED_PORT1;
    handle_GPDMA1_Channel1.InitLinkedList.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
    handle_GPDMA1_Channel1.InitLinkedList.LinkedListMode = DMA_LINKEDLIST_CIRCULAR;
    if (HAL_DMAEx_List_Init(&handle_GPDMA1_Channel1) != HAL_OK)
    {
      Error_Handler();
    }

    if (HAL_DMAEx_List_LinkQ(&handle_GPDMA1_Channel1, &List_GPDMA1_Channel1) != HAL_OK)
    {
      Error_Handler();
    }

    handle_GPDMA1_Channel1.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_HALFWORD;
    handle_GPDMA1_Channel1.Init.DestDataWidth = DMA_DEST_DATAWIDTH_HALFWORD;

    __HAL_LINKDMA(spiHandle, hdmatx, handle_GPDMA1_Channel1);

    if (HAL_DMA_ConfigChannelAttributes(&handle_GPDMA1_Channel1, DMA_CHANNEL_NPRIV) != HAL_OK)
    {
      Error_Handler();
    }


    /* SPI1 interrupt Init */
    // HAL_NVIC_SetPriority(SPI1_IRQn, 5, 0);
    // HAL_NVIC_EnableIRQ(SPI1_IRQn);

    // HAL_NVIC_SetPriority(GPDMA1_Channel1_IRQn, 5, 0);
    // HAL_NVIC_EnableIRQ(GPDMA1_Channel1_IRQn);
  }
}

void HAL_SPI_MspDeInit(SPI_HandleTypeDef* spiHandle)
{

  if(spiHandle->Instance==SPI2)
  {
    /* Peripheral clock disable */    
    __HAL_RCC_SPI1_CLK_DISABLE();

    /**SPI1 GPIO Configuration
    PA1     ------> SPI1_SCK
    PA7     ------> SPI1_MOSI
    PB0     ------> SPI1_NSS
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_1|GPIO_PIN_7);

    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_0);

    /* SPI1 DMA DeInit */
    HAL_DMA_DeInit(spiHandle->hdmatx);

    /* SPI1 interrupt Deinit */
    HAL_NVIC_DisableIRQ(SPI1_IRQn); 
  }
}


#endif