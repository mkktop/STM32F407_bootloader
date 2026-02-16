#ifndef __INT_BOOTLOADER_H__
#define __INT_BOOTLOADER_H__
#include "usart.h"
#include "string.h"
#define BOOTLOADER_UART_BUF_SIZE 512 // 512字节的缓冲区
#define APPLICATION_ADDRESS 0x08010000 // 应用程序A地址

void Int_bootloader_init(void);
uint32_t get_flash_sector(uint32_t address);
#endif /* __INT_BOOTLOADER_H__ */
