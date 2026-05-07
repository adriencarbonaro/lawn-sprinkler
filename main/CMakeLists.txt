set(srcs
    main.c
    button/button.c
    controller/controller.c
    mqtt/mqtt.c
    mqtt/mqtt_discovery.c
    mqtt/mqtt_topics.c
    schedule/schedule.c
    wifi/sntp.c
    wifi/wifi.c
    zone/zone.c
)

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "."
                 "button"
                 "controller"
                 "mqtt"
                 "schedule"
                 "utils"
                 "wifi"
                 "zone"
    REQUIRES esp_driver_gpio
             esp_wifi
             esp_netif
             esp_event
             nvs_flash
             mqtt
             json
             lwip
)

# Add build directory to resolve version.h
target_include_directories(${COMPONENT_LIB} PRIVATE
    "${CMAKE_BINARY_DIR}/"
)
