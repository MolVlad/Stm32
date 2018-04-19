#include "stm32f0xx_ll_bus.h"
#include "stm32f0xx_ll_gpio.h"
#include "stm32f0xx_ll_rcc.h"
#include "stm32f0xx_ll_system.h"
#include "stm32f0xx_ll_exti.h"
#include "stm32f0xx_ll_tim.h"
#include "stm32f0xx_ll_spi.h"

/*-----|PINS|-----
PB0			сервопривод

PC0			питание
PC1			открыто
PC2			закрыто
PC3			ожидается пароль

	освещение
PC4
PC5
PC6
PC7

	экран
PA1 			светодиод тревоги
PA2			светодиод тревоги
PA3 BL			подсветка экрана
PA4 DC			1 данные/0 команда
PA5 SCK_SPI		тактирование
PA6 RST			сброс
PA7 MOSI_SPI-DIN	данные с мк
*/

volatile unsigned int tim2_brightness;
volatile int real_time = 1736;
volatile int sys_tick = 0;
volatile int LCD_Enabled = 0;
char Password[] = "12**";

const unsigned int TIM2_PWM_FREQ = 100;
const unsigned int TIM2_CLOCK_FREQ = 10;
const int SAFE_NUM = 15;

enum FRONT
{
	RISING = 0,
	FALLING
};

enum STATE_SYSTEM
{
	SLEEP = 1,
	CLOSED,
	OPENED,
	WAITING,
	WRONG,
	FAULT,
	ASSERT_FIRE,
	ASSERT_BREAKING,
	BLOCKED
};

enum STATE_SYSTEM state_system = SLEEP;

void SystemClock_Config();
void SysTick_Handler();
void HardFault_Handler();
void SPI_Config();
void EnableClock();
void OutPut_Config();
void TIM3_Config();
void TIM2_Config();
void TIM2_IRQHandler();

void LED_AssertBlink_On();
void LED_AssertBlink_Off();
void LED_Power_On();
void LED_Power_Off();
void LED_Opened_On();
void LED_Opened_Off();
void LED_Lighting_On();
void LED_Lighting_Off();
void LED_Closed_On();
void LED_Closed_Off();
void LED_Waiting_On();
void LED_Waiting_Off();

void ChangeState(int state);
void State_Fire();
void State_Breaking();
void State_Blocked();
void State_Opened();
void State_Closed();
void State_Waiting();
void State_Sleep();
void State_Fault();
void State_Wrong();

void EnableLCD();
void LCD_Command();
void LCD_Data();
void LCD_Reset_On();
void LCD_Reset_Off();
void LCD_Backlight_On();
void LCD_Backlight_Off();
void LCD_Wait_Busy();
void LCD_Print(int x, int y, int n);
void LCD_SetCoord(int x, int y);
void LCD_Clear();
void LCD_Print_Time();
void LCD_Print_Lattice(int num_str);
void LCD_Print_Assert(int num_str);
void LCD_Print_Safe_Num();
void LCD_Print_Password(char * Password);

void Open_The_Lock();
void Close_The_Lock();

int main()
{
	SystemClock_Config();
	EnableClock();
	SPI_Config();
	OutPut_Config();
	EnableLCD();
	TIM2_Config();
	TIM3_Config();
	LED_Power_On();

	ChangeState(state_system);

	while(1);
}

void Close_The_Lock()
{
	LL_TIM_OC_SetCompareCH3(TIM3, 265);
}

void Open_The_Lock()
{
	LL_TIM_OC_SetCompareCH3(TIM3, 1200);
}

void LCD_Command()
{
	LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_4);
}

void LCD_Data()
{
	LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_4);
}

void LCD_Reset_On()
{
	LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_6);
}
void LCD_Reset_Off()
{
	LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_6);
}

void LCD_Backlight_On()
{
	LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_3);
}

void LCD_Backlight_Off()
{
	LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_3);
}

void LCD_Wait_Busy()
{
	while(LL_SPI_IsActiveFlag_BSY(SPI1));
}

void LED_Power_On()
{
	LL_GPIO_SetOutputPin(GPIOC, LL_GPIO_PIN_0);
}

void LED_Opened_On()
{
	LL_GPIO_SetOutputPin(GPIOC, LL_GPIO_PIN_1);
}

void LED_Closed_On()
{
	LL_GPIO_SetOutputPin(GPIOC, LL_GPIO_PIN_2);
}

void LED_Waiting_On()
{
	LL_GPIO_SetOutputPin(GPIOC, LL_GPIO_PIN_3);
}

void LED_Power_Off()
{
	LL_GPIO_ResetOutputPin(GPIOC, LL_GPIO_PIN_0);
}

void LED_Opened_Off()
{
	LL_GPIO_ResetOutputPin(GPIOC, LL_GPIO_PIN_1);
}

void LED_Closed_Off()
{
	LL_GPIO_ResetOutputPin(GPIOC, LL_GPIO_PIN_2);
}

