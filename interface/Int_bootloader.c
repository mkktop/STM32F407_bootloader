#include "Int_bootloader.h"

uint8_t bootloader_uart_rec_buffer[BOOTLOADER_UART_BUF_SIZE] = {0}; // 接收缓冲区
uint16_t bootloader_uart_rec_len = 0; // 接收到的数据长度
uint16_t rec_total_len = 0; // 接收到的数据总长度
/**
 * @brief 串口接收中断回调函数
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART1) // 判断是否为USART1
    {
        bootloader_uart_rec_len = Size; // 更新接收到的数据长度
        //TODO: 处理接收到的数据
        rec_total_len += bootloader_uart_rec_len; // 更新接收到的数据总长度
        memset(bootloader_uart_rec_buffer, 0, BOOTLOADER_UART_BUF_SIZE); // 清空接收缓冲区
        HAL_UARTEx_ReceiveToIdle_IT(&huart1, bootloader_uart_rec_buffer, BOOTLOADER_UART_BUF_SIZE); // 使能接收中断
    }
    
}

/**
 * @brief bootloader初始化
 */
void Int_bootloader_init(void)
{
    //为保持健壮性，在初始化前先清除标志位
    __HAL_UART_CLEAR_OREFLAG(&huart1); // 清除ORE标志(若在初始化之前产生ORE错误，可能会引发BUG，故需清除)
    __HAL_UART_CLEAR_IDLEFLAG(&huart1); // 清除IDLE标志
    HAL_UARTEx_ReceiveToIdle_IT(&huart1, bootloader_uart_rec_buffer, BOOTLOADER_UART_BUF_SIZE); // 使能接收中断
}
