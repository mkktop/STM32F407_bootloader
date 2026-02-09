#ifndef __INT_BOOTLOADER_H__
#define __INT_BOOTLOADER_H__
#include "usart.h"
#include "string.h"
#define BOOTLOADER_UART_BUF_SIZE 512 // 512字节的缓冲区

void Int_bootloader_init(void);

#endif /* __INT_BOOTLOADER_H__ */