void LED_Waiting_Off()
{
	LL_GPIO_ResetOutputPin(GPIOC, LL_GPIO_PIN_3);
}

void LED_Lighting_On()
{
	LL_GPIO_SetOutputPin(GPIOC, LL_GPIO_PIN_4);
	LL_GPIO_SetOutputPin(GPIOC, LL_GPIO_PIN_5);
	LL_GPIO_SetOutputPin(GPIOC, LL_GPIO_PIN_6);
	LL_GPIO_SetOutputPin(GPIOC, LL_GPIO_PIN_7);
}

void LED_Lighting_Off()
{
	LL_GPIO_ResetOutputPin(GPIOC, LL_GPIO_PIN_4);
	LL_GPIO_ResetOutputPin(GPIOC, LL_GPIO_PIN_5);
	LL_GPIO_ResetOutputPin(GPIOC, LL_GPIO_PIN_6);
	LL_GPIO_ResetOutputPin(GPIOC, LL_GPIO_PIN_7);
}

void LED_AssertBlink_On()
{
	tim2_brightness = 0;
	LL_TIM_EnableCounter(TIM2);
}

void LED_AssertBlink_Off()
{
		LL_TIM_OC_SetCompareCH2(TIM2, 0);
		LL_TIM_OC_SetCompareCH3(TIM2, 0);

		LL_TIM_DisableCounter(TIM2);
}

void ChangeState(int state)
{
	switch (state)
	{
		case SLEEP:
			State_Sleep();
			break;
		case CLOSED:
			State_Closed();
			break;
		case OPENED:
			State_Opened();
			break;
		case WRONG:
			State_Wrong();
			break;
		case FAULT:
			State_Fault();
			break;
		case ASSERT_FIRE:
			State_Fire();
			break;
		case ASSERT_BREAKING:
			State_Breaking();
			break;
		case BLOCKED:
			State_Blocked();
			break;
		case WAITING:
			State_Waiting();
			break;
}
}

void LCD_Print_Assert(int num_str)
{
	LCD_Print(0x0c * 0, 0x02 * num_str, 20);	//Т
	LCD_Print(0x0c * 1, 0x02 * num_str, 18);	//Р
	LCD_Print(0x0c * 2, 0x02 * num_str, 6);		//Е
	LCD_Print(0x0c * 3, 0x02 * num_str, 3);		//В
	LCD_Print(0x0c * 4, 0x02 * num_str, 16);	//О
	LCD_Print(0x0c * 5, 0x02 * num_str, 4);		//Г
	LCD_Print(0x0c * 6, 0x02 * num_str, 1);		//А
}

void LCD_Print_Lattice(int num_str)
{
	LCD_Print(0x0c * 0, 0x02 * num_str, 45);	//#
	LCD_Print(0x0c * 1, 0x02 * num_str, 45);	//#
	LCD_Print(0x0c * 2, 0x02 * num_str, 45);	//#
	LCD_Print(0x0c * 3, 0x02 * num_str, 45);	//#
	LCD_Print(0x0c * 4, 0x02 * num_str, 45);	//#
	LCD_Print(0x0c * 5, 0x02 * num_str, 45);	//#
	LCD_Print(0x0c * 6, 0x02 * num_str, 45);	//#
}

void State_Fire()
{
	LED_Opened_Off();
	LED_Closed_On();
	LED_Waiting_Off();
	LED_AssertBlink_On();
	LED_Lighting_Off();

	LCD_Backlight_On();

	LCD_Print_Assert(0);
	LCD_Print_Lattice(1);

	LCD_Print(0x0c * 0, 0x02 * 2, 0);	//отступ
	LCD_Print(0x0c * 1, 0x02 * 2, 17);	//П
	LCD_Print(0x0c * 2, 0x02 * 2, 16);	//О
	LCD_Print(0x0c * 3, 0x02 * 2, 8);	//Ж
	LCD_Print(0x0c * 4, 0x02 * 2, 1);	//А
	LCD_Print(0x0c * 5, 0x02 * 2, 18);	//Р
	LCD_Print(0x0c * 6, 0x02 * 2, 0);	//отступ

	LCD_Wait_Busy();
}

void State_Blocked()
{
	LED_Opened_Off();
	LED_Closed_On();
	LED_Waiting_Off();
	LED_AssertBlink_On();
	LED_Lighting_Off();

	LCD_Backlight_On();

	LCD_Print_Lattice(0);

	LCD_Print(0x0c * 0, 0x02 * 1, 3);	//B
	LCD_Print(0x0c * 1, 0x02 * 1, 47);	//L
	LCD_Print(0x0c * 2, 0x02 * 1, 16);	//O
	LCD_Print(0x0c * 3, 0x02 * 1, 19);	//C
	LCD_Print(0x0c * 4, 0x02 * 1, 12);	//K
	LCD_Print(0x0c * 5, 0x02 * 1, 6);	//E
	LCD_Print(0x0c * 6, 0x02 * 1, 48);	//D

	LCD_Print_Lattice(2);

	LCD_Wait_Busy();
}

