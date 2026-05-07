#include "button.h"
#include "controller.h"
#include "esp_err.h"
#include "mqtt.h"
#include "nvs_flash.h"
#include "schedule.h"
#include "sntp.h"
#include "wifi.h"
#include "zone.h"

void app_main(void)
{
    /* NVS must be initialised before any module that reads/writes it
     * (schedule, wifi). */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        nvs_flash_erase();
        nvs_flash_init();
    }

    zone_init();

    schedule_init();

    wifi_init();
    time_sync_init();

    mqtt_init();

    controller_init();

    button_init();
}
