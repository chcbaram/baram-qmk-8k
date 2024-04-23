#include "pssi.h"
#include "cli.h"
#include "cli_gui.h"
#include "spi.h"

static void cliCmd(cli_args_t *args);
static bool pssiInitHw(void);


static uint8_t spi_ch = _DEF_SPI1;

static PSSI_HandleTypeDef hpssi;
static DMA_NodeTypeDef Node_GPDMA1_Channel2;
static DMA_QListTypeDef List_GPDMA1_Channel2;
static DMA_HandleTypeDef handle_GPDMA1_Channel2;

static uint8_t pssi_buf[1024];
static uint32_t pssi_dma_pre_time = 0;
static uint32_t pssi_dma_exe_time = 0;





bool pssiInit(void)
{
  bool ret = true;

  
  ret &= spiBegin(spi_ch);


  if (ret)
  {
    uint16_t spi_data = 0xFFFF;
    spiDmaTxStart(spi_ch, (uint8_t *)&spi_data, 1);
  }
  delay(2);

  pssiInitHw();


  if(HAL_PSSI_Receive_DMA(&hpssi, (uint32_t*)pssi_buf , 16)!= HAL_OK)
  {
    Error_Handler();
  }

  logPrintf("[%s] pssiInit()\n", ret ? "OK":"NG");

  cliAdd("pssi", cliCmd);

  return true;
}

bool pssiInitHw(void)
{
  hpssi.Instance                = PSSI;
  hpssi.Init.DataWidth          = HAL_PSSI_8BITS;
  hpssi.Init.BusWidth           = HAL_PSSI_8LINES;
  hpssi.Init.ControlSignal      = HAL_PSSI_DE_ENABLE;
  hpssi.Init.ClockPolarity      = HAL_PSSI_FALLING_EDGE;
  hpssi.Init.DataEnablePolarity = HAL_PSSI_DEPOL_ACTIVE_HIGH;
  hpssi.Init.ReadyPolarity      = HAL_PSSI_RDYPOL_ACTIVE_LOW;
  if (HAL_PSSI_Init(&hpssi) != HAL_OK)
  {
    Error_Handler();
  }

  return true;
}

void GPDMA1_Channel2_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&handle_GPDMA1_Channel2);

  pssi_dma_exe_time = micros() - pssi_dma_pre_time;
}

void DCMI_PSSI_IRQHandler(void)
{
  HAL_PSSI_IRQHandler(&hpssi);
}

