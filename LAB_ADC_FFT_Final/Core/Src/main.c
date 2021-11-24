/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dcmi.h"
#include "dfsdm.h"
#include "i2c.h"
#include "usart.h"
#include "quadspi.h"
#include "sai.h"
#include "spi.h"
#include "tim.h"
#include "gpio.h"
#include "fmc.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdio.h"
#include "arm_math.h"
#include "arm_const_structs.h"
#include "stm32l496g_discovery.h"
#include "stm32l496g_discovery_lcd.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define BIAS        0.0437
#define FFT_LENGTH  4096
#define SAMPLE_FEQ  5000
#define TRANS_FEQ   10525000000.0
#define LIGHT_SPEED 300000000.0
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
int FFT_flag = 0;
int Startflag = 0;
float32_t speed = 0.0;
float32_t frequency = 0.0;
float32_t FFT_Input_Buffer[FFT_LENGTH * 2] = {};
float32_t FFT_Output_Buffer[FFT_LENGTH] = {};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if (htim->Instance == TIM3) HAL_ADC_Start_IT(&hadc1);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    static uint32_t AD_Value;
    static int sample_num = 0;

    HAL_ADC_Stop_IT(&hadc1);
    HAL_TIM_Base_Stop_IT(&htim3);

    AD_Value = HAL_ADC_GetValue(&hadc1);

    if (sample_num < FFT_LENGTH - 1)
    {
    	FFT_Input_Buffer[sample_num * 2] = (float32_t)AD_Value * 2.5 / 2048;
    	FFT_Input_Buffer[sample_num * 2 + 1] = 0;
    	sample_num ++;
    	HAL_TIM_Base_Start_IT(&htim3);
    }
    else
    {
    	sample_num = 0; HAL_TIM_Base_Stop(&htim3);
    	FFT_flag = 1;
    }

    //HAL_TIM_Base_Start_IT(&htim3);
}

void LCD_Print(int status, double frequency, double speed)
{
	static int last_status = 1;
	static char Display1[20] = "";
	static char Display2[20] = "";
	static char Display3[20] = "";
	if (frequency == -1)
	{
		sprintf(Display1, " Out of Range "); sprintf(Display2, " Out of Range ");
	}
	else
	{
		sprintf(Display1, "%9.2f Hz ", frequency); sprintf(Display2, "%9.3f m/s", speed);
	}
	BSP_LCD_DisplayStringAtLine(2,(uint8_t*)Display1);
	BSP_LCD_DisplayStringAtLine(6,(uint8_t*)Display2);
	if (last_status != status)
	{
		BSP_LCD_ClearStringLine(8);
		sprintf(Display3, "Status: %s", (status == 0) ? "OFF":"ON");
		BSP_LCD_SetTextColor((status == 0) ? LCD_COLOR_RED : LCD_COLOR_GREEN);
		BSP_LCD_DisplayStringAtLine(8,(uint8_t*)Display3);
		BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
	}
	last_status = status;
}

void LCD_Config(void)
{
    BSP_LCD_Init();

    BSP_LCD_SetBackColor(LCD_COLOR_WHITE);
    BSP_LCD_SetFont(&Font24);
    BSP_LCD_DisplayStringAtLine(0,(uint8_t*)"Frequency:");
	BSP_LCD_DisplayStringAtLine(4,(uint8_t*)"Speed:");
    LCD_Print(0,0,0);
}

void speed_calcluation(int n)
{
	frequency = (float)n / (float)FFT_LENGTH * SAMPLE_FEQ - n * BIAS;
	speed = frequency * LIGHT_SPEED / TRANS_FEQ /2;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if (GPIO_Pin == GPIO_PIN_10)
	{
		Startflag = (Startflag == 0) ? 1: 0;
		HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_13);
		HAL_TIM_Base_Start_IT(&htim3);
	}
	else if(GPIO_Pin == GPIO_PIN_8)
	{
		Startflag = 0; TIM3->CNT = 0; frequency = 0; speed = 0; FFT_flag = 0;
		HAL_UART_Transmit_IT(&huart2, (uint8_t*)"---Reset---\r\n", sizeof("---Reset---\r\n"));
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, SET);
		HAL_TIM_Base_Stop(&htim3);
	}

	if (Startflag == 0)
	{
		FFT_flag = 0;
		memset(FFT_Input_Buffer,0,sizeof(FFT_Input_Buffer));
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
  static char Txmsg[70] = "";
  static uint32_t FFT_index = 0;
  static float32_t value = 0.0;
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

/* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_DCMI_Init();
  MX_DFSDM1_Init();
  MX_FMC_Init();
  MX_I2C1_Init();
  MX_I2C2_Init();
  MX_LPUART1_UART_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_QUADSPI_Init();
  MX_SAI1_Init();
  MX_SPI1_Init();
  MX_SPI2_Init();
  MX_TIM3_Init();
  MX_TIM8_Init();
  /* USER CODE BEGIN 2 */
  LCD_Config();
  HAL_TIM_PWM_Start(&htim8,TIM_CHANNEL_2);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, SET);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	if (FFT_flag == 1 && Startflag == 1)
	{
	    arm_cfft_f32(&arm_cfft_sR_f32_len4096, FFT_Input_Buffer, 0, 1);
	    arm_cmplx_mag_f32(FFT_Input_Buffer, FFT_Output_Buffer, FFT_LENGTH);
	    FFT_Output_Buffer[0] = 0;
	    arm_max_f32(FFT_Output_Buffer, FFT_LENGTH / 2, &value, &FFT_index);
	    speed_calcluation(FFT_index);

	    sprintf(Txmsg, "num: %4lu frq: %7.2fHz  maxvalue: %5.0f speed: %6.3fm/s\r\n", FFT_index, frequency, value, speed);
	    HAL_UART_Transmit_IT(&huart2, (uint8_t*)Txmsg, sizeof(Txmsg));
	    while(huart2.gState != HAL_UART_STATE_READY);

	    FFT_index = 0; value = 0; FFT_flag = 0;
	    HAL_TIM_Base_Start_IT(&htim3);
	}
	if (Startflag == 1 && (frequency > 1200 || frequency < 70)) frequency = -1;
	LCD_Print(Startflag, frequency, speed);
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

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV4;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Peripherals Common Clock Configuration
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the peripherals clock
  */
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_SAI1|RCC_PERIPHCLK_ADC;
  PeriphClkInit.Sai1ClockSelection = RCC_SAI1CLKSOURCE_PLLSAI1;
  PeriphClkInit.AdcClockSelection = RCC_ADCCLKSOURCE_PLLSAI1;
  PeriphClkInit.PLLSAI1.PLLSAI1Source = RCC_PLLSOURCE_HSI;
  PeriphClkInit.PLLSAI1.PLLSAI1M = 1;
  PeriphClkInit.PLLSAI1.PLLSAI1N = 16;
  PeriphClkInit.PLLSAI1.PLLSAI1P = RCC_PLLP_DIV4;
  PeriphClkInit.PLLSAI1.PLLSAI1Q = RCC_PLLQ_DIV2;
  PeriphClkInit.PLLSAI1.PLLSAI1R = RCC_PLLR_DIV4;
  PeriphClkInit.PLLSAI1.PLLSAI1ClockOut = RCC_PLLSAI1_SAI1CLK|RCC_PLLSAI1_ADC1CLK;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
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

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/