void State_Breaking()
{
	LED_Opened_Off();
	LED_Closed_On();
	LED_Waiting_Off();
	LED_AssertBlink_On();
	LED_Lighting_Off();

	LCD_Backlight_On();

	LCD_Print_Assert(0);
	LCD_Print_Lattice(1);

	LCD_Print(0x0c * 0, 0x02 * 2, 0);
	LCD_Print(0x0c * 1, 0x02 * 2, 3);	//В
	LCD_Print(0x0c * 2, 0x02 * 2, 9);	//З
	LCD_Print(0x0c * 3, 0x02 * 2, 13);	//Л
	LCD_Print(0x0c * 4, 0x02 * 2, 16);	//О
	LCD_Print(0x0c * 5, 0x02 * 2, 14);	//М
	LCD_Print(0x0c * 6, 0x02 * 2, 0);

	LCD_Wait_Busy();
}

void LCD_Print_Time()
{
	LCD_Print(0x0c * 0, 0x02 * 0, 0);
	LCD_Print(0x0c * 1, 0x02 * 0, 34 + real_time / 1000);
	LCD_Print(0x0c * 2, 0x02 * 0, 34 + real_time / 100 % 10);
	LCD_Print(0x0c * 3, 0x02 * 0, 46);
	LCD_Print(0x0c * 4, 0x02 * 0, 34 + real_time / 10 % 10);
	LCD_Print(0x0c * 5, 0x02 * 0, 34 + real_time % 10);
	LCD_Print(0x0c * 6, 0x02 * 0, 0);
}

void LCD_Print_Safe_Num()
{
	int first_numeral = SAFE_NUM / 10;
	int second_numeral = SAFE_NUM % 10;

	LCD_Print(0x0c * 0, 0x02 * 1, 19);	//С
	LCD_Print(0x0c * 1, 0x02 * 1, 6);	//Е
	LCD_Print(0x0c * 2, 0x02 * 1, 11);	//Й
	LCD_Print(0x0c * 3, 0x02 * 1, 22);	//Ф

	LCD_Print(0x0c * 4, 0x02 * 1, 0);			//пропуск
	LCD_Print(0x0c * 5, 0x02 * 1, 34 + first_numeral);	//первая цифра номера
	LCD_Print(0x0c * 6, 0x02 * 1, 34 + second_numeral);	//вторая цифра номера
}

void State_Opened()
{
	Open_The_Lock();

	LED_Opened_On();
	LED_Closed_Off();
	LED_Waiting_Off();
	LED_AssertBlink_Off();
	LED_Lighting_On();

	LCD_Backlight_On();

	LCD_Print_Time();
	LCD_Print_Safe_Num();

	LCD_Print(0x0c * 0, 0x02 * 2, 16);	//О
	LCD_Print(0x0c * 1, 0x02 * 2, 20);	//Т
	LCD_Print(0x0c * 2, 0x02 * 2, 12);	//К
	LCD_Print(0x0c * 3, 0x02 * 2, 18);	//Р
	LCD_Print(0x0c * 4, 0x02 * 2, 29);	//Ы
	LCD_Print(0x0c * 5, 0x02 * 2, 20);	//Т
	LCD_Print(0x0c * 6, 0x02 * 2, 16);	//О

	LCD_Wait_Busy();
}

void State_Wrong()
{
	LED_Opened_Off();
	LED_Closed_On();
	LED_Waiting_Off();
	LED_AssertBlink_Off();
	LED_Lighting_Off();

	LCD_Backlight_On();

	LCD_Print_Lattice(0);

	LCD_Print(0x0c * 0, 0x02 * 1, 15);	//Н
	LCD_Print(0x0c * 1, 0x02 * 1, 6);	//Е
	LCD_Print(0x0c * 2, 0x02 * 1, 3);	//В
	LCD_Print(0x0c * 3, 0x02 * 1, 6);	//Е
	LCD_Print(0x0c * 4, 0x02 * 1, 18);	//Р
	LCD_Print(0x0c * 5, 0x02 * 1, 15);	//Н
	LCD_Print(0x0c * 6, 0x02 * 1, 16);	//О

	LCD_Print_Lattice(2);

	LCD_Wait_Busy();
}

void State_Closed()
{
	LED_Opened_Off();
	LED_Closed_On();
	LED_Waiting_Off();
	LED_AssertBlink_Off();
	LED_Lighting_Off();

	LCD_Backlight_On();

	LCD_Print_Time();

	LCD_Print_Safe_Num();

	LCD_Print(0x0c * 0, 0x02 * 2, 9);	//З
	LCD_Print(0x0c * 1, 0x02 * 2, 1);	//А
	LCD_Print(0x0c * 2, 0x02 * 2, 12);	//К
	LCD_Print(0x0c * 3, 0x02 * 2, 18);	//Р
	LCD_Print(0x0c * 4, 0x02 * 2, 29);	//Ы
	LCD_Print(0x0c * 5, 0x02 * 2, 20);	//Т
	LCD_Print(0x0c * 6, 0x02 * 2, 16);	//О

	LCD_Wait_Busy();
}

