#include "stm32f1xx_hal.h"

#define FRAME_HEAD 0xFF;
#define FRAME_TAIL 0xFE;

volatile uint8_t rx_buf[4];
volatile uint8_t rx_idx = 0;
volatile uint8_t frame_ok = 0;

UART_HandleTypeDef huart1; // HAL库需要定义句柄

//================================初始化================================
void SU03T1_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // PA9 TX
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA,&GPIO_InitStruct);

    // PA10 RX
    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA,&GPIO_InitStruct);

    // USART1 配置
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 9600;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    HAL_UART_Init(&huart1);

    // NVIC
    HAL_NVIC_SetPriority(USART1_IRQn,1,0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);

    // 启动接收中断(每次收1字节)
    HAL_UART_Receive_IT(&huart1,&rx_buf[rx_idx],1);
}

//================================中断入口================================
void USART1_IRQHandler(void)
   {
    HAL_UART_IRQHandler(&huart1);  // HAL统一处理
   }

//================================主循环================================
int main(void){
    HAL_Init();
    SystemClock_Config(); // 时钟配置函数，需用户实现
    SU03T1_Init();

    while(1){
        if(frame_ok) {
            frame_ok = 0; // 处理完后重置标志
            uint8_t cmd = rx_buf[1]; // 取命令字

            switch(cmd) {
                case 0x10: Action_getdown(); break;
                case 0x11: Action_upright(); break;
                case 0x12: Action_advance(); break;
                case 0x13: Action_back(); break;
                case 0x14: Action_Lrotation(); break;
                case 0x15: Action_Rrotation(); break;
            }
        }
    }
}