#include "app.h"
#include "Int_bootloader.h"
#include "usart.h"
#define KEY1_PRESS_TIME_MS  1000
#define OTA_ACK_BUF_SIZE    64
uint8_t ota_ack_buf[OTA_ACK_BUF_SIZE] = {0};
#define OTA_ACK_TIMEOUT_MS  10000

#define FRAME_HEADER_1          0x5D
#define FRAME_HEADER_2          0x6C
#define FRAME_TAIL_1            0x7E
#define FRAME_TAIL_2            0x5A


uint16_t ota_ack_len = 0;
uint32_t ota_package_size = 0;  // OTA 升级包大小
app_ota_state_t ota_state = APP_OTA_STATE_IDLE;
extern uint32_t last_rec_time;
extern uint32_t rec_total_len;
/// @brief 解析 OTA 升级确认帧
/// @return 1-解析成功, 0-解析失败
static uint8_t app_ota_parse_frame(void)
{
    // 检查最小帧长度（帧头2 + 长度1 + 数据至少1 + 帧尾2 = 6）
    if (ota_ack_len < 6)
    {
        printf("Frame too short: %d\n", ota_ack_len);
        return 0;
    }
    
    // 检查帧头
    if (ota_ack_buf[0] != FRAME_HEADER_1 || ota_ack_buf[1] != FRAME_HEADER_2)
    {
        printf("Invalid header: 0x%02X 0x%02X\n", ota_ack_buf[0], ota_ack_buf[1]);
        return 0;
    }
    
    // 获取数据长度
    uint8_t data_len = ota_ack_buf[2];
    
    // 检查帧长度是否匹配
    // 帧头(2) + 长度(1) + 数据(N) + 帧尾(2) = 5 + N
    if (ota_ack_len != (5 + data_len))
    {
        printf("Frame length mismatch: expected %d, got %d\n", 5 + data_len, ota_ack_len);
        return 0;
    }
    
    // 检查帧尾
    uint8_t tail_pos = 3 + data_len;
    if (ota_ack_buf[tail_pos] != FRAME_TAIL_1 || ota_ack_buf[tail_pos + 1] != FRAME_TAIL_2)
    {
        printf("Invalid tail: 0x%02X 0x%02X\n", ota_ack_buf[tail_pos], ota_ack_buf[tail_pos + 1]);
        return 0;
    }
    
    // 解析数据内容（OTA 升级包大小）

    ota_package_size = ((uint32_t)ota_ack_buf[3] << 8) | ota_ack_buf[4];
    
    printf("OTA package size: %lu bytes\n", ota_package_size);
    
    return 1;
}

/// @brief 初始化OTA应用
/// @param  
void app_ota_init(void)
{
    uint32_t press_start_time = 0;

    // 检测按键是否按下
    if (HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_SET)
    {
        press_start_time = HAL_GetTick();

        // 等待按键释放或超时
        while (HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_SET)
        {
            // 按键按下超过 1 秒
            if ((HAL_GetTick() - press_start_time) >= KEY1_PRESS_TIME_MS)
            {
                // 按键按下超过 1 秒，触发 OTA 升级程序
                //printf("KEY1 pressed, OTA flag set, system reset\n");
                ota_state = APP_OTA_STATE_WAIT_ACK;// 等待升级包的发送端的确认
                return;
            }
        }
    }
    ota_state = APP_OTA_STATE_JUMP;
}

void app_ota_wait_ack(void)
{
    HAL_StatusTypeDef status;
    //printf("app_ota_wait_ack\n");
    status = HAL_UARTEx_ReceiveToIdle(&huart1, ota_ack_buf, OTA_ACK_BUF_SIZE, &ota_ack_len, OTA_ACK_TIMEOUT_MS);
    
    if (status == HAL_OK && ota_ack_len > 0)
    {
        // 解析帧
        if (app_ota_parse_frame())
        {
            //printf("OTA ack received, package size: %lu\n", ota_package_size);
            ota_state = APP_OTA_STATE_RUNNING;
        }
        else
        {
            //printf("OTA ack parse failed\n");
            ota_state = APP_OTA_STATE_IDLE;
        }
    }
    else if (status == HAL_TIMEOUT)
    {
        printf("OTA ack timeout\n");
        ota_state = APP_OTA_STATE_IDLE;  
    }
    else
    {
        printf("OTA ack receive error: %d\n", status);
        ota_state = APP_OTA_STATE_IDLE;
    }
    
    // 清空缓冲区
    memset(ota_ack_buf, 0, OTA_ACK_BUF_SIZE);
    ota_ack_len = 0;
}

void app_ota_running(void)
{
    // 擦除应用程序A的Flash
    Int_erase_app1_flash();
    //发送开始升级指令
    uint8_t start_frame[7] = {FRAME_HEADER_1, FRAME_HEADER_2, 0xAA, 0xBB,0xCC,FRAME_TAIL_1, FRAME_TAIL_2};
    HAL_UART_Transmit(&huart1, start_frame, sizeof(start_frame), HAL_MAX_DELAY);
    Int_bootloader_init();
    last_rec_time = HAL_GetTick();
    while (HAL_GetTick() - last_rec_time < 5000)
    {
    }
    //printf("app_ota_recv_end\n");
    ota_state = APP_OTA_STATE_CHECK;
}

void app_ota_check(void)
{
    if (rec_total_len != ota_package_size){
        printf("OTA package size mismatch: expected %lu, got %lu\n", ota_package_size, rec_total_len);
        ota_state = APP_OTA_STATE_IDLE;
        return;
    }
    ota_state = APP_OTA_STATE_JUMP;
}



void app_ota_work(void)
{
    switch (ota_state)
    {
        // 初始化OTA应用
    case APP_OTA_STATE_IDLE:
        app_ota_init();
        break;
    case APP_OTA_STATE_WAIT_ACK:
        app_ota_wait_ack();
        break;
    case APP_OTA_STATE_RUNNING:
        app_ota_running();
        break;
    case APP_OTA_STATE_CHECK:
        app_ota_check();
        break;
    case APP_OTA_STATE_JUMP:
        Int_bootloader_jump_to_app();
        break;
    default:
        break;
    }
}
