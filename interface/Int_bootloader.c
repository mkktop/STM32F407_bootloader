#include "Int_bootloader.h"

uint8_t bootloader_uart_rec_buffer[BOOTLOADER_UART_BUF_SIZE] = {0}; // 接收缓冲区
uint16_t bootloader_uart_rec_len = 0;                               // 接收到的数据长度
uint16_t rec_total_len = 0;                                         // 接收到的数据总长度
uint32_t flash_write_offset = 0;                                    // 写flash偏移量
static uint8_t pending_byte = 0;                                    // 缓存的字节
static uint8_t is_pending_byte = 0;                                 // 是否有缓存字节
/**
 * @brief 串口接收中断回调函数
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART1) // 判断是否为USART1
    {
        bootloader_uart_rec_len = Size; // 更新接收到的数据长度
        // TODO: 处理接收到的数据
        // 解锁flash
        HAL_FLASH_Unlock();
        uint8_t is_erase = 0;
        for (uint16_t i = 0; i < bootloader_uart_rec_len; i++)
        {
            // 读取flash中要写入的位置是否为0xff，如果不是0xff则进行擦除
            uint8_t data = *(volatile uint8_t *)(APPLICATION_ADDRESS + flash_write_offset + i);
            if (data != 0xff)
            {
                is_erase = 1;
                break;
            }
        }
        // 擦除为耗时操作，后续打算先收到升级指令，擦除整个分区后再写入
        if (is_erase)
        {
            is_erase = 0;
            // 擦除flash
            FLASH_EraseInitTypeDef erase_init_struct = {0};        // 用于配置Flash擦除操作的数据结构
            erase_init_struct.TypeErase = FLASH_TYPEERASE_SECTORS; // 扇区擦除
            erase_init_struct.Banks = FLASH_BANK_1;
            // 计算起始和结束地址
            uint32_t start_address = APPLICATION_ADDRESS + flash_write_offset;
            uint32_t end_address = start_address + bootloader_uart_rec_len - 1;

            // 计算起始和结束扇区
            uint32_t start_sector = get_flash_sector(start_address);
            uint32_t end_sector = get_flash_sector(end_address);

            // 设置擦除参数
            erase_init_struct.Sector = start_sector;
            erase_init_struct.NbSectors = end_sector - start_sector + 1;
            uint32_t sector_error = 0;
            HAL_FLASHEx_Erase(&erase_init_struct, &sector_error);
        }

        // 判断将要写入的数据是否为偶数，处理上次遗漏的字节
        if ((bootloader_uart_rec_len + is_pending_byte) % 2 == 0)
        {
            // 本次能够正好完全写入
            if (is_pending_byte)
            {
                // 上次遗漏一个字节，这次需要先写入
                for (uint16_t i = 0; i < bootloader_uart_rec_len; i += 2)
                {
                    uint32_t write_flash_addr = APPLICATION_ADDRESS + flash_write_offset + i;
                    uint16_t data16;
                    if (i == 0)
                    {
                        data16 = pending_byte | (bootloader_uart_rec_buffer[i] << 8);
                    }
                    else
                    {
                        data16 = (bootloader_uart_rec_buffer[i - 1]) | (bootloader_uart_rec_buffer[i] << 8);
                    }
                    HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, write_flash_addr, data16);
                }
                flash_write_offset += bootloader_uart_rec_len + 1; // 因为上次有遗漏，本次要多偏移一次
                is_pending_byte = 0;
            }
            else
            {
                // 正好写入
                for (uint16_t i = 0; i < bootloader_uart_rec_len; i += 2)
                {
                    uint32_t write_flash_addr = APPLICATION_ADDRESS + flash_write_offset + i;
                    uint16_t data16;
                    data16 = (bootloader_uart_rec_buffer[i + 1] << 8) | bootloader_uart_rec_buffer[i];
                    HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, write_flash_addr, data16);
                }
                flash_write_offset += bootloader_uart_rec_len; // 更新flash偏移量
                is_pending_byte = 0;
            }
        }
        else
        {
            // 本次会出现遗漏的情况
            if (is_pending_byte)
            {
                // 上次遗漏一个字节，这次需要先写入
                for (uint16_t i = 0; i < bootloader_uart_rec_len; i += 2)
                {
                    uint32_t write_flash_addr = APPLICATION_ADDRESS + flash_write_offset + i;
                    uint16_t data16;
                    if (i == 0)
                    {
                        data16 = pending_byte | (bootloader_uart_rec_buffer[i] << 8);
                    }
                    else
                    {
                        data16 = (bootloader_uart_rec_buffer[i - 1]) | (bootloader_uart_rec_buffer[i] << 8);
                    }
                    HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, write_flash_addr, data16);
                }
                flash_write_offset += bootloader_uart_rec_len;
                pending_byte = bootloader_uart_rec_buffer[bootloader_uart_rec_len - 1];
                is_pending_byte = 1;
            }
            else
            {
                // 上次没有遗漏
                for (uint16_t i = 0; i < bootloader_uart_rec_len - 1; i += 2)
                {
                    uint32_t write_flash_addr = APPLICATION_ADDRESS + flash_write_offset + i;
                    uint16_t data16;
                    data16 = (bootloader_uart_rec_buffer[i + 1] << 8) | bootloader_uart_rec_buffer[i];
                    HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, write_flash_addr, data16);
                }
                flash_write_offset += bootloader_uart_rec_len - 1; // 更新flash偏移量
                pending_byte = bootloader_uart_rec_buffer[bootloader_uart_rec_len - 1];
                is_pending_byte = 1;
            }
        }
        // 锁住flash
        HAL_FLASH_Lock();

        rec_total_len += bootloader_uart_rec_len;                                                   // 更新接收到的数据总长度
        memset(bootloader_uart_rec_buffer, 0, BOOTLOADER_UART_BUF_SIZE);                            // 清空接收缓冲区
        HAL_UARTEx_ReceiveToIdle_IT(&huart1, bootloader_uart_rec_buffer, BOOTLOADER_UART_BUF_SIZE); // 使能接收中断
    }
}

/**
 * @brief bootloader初始化
 */