void State_Fault()
{
	LED_Opened_On();
	LED_Closed_Off();
	LED_Waiting_Off();
	LED_AssertBlink_Off();
	LED_Lighting_Off();

	LCD_Backlight_On();

	LCD_Print_Lattice(0);

	LCD_Print(0x0c * 0, 0x02 * 1, 0);	//отступы в начале строки
	LCD_Print(0x0c * 6, 0x02 * 1, 0);	//отступы в конце строки

	LCD_Print(0x0c * 0 + 0x05,  0x02 * 1, 16);	//О
	LCD_Print(0x0c * 1 + 0x05,  0x02 * 1, 26);	//Ш
	LCD_Print(0x0c * 2 + 0x05,  0x02 * 1, 10);	//И
	LCD_Print(0x0c * 3 + 0x05,  0x02 * 1, 2);	//Б
	LCD_Print(0x0c * 4 + 0x05,  0x02 * 1, 12);	//К
	LCD_Print(0x0c * 5 + 0x05,  0x02 * 1, 1);	//А

	LCD_Print_Lattice(2);

	LCD_Wait_Busy();
}

void State_Waiting()
{
	Close_The_Lock();

	LED_Opened_Off();
	LED_Closed_Off();
	LED_Waiting_On();
	LED_AssertBlink_Off();
	LED_Lighting_Off();

	LCD_Backlight_On();

	LCD_Print(0x0c * 0, 0x02 * 0, 3);	//В
	LCD_Print(0x0c * 1, 0x02 * 0, 3);	//В
	LCD_Print(0x0c * 2, 0x02 * 0, 6);	//Е
	LCD_Print(0x0c * 3, 0x02 * 0, 5);	//Д
	LCD_Print(0x0c * 4, 0x02 * 0, 10);	//И
	LCD_Print(0x0c * 5, 0x02 * 0, 20);	//Т
	LCD_Print(0x0c * 6, 0x02 * 0, 6);	//Е

	LCD_Print(0x0c * 0, 0x02 * 1, 0);	//отступ в начале строки
	LCD_Print(0x0c * 6, 0x02 * 1, 0);	//отступ в конце строки

	LCD_Print(0x0c * 0 + 0x05,  0x02 * 1, 17);	//П
	LCD_Print(0x0c * 1 + 0x05,  0x02 * 1, 1);	//А
	LCD_Print(0x0c * 2 + 0x05,  0x02 * 1, 18);	//Р
	LCD_Print(0x0c * 3 + 0x05,  0x02 * 1, 16);	//О
	LCD_Print(0x0c * 4 + 0x05,  0x02 * 1, 13);	//Л
	LCD_Print(0x0c * 5 + 0x05,  0x02 * 1, 30);	//Ь

	LCD_Print(0x0c * 0, 0x02 * 2, 0);
	LCD_Print(0x0c * 6, 0x02 * 2, 0);

	LCD_Print_Password(Password);

	LCD_Wait_Busy();
}

void LCD_Print_Password(char * Password)
{
	int i;

	LCD_Print(0x0c * 0 + 0x05,  0x02 * 2, 0);	//отступ

	for(i = 0; i < 4; i++)
		if(Password[i] == '*')
			LCD_Print(0x0c * (i + 1) + 0x05,  0x02 * 2, 44);	//*
		else
			LCD_Print(0x0c * (i + 1) + 0x05,  0x02 * 2, 34 + Password[i] - '0');	//соответсвующее число

	LCD_Print(0x0c * 5 + 0x05,  0x02 * 2, 0);	//отступ
}


void State_Sleep()
{
	LED_Opened_Off();
	LED_Closed_Off();
	LED_Waiting_Off();
	LED_AssertBlink_Off();
	LED_Lighting_Off();

	LCD_Clear();
	LCD_Wait_Busy();
	LCD_Backlight_Off();
}

void EnableClock()
{
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOC);

	LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_SYSCFG);
	LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM2);
	LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM3);
	LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_SPI1);
}

