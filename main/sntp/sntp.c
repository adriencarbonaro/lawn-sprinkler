#include "sntp.h"

#include "config.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"

#include <time.h>

static const char* TAG = "sntp";

static bool synced = false;

static void on_sync(struct timeval* tv)
{
    synced = true;
    ESP_LOGI(TAG, "time synced (epoch=%lld)", (long long)tv->tv_sec);
}

void time_sync_init(void)
{
    setenv("TZ", NTP_TZ, 1);
    tzset();

    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG(NTP_SERVER);
    cfg.sync_cb = on_sync;
    cfg.start = true;

    esp_netif_sntp_init(&cfg);
    ESP_LOGI(TAG, "sntp started (server=%s, tz=%s)", NTP_SERVER, NTP_TZ);
}

bool time_sync_ready(void) { return synced; }
