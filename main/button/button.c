#include "button.h"

#include "button_config.h"
#include "button_gpio.h"
#include "controller.h"
#include "esp_log.h"
#include "iot_button.h"
#include "utils.h"

static const char* TAG = "button";

static const int button_gpios[] = {GPIO_BTN_ZONE_1,
                                   GPIO_BTN_ZONE_2,
                                   GPIO_BTN_ZONE_3,
                                   GPIO_BTN_ZONE_4,
                                   GPIO_BTN_ZONE_5,
                                   GPIO_BTN_ZONE_6};

static void on_single_click(void* arg, void* user_data)
{
    uint8_t zone = (uint8_t)(uintptr_t)user_data;
    ESP_LOGI(TAG, "zone %u button click", zone);
    controller_on_button(zone);
}

void button_init(void)
{
    const button_config_t btn_cfg = {
        .long_press_time = BUTTON_LONG_PRESS_TIME,
        .short_press_time = BUTTON_SHORT_PRESS_TIME,
    };

    for (uint8_t i = 0; i < ARRAY_DIM(button_gpios); i++)
    {
        const button_gpio_config_t btn_gpio_cfg = {
            .gpio_num = button_gpios[i],
            .active_level = BUTTON_ACTIVE_LEVEL,
            .disable_pull = false,
        };

        button_handle_t btn = NULL;
        esp_err_t ret =
            iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &btn);
        ESP_ERROR_CHECK(ret);

        ret = iot_button_register_cb(btn,
                                     BUTTON_SINGLE_CLICK,
                                     NULL,
                                     on_single_click,
                                     (void*)(uintptr_t)i);
        ESP_ERROR_CHECK(ret);
    }
}