void OutPut_Config()
{
	LL_GPIO_SetPinMode(GPIOC, LL_GPIO_PIN_0, LL_GPIO_MODE_OUTPUT);	//питание
	LL_GPIO_SetPinMode(GPIOC, LL_GPIO_PIN_1, LL_GPIO_MODE_OUTPUT);	//открыт
	LL_GPIO_SetPinMode(GPIOC, LL_GPIO_PIN_2, LL_GPIO_MODE_OUTPUT);	//закрыт
	LL_GPIO_SetPinMode(GPIOC, LL_GPIO_PIN_3, LL_GPIO_MODE_OUTPUT);	//ожидает пароль

	//освещение сейфа
	LL_GPIO_SetPinMode(GPIOC, LL_GPIO_PIN_4, LL_GPIO_MODE_OUTPUT);
	LL_GPIO_SetPinMode(GPIOC, LL_GPIO_PIN_5, LL_GPIO_MODE_OUTPUT);
	LL_GPIO_SetPinMode(GPIOC, LL_GPIO_PIN_6, LL_GPIO_MODE_OUTPUT);
	LL_GPIO_SetPinMode(GPIOC, LL_GPIO_PIN_7, LL_GPIO_MODE_OUTPUT);

	LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_3, LL_GPIO_MODE_OUTPUT);	//подсветка экрана
	LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_4, LL_GPIO_MODE_OUTPUT);	//для экрана
	LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_6, LL_GPIO_MODE_OUTPUT);	//для экрана RESET

	LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_3, LL_GPIO_PULL_DOWN);
	LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_4, LL_GPIO_PULL_DOWN);
	LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_6, LL_GPIO_PULL_DOWN);

	LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_1, LL_GPIO_MODE_ALTERNATE);	//для светодиода тревоги
	LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_2, LL_GPIO_MODE_ALTERNATE);	//для•светодиода•тревоги

	LL_GPIO_SetAFPin_0_7(GPIOA, LL_GPIO_PIN_1, LL_GPIO_AF_2);
	LL_GPIO_SetAFPin_0_7(GPIOA, LL_GPIO_PIN_2, LL_GPIO_AF_2);

	LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_1, LL_GPIO_PULL_DOWN);
	LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_2, LL_GPIO_PULL_DOWN);

	LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_0, LL_GPIO_MODE_ALTERNATE);	//для сервопривода
	LL_GPIO_SetAFPin_0_7(GPIOB, LL_GPIO_PIN_0, LL_GPIO_AF_1);
	LL_GPIO_SetPinPull(GPIOB, LL_GPIO_PIN_0, LL_GPIO_PULL_DOWN);
}
void TIM3_Config()
{
	LL_TIM_InitTypeDef TIM_InitStruct;
	LL_TIM_OC_InitTypeDef TIM3_OC_CH3;

	TIM_InitStruct.Prescaler = __LL_TIM_CALC_PSC(SystemCoreClock, 1000000);
	TIM_InitStruct.CounterMode = LL_TIM_COUNTERMODE_UP;
	TIM_InitStruct.Autoreload = 10000;
	TIM_InitStruct.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
	TIM_InitStruct.RepetitionCounter = 0;
	LL_TIM_Init(TIM3, &TIM_InitStruct);

	LL_TIM_OC_StructInit(&TIM3_OC_CH3);
	TIM3_OC_CH3.OCMode = LL_TIM_OCMODE_PWM1;
	TIM3_OC_CH3.OCState = LL_TIM_OCSTATE_ENABLE;
	TIM3_OC_CH3.OCPolarity = LL_TIM_OCPOLARITY_HIGH;
	TIM3_OC_CH3.CompareValue = 0;
	LL_TIM_OC_Init(TIM3, LL_TIM_CHANNEL_CH3, &TIM3_OC_CH3);

	LL_TIM_EnableCounter(TIM3);
}

void TIM2_Config()
{
	LL_TIM_InitTypeDef TIM_InitStruct;
	LL_TIM_OC_InitTypeDef TIM2_OC_CH3;
	LL_TIM_OC_InitTypeDef TIM2_OC_CH2;

	TIM_InitStruct.Prescaler = __LL_TIM_CALC_PSC(SystemCoreClock, TIM2_CLOCK_FREQ);
	TIM_InitStruct.CounterMode = LL_TIM_COUNTERMODE_UP;
	TIM_InitStruct.Autoreload = __LL_TIM_CALC_ARR(SystemCoreClock, TIM_InitStruct.Prescaler, TIM2_PWM_FREQ);
	TIM_InitStruct.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
	TIM_InitStruct.RepetitionCounter = 0;
	LL_TIM_Init(TIM2, &TIM_InitStruct);

	LL_TIM_OC_StructInit(&TIM2_OC_CH3);
	TIM2_OC_CH3.OCMode = LL_TIM_OCMODE_PWM1;
	TIM2_OC_CH3.OCState = LL_TIM_OCSTATE_ENABLE;
	TIM2_OC_CH3.OCPolarity = LL_TIM_OCPOLARITY_HIGH;
	TIM2_OC_CH3.CompareValue = LL_TIM_GetAutoReload(TIM2);
	LL_TIM_OC_Init(TIM2, LL_TIM_CHANNEL_CH3, &TIM2_OC_CH3);

	LL_TIM_OC_StructInit(&TIM2_OC_CH2);
	TIM2_OC_CH2.OCMode = LL_TIM_OCMODE_PWM1;
	TIM2_OC_CH2.OCState = LL_TIM_OCSTATE_ENABLE;
	TIM2_OC_CH2.OCPolarity = LL_TIM_OCPOLARITY_HIGH;
	TIM2_OC_CH2.CompareValue = LL_TIM_GetAutoReload(TIM2);
	LL_TIM_OC_Init(TIM2, LL_TIM_CHANNEL_CH2, &TIM2_OC_CH2);

	NVIC_EnableIRQ(TIM2_IRQn);
	NVIC_SetPriority(TIM2_IRQn, 1);

	LL_TIM_EnableIT_UPDATE(TIM2);
}

