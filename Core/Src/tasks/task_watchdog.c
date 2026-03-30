/**
 * @file    tasks/task_watchdog.c
 * @brief   IWDG watchdog feed task.
 *
 * Calls HAL_IWDG_Refresh() every WATCHDOG_FEED_PERIOD_MS.
 * If this task stops running (deadlock, hard fault, stack overflow),
 * the IWDG fires and resets the MCU.
 *
 * This task intentionally uses vTaskDelay (not vTaskDelayUntil) — if it
 * runs slightly late that is fine; what matters is it never runs too early
 * and wastes its budget before the IWDG timeout is due.
 */

#include "tasks/task_watchdog.h"
#include "app_config.h"
#include "stm32h7xx_hal.h"

extern IWDG_HandleTypeDef hiwdg1;

void task_watchdog(void *pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(WATCHDOG_FEED_PERIOD_MS));
        HAL_IWDG_Refresh(&hiwdg1);
    }
}