void HAL_PSSI_MspInit(PSSI_HandleTypeDef* pssiHandle)
{
  DMA_NodeConfTypeDef NodeConfig= {0};
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(pssiHandle->Instance==PSSI)
  {
  /* USER CODE BEGIN PSSI_MspInit 0 */

  /* USER CODE END PSSI_MspInit 0 */
    /* PSSI clock enable */
    __HAL_RCC_DCMI_PSSI_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**PSSI GPIO Configuration
    PA4     ------> PSSI_DE
    PA6     ------> PSSI_PDCK
    PC6     ------> PSSI_D0
    PC7     ------> PSSI_D1
    PC8     ------> PSSI_D2
    PC9     ------> PSSI_D3
    PC11     ------> PSSI_D4
    PB6     ------> PSSI_D5
    PB7     ------> PSSI_RDY
    PB8     ------> PSSI_D6
    PB9     ------> PSSI_D7
    */
    GPIO_InitStruct.Pin = GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF10_PSSI;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF4_PSSI;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF10_PSSI;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF4_PSSI;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF10_PSSI;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);



    NodeConfig.NodeType = DMA_GPDMA_LINEAR_NODE;
    NodeConfig.Init.Request = GPDMA1_REQUEST_DCMI_PSSI;
    NodeConfig.Init.BlkHWRequest = DMA_BREQ_SINGLE_BURST;
    NodeConfig.Init.Direction = DMA_PERIPH_TO_MEMORY;
    NodeConfig.Init.SrcInc = DMA_SINC_FIXED;
    NodeConfig.Init.DestInc = DMA_DINC_INCREMENTED;
    NodeConfig.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_BYTE;
    NodeConfig.Init.DestDataWidth = DMA_DEST_DATAWIDTH_BYTE;
    NodeConfig.Init.SrcBurstLength = 1;
    NodeConfig.Init.DestBurstLength = 1;
    NodeConfig.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0|DMA_DEST_ALLOCATED_PORT0;
    NodeConfig.Init.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
    NodeConfig.Init.Mode = DMA_NORMAL;
    NodeConfig.TriggerConfig.TriggerPolarity = DMA_TRIG_POLARITY_MASKED;
    NodeConfig.DataHandlingConfig.DataExchange = DMA_EXCHANGE_NONE;
    NodeConfig.DataHandlingConfig.DataAlignment = DMA_DATA_RIGHTALIGN_ZEROPADDED;
    if (HAL_DMAEx_List_BuildNode(&NodeConfig, &Node_GPDMA1_Channel2) != HAL_OK)
    {
      Error_Handler();
    }

    if (HAL_DMAEx_List_InsertNode(&List_GPDMA1_Channel2, NULL, &Node_GPDMA1_Channel2) != HAL_OK)
    {
      Error_Handler();
    }

    if (HAL_DMAEx_List_SetCircularMode(&List_GPDMA1_Channel2) != HAL_OK)
    {
      Error_Handler();
    }

    handle_GPDMA1_Channel2.Instance = GPDMA1_Channel2;
    handle_GPDMA1_Channel2.InitLinkedList.Priority = DMA_LOW_PRIORITY_LOW_WEIGHT;
    handle_GPDMA1_Channel2.InitLinkedList.LinkStepMode = DMA_LSM_FULL_EXECUTION;
    handle_GPDMA1_Channel2.InitLinkedList.LinkAllocatedPort = DMA_LINK_ALLOCATED_PORT0;
    handle_GPDMA1_Channel2.InitLinkedList.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
    handle_GPDMA1_Channel2.InitLinkedList.LinkedListMode = DMA_LINKEDLIST_CIRCULAR;
    if (HAL_DMAEx_List_Init(&handle_GPDMA1_Channel2) != HAL_OK)
    {
      Error_Handler();
    }

    if (HAL_DMAEx_List_LinkQ(&handle_GPDMA1_Channel2, &List_GPDMA1_Channel2) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(pssiHandle, hdmarx, handle_GPDMA1_Channel2);

    if (HAL_DMA_ConfigChannelAttributes(&handle_GPDMA1_Channel2, DMA_CHANNEL_NPRIV) != HAL_OK)
    {
      Error_Handler();
    }

    HAL_NVIC_SetPriority(GPDMA1_Channel2_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(GPDMA1_Channel2_IRQn);    

    // HAL_NVIC_SetPriority(DCMI_PSSI_IRQn, 5, 0);
    // HAL_NVIC_EnableIRQ(DCMI_PSSI_IRQn);    
  }
}

void HAL_PSSI_MspDeInit(PSSI_HandleTypeDef* pssiHandle)
{

  if(pssiHandle->Instance==PSSI)
  {
    /* Peripheral clock disable */
    __HAL_RCC_DCMI_PSSI_CLK_DISABLE();

    /**PSSI GPIO Configuration
    PA4     ------> PSSI_DE
    PA6     ------> PSSI_PDCK
    PC6     ------> PSSI_D0
    PC7     ------> PSSI_D1
    PC8     ------> PSSI_D2
    PC9     ------> PSSI_D3
    PC11     ------> PSSI_D4
    PB6     ------> PSSI_D5
    PB7     ------> PSSI_RDY
    PB8     ------> PSSI_D6
    PB9     ------> PSSI_D7
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_4|GPIO_PIN_6);

    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9
                          |GPIO_PIN_11);

    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9);
  }
}

void cliCmd(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 1 && args->isStr(0, "info"))
  {
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "spi"))
  {
    uint16_t spi_data = (uint16_t)(~(1<<15));

    cliShowCursor(false);


    while(cliKeepLoop())
    {
      pssi_dma_pre_time = micros();
      spiDmaTxStart(spi_ch, (uint8_t *)&spi_data, 1);      
      delay(10);

      cliPrintf("SCAN TIME : %d us\n", pssi_dma_exe_time);      

      cliPrintf("     ");
      for (int cols=0; cols<12; cols++)
      {
        cliPrintf("%02d ", cols);
      }
      cliPrintf("\n");

      for (int rows=0; rows<4; rows++)
      {
        cliPrintf("%02d : ", rows);

        for (int cols=0; cols<12; cols++)
        {
          uint8_t bit_data;
          
          bit_data = pssi_buf[cols];
          if (bit_data & (1<<rows))
            cliPrintf("_  ");
          else
            cliPrintf("O  ");
          bit_data <<= 1;
        }
        cliPrintf("\n");
      }
      cliMoveUp(6);
    }
    cliMoveDown(6);

    cliShowCursor(true);

    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("psii info\n");
    cliPrintf("psii spi\n");
  }
}