void TIM2_IRQHandler()
{
	if (LL_TIM_IsActiveFlag_UPDATE(TIM2))
	{
		static enum FRONT tim2_front = RISING;

		switch (tim2_front)
		{
			case RISING:
				tim2_brightness++;
				if (tim2_brightness == TIM2_CLOCK_FREQ)
					tim2_front = FALLING;
				break;
			case FALLING:
				tim2_brightness--;
				if (tim2_brightness == 0)
					tim2_front = RISING;
				break;
			default:
				while(1);
		}

		LL_TIM_OC_SetCompareCH2(TIM2, (LL_TIM_GetAutoReload(TIM2) + 1)*tim2_brightness/TIM2_CLOCK_FREQ);
		LL_TIM_OC_SetCompareCH3(TIM2, (LL_TIM_GetAutoReload(TIM2) + 1)*tim2_brightness/TIM2_CLOCK_FREQ);

		LL_TIM_ClearFlag_UPDATE(TIM2);
	}
}

void SPI_Transmit(int data)
{
	while(!LL_SPI_IsActiveFlag_TXE(SPI1));
	LL_SPI_TransmitData8(SPI1, data);	//записывает в регистр DR данные
}

void EnableLCD()
{
	LCD_Command();
	LCD_Reset_Off();

	SPI_Transmit(0x21);
	SPI_Transmit(0x13);
	SPI_Transmit(0x04);
	SPI_Transmit(0xb8);
	SPI_Transmit(0x20);
	SPI_Transmit(0x09);
	SPI_Transmit(0x0c);

	LCD_Wait_Busy();
	LCD_Clear();
	LCD_Backlight_On();

	LCD_Enabled = 1;
}

void LCD_Clear()
{
	int i, j;

	for(i = 0; i < 7; i++)
		for(j = 0; j < 3; j++)
			LCD_Print(0x0c * i, 0x02 * j, 0);
}

void LCD_SetCoord(int x, int y)
{
	LCD_Wait_Busy();
	LCD_Command();
	SPI_Transmit(0x80 + x);
	SPI_Transmit(0x40 + y);
}

void SPI_Config()
{
	LL_SPI_InitTypeDef SPI;

	LL_GPIO_SetPinMode (GPIOA, LL_GPIO_PIN_5, LL_GPIO_MODE_ALTERNATE);
	LL_GPIO_SetPinMode (GPIOA, LL_GPIO_PIN_7, LL_GPIO_MODE_ALTERNATE);

	LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_5, LL_GPIO_PULL_DOWN);
	LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_7, LL_GPIO_PULL_DOWN);

	LL_GPIO_SetAFPin_0_7(GPIOA, LL_GPIO_PIN_5, LL_GPIO_AF_0);	//SCK
	LL_GPIO_SetAFPin_0_7(GPIOA, LL_GPIO_PIN_7, LL_GPIO_AF_0);	//MOSI

	SPI.TransferDirection = LL_SPI_FULL_DUPLEX;
	SPI.Mode = LL_SPI_MODE_MASTER;
	SPI.DataWidth = LL_SPI_DATAWIDTH_8BIT;
	SPI.ClockPolarity = LL_SPI_POLARITY_HIGH;	//какое напряжение тактирующего сигнала когда нет передачи
	SPI.ClockPhase = LL_SPI_PHASE_2EDGE;	//2EDGE - напряжение на пине данных 0, когда не передается
	SPI.NSS = LL_SPI_NSS_SOFT;
	SPI.BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV4;
	SPI.BitOrder = LL_SPI_MSB_FIRST;
	SPI.CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE;
	SPI.CRCPoly = 7U;

	LL_SPI_Init(SPI1, &SPI);

	LL_SPI_Enable(SPI1);
	while(!LL_SPI_IsEnabled(SPI1));
}

void SystemClock_Config()
{
	LL_FLASH_SetLatency(LL_FLASH_LATENCY_1);

	LL_RCC_HSI_Enable();
	while (LL_RCC_HSI_IsReady() != 1);

	LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI_DIV_2,
					LL_RCC_PLL_MUL_12);

	LL_RCC_PLL_Enable();
	while (LL_RCC_PLL_IsReady() != 1);

	LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
	LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
	while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL);

	LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_4);

	SysTick_Config(48000000/50000);
	SystemCoreClock = 48000000;
}

int a[] = {OPENED, WAITING, FAULT, OPENED, WAITING, CLOSED, WAITING, WRONG, CLOSED, ASSERT_FIRE, CLOSED, ASSERT_BREAKING, CLOSED, BLOCKED, SLEEP};
int i = 0;

