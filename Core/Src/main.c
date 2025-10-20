/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "pocsag.h"
#include "si4463_driver.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define TXBUFLEN 256
#define SI4463_SDN_PORT    GPIOB
#define SI4463_SDN_PIN     GPIO_PIN_0
#define SI4463_NIRQ_PORT   GPIOB
#define SI4463_NIRQ_PIN    GPIO_PIN_1
#define SI4463_CS_PORT     GPIOB
#define SI4463_CS_PIN      GPIO_PIN_10
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
static char txBuff[TXBUFLEN];
Pocsag_t pocsag;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
void setupSI4463(void);
void processPOCSAGCommand(void);
void parseAndSendPOCSAG(char* command);
void parseAndSetFrequency(char* command);
void transmitPOCSAGWithSI4463(long address, int addresssource, int repeat, char* textmsg);
void uart_print(const char* message);
void uart_printf(const char* format, ...);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// Simple UART print function
void uart_print(const char* message) {
    HAL_UART_Transmit(&huart1, (uint8_t*)message, strlen(message), HAL_MAX_DELAY);
}

// Formatted UART print function
void uart_printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int len = vsnprintf(txBuff, TXBUFLEN, format, args);
    va_end(args);

    if (len > 0 && len < TXBUFLEN) {
        HAL_UART_Transmit(&huart1, (uint8_t*)txBuff, len, HAL_MAX_DELAY);
    }
}

// Initialize SI4463
void setupSI4463(void) {
    // Initialize SI4463
    Si4463_Init(&hspi1,
                SI4463_SDN_PORT, SI4463_SDN_PIN,
                SI4463_NIRQ_PORT, SI4463_NIRQ_PIN,
                SI4463_CS_PORT, SI4463_CS_PIN);

    // Set default POCSAG frequency
    Si4463_SetFrequency(433.920f);

    // Configure for POCSAG
    Si4463_ConfigureForPOCSAG();

    uart_print("SI4463 initialized for POCSAG at 433.92 MHz\r\n");
}

// Command processing
void processPOCSAGCommand(void) {
    static uint8_t rx_buffer[256];
    static uint16_t rx_index = 0;
    uint8_t byte;

    while (HAL_UART_Receive(&huart1, &byte, 1, 0) == HAL_OK) {
        if (byte == '\r' || byte == '\n') {
            if (rx_index > 0) {
                rx_buffer[rx_index] = '\0';

                // Echo the command
                uart_print("\r\n");

                // Process command
                if (rx_buffer[0] == 'P' || rx_buffer[0] == 'p') {
                    parseAndSendPOCSAG((char*)rx_buffer);
                } else if (rx_buffer[0] == 'F' || rx_buffer[0] == 'f') {
                    parseAndSetFrequency((char*)rx_buffer);
                } else {
                    uart_print("Unknown command. Use P or F.\r\n");
                }

                rx_index = 0;
            }
        } else if (rx_index < sizeof(rx_buffer) - 1) {
            // Echo character back
            HAL_UART_Transmit(&huart1, &byte, 1, HAL_MAX_DELAY);
            rx_buffer[rx_index++] = byte;
        }
    }
}

void parseAndSendPOCSAG(char* command) {
    long address = 0;
    int addresssource = 0;
    int repeat = 0;
    char textmsg[42] = {0};

    // Parse command: P <address> <source> <repeat> <message>
    if (sscanf(command, "%*c %ld %d %d %40[^\n]",
               &address, &addresssource, &repeat, textmsg) == 4) {

        uart_printf("address: %ld\r\n", address);
        uart_printf("addresssource: %d\r\n", addresssource);
        uart_printf("repeat: %d\r\n", repeat);
        uart_printf("message: %s\r\n", textmsg);

        transmitPOCSAGWithSI4463(address, addresssource, repeat, textmsg);
    } else {
        uart_print("Invalid P command format. Use: P <address> <source> <repeat> <message>\r\n");
        uart_print("Example: P 123456 0 1 \"Hello World\"\r\n");
    }
}

void transmitPOCSAGWithSI4463(long address, int addresssource, int repeat, char* textmsg) {
    int rc = Pocsag_CreatePocsag(&pocsag, address, addresssource, textmsg, 0, 1);

    if (!rc) {
        uart_printf("Error in createpocsag! Error: %d\r\n", Pocsag_GetError(&pocsag));
        HAL_Delay(10000);
    } else {
        uint8_t* pocsagData = (uint8_t*)Pocsag_GetMsgPointer(&pocsag);
        uint16_t pocsagSize = Pocsag_GetSize(&pocsag);

        uart_printf("POCSAG message created: %d bytes\r\n", pocsagSize);

        for (int l = -1; l < repeat; l++) {
            uart_printf("POCSAG SEND with SI4463 (transmission %d)\r\n", l + 2);

            // Transmit using SI4463
            Si4463_TransmitPOCSAG(pocsagData, pocsagSize);

            HAL_Delay(3000);
        }

        uart_print("Transmission complete\r\n");
    }
}

void parseAndSetFrequency(char* command) {
    int freq1 = 0, freq2 = 0;

    if (sscanf(command, "%*c %d %d", &freq1, &freq2) == 2) {
        float newfreq = (float)freq1 + ((float)freq2) / 10000.0F;

        // Validate frequency range
        if ((newfreq >= 135.0F && newfreq <= 175.0F) ||
            (newfreq >= 400.0F && newfreq <= 470.0F) ||
            (newfreq >= 850.0F && newfreq <= 930.0F)) {

            uart_printf("Switching to new frequency: %.4f MHz\r\n", newfreq);

            // Update SI4463 frequency
            Si4463_SetFrequency(newfreq);

            uart_print("Frequency set successfully\r\n");
        } else {
            uart_print("Error: invalid frequency range.\r\n");
            uart_print("Valid ranges: 135-175 MHz, 400-470 MHz, 850-930 MHz\r\n");
        }
    } else {
        uart_print("Invalid F command format. Use: F <freqmhz> <freq100Hz>\r\n");
        uart_print("Example: F 433 9200 for 433.9200 MHz\r\n");
    }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  // Initialize POCSAG
  Pocsag_Init(&pocsag);

  setupSI4463();

  uart_print("\r\nPOCSAG text-message tool v0.1 (STM32F103 + SI4463)\r\n");
  uart_print("https://github.com/on1arf/pocsag\r\n");
  uart_print("Format:\r\n");
  uart_print("P <address> <source> <repeat> <message>\r\n");
  uart_print("F <freqmhz> <freq100Hz>\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    processPOCSAGCommand();
    HAL_Delay(100);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 9600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_10, GPIO_PIN_RESET);

  /*Configure GPIO pins : PB0 PB10 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PB1 */
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  // Configure additional SI4463 pins
  // PB1 as input (NIRQ)
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  // PB10 as output (CS)
  GPIO_InitStruct.Pin = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  // Set initial CS state high
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET);
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