void Int_bootloader_init(void)
{
    // 为保持健壮性，在初始化前先清除标志位
    __HAL_UART_CLEAR_OREFLAG(&huart1);                                                          // 清除ORE标志(若在初始化之前产生ORE错误，可能会引发BUG，故需清除)
    __HAL_UART_CLEAR_IDLEFLAG(&huart1);                                                         // 清除IDLE标志
    HAL_UARTEx_ReceiveToIdle_IT(&huart1, bootloader_uart_rec_buffer, BOOTLOADER_UART_BUF_SIZE); // 使能接收中断
}

/**
 * @brief 根据Flash地址计算对应的扇区号
 * @param address Flash地址
 * @return 扇区号 (FLASH_SECTOR_0 到 FLASH_SECTOR_11)
 */
uint32_t get_flash_sector(uint32_t address)
{
    uint32_t sector = 0;

    if ((address >= 0x08000000) && (address < 0x08004000))
    {
        sector = FLASH_SECTOR_0;
    }
    else if ((address >= 0x08004000) && (address < 0x08008000))
    {
        sector = FLASH_SECTOR_1;
    }
    else if ((address >= 0x08008000) && (address < 0x0800C000))
    {
        sector = FLASH_SECTOR_2;
    }
    else if ((address >= 0x0800C000) && (address < 0x08010000))
    {
        sector = FLASH_SECTOR_3;
    }
    else if ((address >= 0x08010000) && (address < 0x08020000))
    {
        sector = FLASH_SECTOR_4;
    }
    else if ((address >= 0x08020000) && (address < 0x08040000))
    {
        sector = FLASH_SECTOR_5;
    }
    else if ((address >= 0x08040000) && (address < 0x08060000))
    {
        sector = FLASH_SECTOR_6;
    }
    else if ((address >= 0x08060000) && (address < 0x08080000))
    {
        sector = FLASH_SECTOR_7;
    }
    else if ((address >= 0x08080000) && (address < 0x080A0000))
    {
        sector = FLASH_SECTOR_8;
    }
    else if ((address >= 0x080A0000) && (address < 0x080C0000))
    {
        sector = FLASH_SECTOR_9;
    }
    else if ((address >= 0x080C0000) && (address < 0x080E0000))
    {
        sector = FLASH_SECTOR_10;
    }
    else if ((address >= 0x080E0000) && (address < 0x08100000))
    {
        sector = FLASH_SECTOR_11;
    }

    return sector;
}
