
# 生成一个完整、可直接编译运行的 STM32 标准库示例
# 针对 SU-03T1 语音模块，实现：开灯/关灯/调亮度/温度播报

code = r"""
//============================================================================
//  SU-03T1 语音模块控制示例 (STM32F103C8T6 标准库)
//  编译环境：Keil MDK + STM32F10x_StdPeriph_Lib
//  功能：接收语音指令，控制LED/风扇/亮度，并回传温度数据
//============================================================================

#include "stm32f10x.h"
#include <stdint.h>

//=========================== 协议定义 ===========================
#define FRAME_HEAD      0xFF    // 帧头
#define FRAME_TAIL      0xFE    // 帧尾
#define FRAME_LEN       4       // 固定帧长度

// 功能码 (必须与智能公元平台参数严格对应)
#define CMD_LIGHT_OFF   0x10    // 关灯
#define CMD_LIGHT_ON    0x11    // 开灯
#define CMD_FAN_OFF     0x20    // 关风扇
#define CMD_FAN_ON      0x21    // 开风扇
#define CMD_BRIGHT_DOWN 0x30    // 亮度减
#define CMD_BRIGHT_UP   0x31    // 亮度加
#define CMD_BRIGHT_SET  0x32    // 直接设置亮度
#define CMD_TEMP_ASK    0x40    // 语音模块请求温度
#define CMD_TEMP_REPLY  0x41    // 单片机回传温度

//=========================== 硬件引脚 ===========================
#define LED_GPIO        GPIOA
#define LED_PIN         GPIO_Pin_5      // PA5 = LED (板载LED)
#define FAN_GPIO        GPIOA
#define FAN_PIN         GPIO_Pin_6      // PA6 = 风扇控制
#define PWM_GPIO        GPIOA
#define PWM_PIN         GPIO_Pin_0      // PA0 = TIM2_CH1 (PWM调光)

//=========================== 全局变量 ===========================
volatile uint8_t  g_rx_frame[FRAME_LEN];    // 接收缓冲区
volatile uint8_t  g_rx_idx = 0;             // 接收索引
volatile uint8_t  g_frame_ready = 0;        // 帧就绪标志

uint8_t g_brightness = 128;                 // 当前亮度 0~255

//=========================== 函数声明 ===========================
void RCC_Config(void);
void GPIO_Config(void);
void USART2_Config(void);
void TIM2_PWM_Config(void);
void NVIC_Config(void);

void Process_Frame(uint8_t *frame);
void Light_On(void);
void Light_Off(void);
void Fan_On(void);
void Fan_Off(void);
void Brightness_Up(void);
void Brightness_Down(void);
void Brightness_Set(uint8_t level);
void Report_Temperature(void);
void USART2_SendByte(uint8_t data);
void USART2_SendFrame(uint8_t func, uint8_t data);
void Delay_ms(uint32_t ms);

//============================================================================
//  主函数
//============================================================================
int main(void)
{
    // 初始化
    RCC_Config();
    GPIO_Config();
    USART2_Config();
    TIM2_PWM_Config();
    NVIC_Config();
    
    // 启动PWM，初始亮度50%
    TIM_Cmd(TIM2, ENABLE);
    TIM_SetCompare1(TIM2, g_brightness);
    
    // 默认关闭灯和风扇
    Light_Off();
    Fan_Off();
    
    while (1)
    {
        if (g_frame_ready)
        {
            g_frame_ready = 0;
            Process_Frame((uint8_t *)g_rx_frame);
        }
    }
}

//============================================================================
//  帧解析与执行 (核心逻辑)
//============================================================================
void Process_Frame(uint8_t *frame)
{
    uint8_t cmd  = frame[1];    // 功能码
    uint8_t data = frame[2];    // 数据字段
    
    switch (cmd)
    {
        case CMD_LIGHT_OFF:
            Light_Off();
            break;
            
        case CMD_LIGHT_ON:
            Light_On();
            break;
            
        case CMD_FAN_OFF:
            Fan_Off();
            break;
            
        case CMD_FAN_ON:
            Fan_On();
            break;
            
        case CMD_BRIGHT_UP:
            Brightness_Up();
            break;
            
        case CMD_BRIGHT_DOWN:
            Brightness_Down();
            break;
            
        case CMD_BRIGHT_SET:
            Brightness_Set(data);   // data = 0~255
            break;
            
        case CMD_TEMP_ASK:
            Report_Temperature();   // 单片机主动回传温度
            break;
            
        default:
            // 未知指令，可发送错误码
            break;
    }
}

//============================================================================
//  灯光控制
//============================================================================
void Light_On(void)
{
    GPIO_SetBits(LED_GPIO, LED_PIN);    // PA5 = 1, LED亮
}

void Light_Off(void)
{
    GPIO_ResetBits(LED_GPIO, LED_PIN);  // PA5 = 0, LED灭
}

//============================================================================
//  风扇控制
//============================================================================
void Fan_On(void)
{
    GPIO_SetBits(FAN_GPIO, FAN_PIN);    // PA6 = 1, 风扇转
}

void Fan_Off(void)
{
    GPIO_ResetBits(FAN_GPIO, FAN_PIN);  // PA6 = 0, 风扇停
}

//============================================================================
//  亮度调节 (PWM)
//============================================================================
void Brightness_Up(void)
{
    if (g_brightness <= 235)
        g_brightness += 20;
    else
        g_brightness = 255;
    
    TIM_SetCompare1(TIM2, g_brightness);
}

void Brightness_Down(void)
{
    if (g_brightness >= 20)
        g_brightness -= 20;
    else
        g_brightness = 0;
    
    TIM_SetCompare1(TIM2, g_brightness);
}

void Brightness_Set(uint8_t level)
{
    g_brightness = level;
    TIM_SetCompare1(TIM2, g_brightness);
}

//============================================================================
//  温度播报 (单片机 → 语音模块)
//  假设当前温度 26.5℃，放大10倍传输 = 265
//============================================================================
void Report_Temperature(void)
{
    uint16_t temp = 265;    // 26.5 * 10
    
    uint8_t tx_buf[5];
    tx_buf[0] = FRAME_HEAD;
    tx_buf[1] = CMD_TEMP_REPLY;
    tx_buf[2] = (temp >> 8) & 0xFF;   // 高字节: 0x01
    tx_buf[3] = temp & 0xFF;          // 低字节: 0x09
    tx_buf[4] = FRAME_TAIL;
    
    for (int i = 0; i < 5; i++)
    {
        USART2_SendByte(tx_buf[i]);
    }
    
    // 语音模块收到 FF 41 01 09 FE 后，配置播报"当前温度26.5度"
}

//============================================================================
//  串口发送函数
//============================================================================
void USART2_SendByte(uint8_t data)
{
    while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
    USART_SendData(USART2, data);
}

void USART2_SendFrame(uint8_t func, uint8_t data)
{
    USART2_SendByte(FRAME_HEAD);
    USART2_SendByte(func);
    USART2_SendByte(data);
    USART2_SendByte(FRAME_TAIL);
}

//============================================================================
//  串口2中断服务函数 (核心：状态机解析)
//  向量号：38
//============================================================================
void USART2_IRQHandler(void)
{
    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    {
        uint8_t rx_byte = (uint8_t)(USART_ReceiveData(USART2) & 0xFF);
        
        //========== 状态机解析 ==========
        if (g_rx_idx == 0)
        {
            // 状态0：等待帧头 0xFF
            if (rx_byte == FRAME_HEAD)
            {
                g_rx_frame[0] = rx_byte;
                g_rx_idx = 1;
            }
            // 不是帧头就丢弃，继续等
        }
        else if (g_rx_idx == 1)
        {
            // 状态1：接收功能码
            g_rx_frame[1] = rx_byte;
            g_rx_idx = 2;
        }
        else if (g_rx_idx == 2)
        {
            // 状态2：接收数据
            g_rx_frame[2] = rx_byte;
            g_rx_idx = 3;
        }
        else if (g_rx_idx == 3)
        {
            // 状态3：等待帧尾 0xFE
            if (rx_byte == FRAME_TAIL)
            {
                g_rx_frame[3] = rx_byte;
                g_frame_ready = 1;      // 一帧完整接收，标记就绪
            }
            // 无论帧尾是否正确，重置状态机
            g_rx_idx = 0;
        }
        
        USART_ClearITPendingBit(USART2, USART_IT_RXNE);
    }
}

//============================================================================
//  RCC 时钟配置
//============================================================================
void RCC_Config(void)
{
    // 开启各模块时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);     // GPIOA
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);    // USART2
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);      // TIM2
}

//============================================================================
//  GPIO 配置
//============================================================================
void GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    
    // PA5 (LED) - 推挽输出
    GPIO_InitStruct.GPIO_Pin = LED_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LED_GPIO, &GPIO_InitStruct);
    
    // PA6 (风扇) - 推挽输出
    GPIO_InitStruct.GPIO_Pin = FAN_PIN;
    GPIO_Init(FAN_GPIO, &GPIO_InitStruct);
    
    // PA0 (TIM2_CH1 PWM) - 复用推挽输出
    GPIO_InitStruct.GPIO_Pin = PWM_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(PWM_GPIO, &GPIO_InitStruct);
    
    // PA2 (USART2_TX) - 复用推挽输出
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    // PA3 (USART2_RX) - 浮空输入
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStruct);
}

//============================================================================
//  USART2 配置 (9600bps, 8N1)
//  时钟：APB1=36MHz
//  BRR = 36000000 / 9600 = 3750 = 0x0EA6
//============================================================================
void USART2_Config(void)
{
    USART_InitTypeDef USART_InitStruct;
    
    USART_InitStruct.USART_BaudRate = 9600;
    USART_InitStruct.USART_WordLength = USART_WordLength_8b;
    USART_InitStruct.USART_StopBits = USART_StopBits_1;
    USART_InitStruct.USART_Parity = USART_Parity_No;
    USART_InitStruct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(USART2, &USART_InitStruct);
    
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);  // 开启接收中断
    USART_Cmd(USART2, ENABLE);                       // 使能串口
}

//============================================================================
//  TIM2 PWM 配置 (PA0=CH1, 1kHz频率)
//  72MHz / 72 / 1000 = 1kHz
//============================================================================
void TIM2_PWM_Config(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStruct;
    TIM_OCInitTypeDef TIM_OCInitStruct;
    
    // 时基配置
    TIM_TimeBaseStruct.TIM_Prescaler = 72 - 1;      // 分频到1MHz
    TIM_TimeBaseStruct.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStruct.TIM_Period = 1000 - 1;       // 1kHz
    TIM_TimeBaseStruct.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStruct);
    
    // PWM模式配置
    TIM_OCInitStruct.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStruct.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStruct.TIM_Pulse = 500;               // 初始占空比50%
    TIM_OCInitStruct.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC1Init(TIM2, &TIM_OCInitStruct);
    
    TIM_OC1PreloadConfig(TIM2, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM2, ENABLE);
}

//============================================================================
//  NVIC 中断配置
//============================================================================
void NVIC_Config(void)
{
    NVIC_InitTypeDef NVIC_InitStruct;
    
    NVIC_InitStruct.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);
}

//============================================================================
//  延时函数 (粗略实现)
//============================================================================
void Delay_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms * 8000; i++)
    {
        __asm volatile ("nop");
    }
}
"""

with open('/mnt/agents/output/su03t1_demo_main.c', 'w', encoding='utf-8') as f:
    f.write(code)

print("完整示例代码已生成")
print(f"文件大小: {len(code)} 字节")
