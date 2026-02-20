#ifndef __APP_H__
#define __APP_H__
#include <stdint.h>
#include "gpio.h"
#include "main.h"

typedef enum
{
    APP_OTA_STATE_IDLE = 0,
    APP_OTA_STATE_WAIT_ACK,
    APP_OTA_STATE_RUNNING,
    APP_OTA_STATE_CHECK,
    APP_OTA_STATE_JUMP,
} app_ota_state_t;

void app_ota_init(void);
void app_ota_run(void);
void app_ota_work(void);
#endif /* __APP_H__ */
