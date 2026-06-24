set(srcs
    main.c
    ha_entities.c
    button/button.c
    controller/controller.c
    led/led.c
    zone/zone.c
)

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "."
                 "button"
                 "controller"
                 "led"
                 "utils"
                 "zone"
    REQUIRES esp_driver_gpio
             esp_netif
             nvs_flash
             ha-mqtt
             wifi
)

# Add build directory to resolve version.h
target_include_directories(${COMPONENT_LIB} PRIVATE
    "${CMAKE_BINARY_DIR}/"
)