void SysTick_Handler()
{
	sys_tick++;

	if(LCD_Enabled)
	{
		if(sys_tick % 100000 == 0)
		{
			ChangeState(a[i]);
			i++;
			if(i == 16)
				i = 0;
		}
	}

	if(sys_tick % 1000000 == 0)
		sys_tick = 0;

}
static const char LCD_Base[][24] =
{
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},	// 0 пустота
	{0x00, 0x00, 0x80, 0xe0, 0xf8, 0x9e, 0x9e, 0xf8, 0xe0, 0x80, 0x00, 0x00,
	0x00, 0x7e, 0x7f, 0x03, 0x01, 0x01, 0x01, 0x01, 0x03, 0x7f, 0x7e, 0x00},	// 1 А
	{0x00, 0xfe, 0xfe, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x06, 0x06, 0x00,
	0x00, 0x7f, 0x7f, 0x7f, 0x61, 0x61, 0x61, 0x73, 0x7f, 0x3f, 0x1f, 0x00},	// 2 Б
	{0x00, 0xfe, 0xfe, 0x86, 0x86, 0x86, 0x86, 0xce, 0xfe, 0x7c, 0x38, 0x00,
	0x00, 0x7f, 0x7f, 0x61, 0x61, 0x61, 0x61, 0x73, 0x7f, 0x3e, 0x1c, 0x00},	// 3 В
	{0x00, 0xfe, 0xfe, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1e, 0x1e, 0x00,
	0x00, 0x7f, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},	// 4 Г
	{0x00, 0x00, 0x80, 0xfe, 0xfe, 0x86, 0x86, 0xfe, 0xfe, 0x80, 0x00, 0x00,
	0x00, 0x7f, 0x7f, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x7f, 0x7f, 0x00},	// 5 Д
	{0x00, 0xfe, 0xfe, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x06, 0x00,
	0x00, 0x7f, 0x7f, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x60, 0x00},	// 6 Е
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},	// 7 Ё
	{0x00, 0x0e, 0x1c, 0x38, 0x70, 0xfe, 0xfe, 0x70, 0x38, 0x1c, 0x0e, 0x00,
	0x00, 0x70, 0x78, 0x1c, 0x0e, 0x7f, 0x7f, 0x0e, 0x1c, 0x78, 0x70, 0x00},	// 8 Ж
	{0x00, 0x06, 0x06, 0x86, 0x86, 0x86, 0x86, 0xce, 0xfe, 0x7c, 0x38, 0x00,
	0x00, 0x60, 0x60, 0x61, 0x61, 0x61, 0x61, 0x73, 0x7f, 0x3e, 0x1c, 0x00},	// 9 З
	{0x00, 0xfe, 0xfe, 0x00, 0x00, 0xc0, 0xf0, 0x3c, 0x0e, 0xfe, 0xfe, 0x00,
	0x00, 0x7f, 0x7f, 0x3c, 0x0f, 0x03, 0x00, 0x00, 0x00, 0x7f, 0x7f, 0x00},	// 10 И
	{0x00, 0xfe, 0xfe, 0x00, 0x00, 0xc6, 0xf6, 0x3c, 0x0e, 0xfe, 0xfe, 0x00,
	0x00, 0x7f, 0x7f, 0x3c, 0x0f, 0x03, 0x00, 0x00, 0x00, 0x7f, 0x7f, 0x00},	// 11 Й
	{0x00, 0xfe, 0xfe, 0xc0, 0xe0, 0x70, 0x38, 0x1c, 0x0e, 0x06, 0x06, 0x00,
	0x00, 0x7f, 0x7f, 0x01, 0x03, 0x07, 0x0e, 0x1c, 0x38, 0x70, 0x60, 0x00},	// 12 К
	{0x00, 0x00, 0x80, 0xe0, 0xf0, 0x38, 0x1c, 0x0e, 0x06, 0xfe, 0xfe, 0x00,
	0x00, 0x7e, 0x7f, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0x7f, 0x00},	// 13 Л
	{0x00, 0xfe, 0xfe, 0x1c, 0x38, 0xf0, 0xf0, 0x38, 0x1c, 0xfe, 0xfe, 0x00,
	0x00, 0x7f, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0x7f, 0x00},	// 14 М
	{0x00, 0xfe, 0xfe, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xfe, 0xfe, 0x00,
	0x00, 0x7f, 0x7f, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x7f, 0x7f, 0x00},	// 15 Н
	{0x00, 0xf8, 0xfc, 0x0e, 0x06, 0x06, 0x06, 0x06, 0x0e, 0xfc, 0xf8, 0x00,
	0x00, 0x1f, 0x3f, 0x70, 0x60, 0x60, 0x60, 0x60, 0x70, 0x3f, 0x1f, 0x00},	// 16 О
	{0x00, 0xfe, 0xfe, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0xfe, 0xfe, 0x00,
	0x00, 0x7f, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0x7f, 0x00},	// 17 П
	{0x00, 0xfe, 0xfe, 0xfe, 0x86, 0x86, 0x86, 0x86, 0xce, 0xfe, 0xfc, 0x00,
	0x00, 0x7f, 0x7f, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00},	// 18 Р
	{0x00, 0xf8, 0xfc, 0x0e, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x00,
	0x00, 0x01f, 0x3f, 0x70, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x00},	// 19 С
	{0x00, 0x06, 0x06, 0x06, 0x06, 0xfe, 0xfe, 0x06, 0x06, 0x06, 0x06, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00},	// 20 Т
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},	// 21 У
	{0x00, 0xf8, 0xfc, 0x0e, 0x06, 0xfe, 0xfe, 0x06, 0x0e, 0xfc, 0xf8, 0x00,
	0x00, 0x01, 0x03, 0x07, 0x06, 0x7f, 0x7f, 0x06, 0x07, 0x03, 0x01, 0x00},	// 22 Ф
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},	// 23 Х
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},	// 24 Ц
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},	// 25 Ч
	{0x00, 0xfe, 0xfe, 0x00, 0x00, 0xfe, 0xfe, 0x00, 0x00, 0xfe, 0xfe, 0x00,
	0x00, 0x7f, 0x7f, 0x60, 0x60, 0x7f, 0x7f, 0x60, 0x60, 0x7f, 0x7f, 0x00},	// 26 Ш
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},	// 27 Щ
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},	// 28 Ъ
	{0x00, 0xfe, 0xfe, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0xfe, 0xfe, 0x00,
	0x00, 0x7f, 0x7f, 0x73, 0x61, 0x73, 0x7f, 0x3f, 0x00, 0x7f, 0x7f, 0x00},	// 29 Ы
	{0x00, 0xfe, 0xfe, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00,
	0x00, 0x7f, 0x7f, 0x73, 0x61, 0x61, 0x61, 0x61, 0x73, 0x3f, 0x1e, 0x00},	// 30 Ь
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},	// 31 Э
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},	// 32 Ю
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},	// 33 Я
	{0x00, 0xfe, 0xfe, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0xfe, 0xfe, 0x00,
	0x00, 0x7f, 0x7f, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7f, 0x7f, 0x00},	// 34 0
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x38, 0x1c, 0x0e, 0xfe, 0xfe, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0x7f, 0x00},	// 35 1
	{0x00, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0xfe, 0xfe, 0x00,
	0x00, 0x7f, 0x7f, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x00},	// 36 2
	{0x00, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0xfe, 0xfe, 0x00,
	0x00, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x7f, 0x7f, 0x00},	// 37 3
	{0x00, 0xfe, 0xfe, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xfe, 0xfe, 0x00,
	0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x7f, 0x7f, 0x00},	// 38 4
	{0x00, 0xfe, 0xfe, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x00,
	0x00, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x7f, 0x7f, 0x00},	// 39 5
	{0x00, 0xfe, 0xfe, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x00,
	0x00, 0x7f, 0x7f, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x7f, 0x7f, 0x00},	// 40 6
	{0x00, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0xfe, 0xfe, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0x7f, 0x00},	// 41 7
	{0x00, 0xfe, 0xfe, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0xfe, 0xfe, 0x00,
	0x00, 0x7f, 0x7f, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x7f, 0x7f, 0x00},	// 42 8
	{0x00, 0xfe, 0xfe, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0xfe, 0xfe, 0x00,
	0x00, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x7f, 0x7f, 0x00},	// 43 9
	{0x00, 0x98, 0xb8, 0xf0, 0xe0, 0xfe, 0xfe, 0xe0, 0xf0, 0xb8, 0x98, 0x00,
	0x00, 0x19, 0x1d, 0x0f, 0x07, 0x7f, 0x7f, 0x07, 0x0f, 0x1d, 0x19, 0x00},	// 44 *
	{0x00, 0x60, 0x60, 0xfe, 0xfe, 0x60, 0x60, 0xfe, 0xfe, 0x60, 0x60, 0x00,
	0x00, 0x06, 0x06, 0x7f, 0x7f, 0x06, 0x06, 0x7f, 0x7f, 0x06, 0x06, 0x00},	// 45 #
	{0x00, 0x00, 0x00, 0x00, 0x30, 0x78, 0x78, 0x30, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x0c, 0x1e, 0x1e, 0x0c, 0x00, 0x00, 0x00, 0x00},	// 46 :
	{0x00, 0xfe, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x7f, 0x7f, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x00, 0x00, 0x00},	// 47 L
	{0x00, 0xfe, 0xfe, 0x06, 0x06, 0x06, 0x06, 0x0e, 0x1c, 0xf8, 0xf0, 0x00,
	0x00, 0x7f, 0x7f, 0x60, 0x60, 0x60, 0x60, 0x70, 0x38, 0x1f, 0x0f, 0x00},	// 48 D
};

void LCD_Print(int x, int y, int n)
{
	int i;

	LCD_SetCoord(x, y);

	LCD_Wait_Busy();
	LCD_Data();
	for(i = 0; i < 12; i++)
		SPI_Transmit(LCD_Base[n][i]);

	LCD_SetCoord(x, y + 1);

	LCD_Wait_Busy();
	LCD_Data();
	for(i = 12; i < 24; i++)
		SPI_Transmit(LCD_Base[n][i]);
}


