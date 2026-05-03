# Lawn sprinkler controller

## 1. Overview

ESP32 based firmware (ESP-IDF / FreeRTOS) for a 6-zone lawn-sprinkler controller.
It drives 6 active-high triac outputs with mutual exclusion (only one zone
ever on at a time), runs a cron-style schedule against local time in **AUTO**
mode, and accepts a **MANUAL** override from either the on-box push buttons
or Home Assistant.

Schedules and the active mode persist in NVS, so the controller resumes its
program after a power cycle.

State is exchanged over MQTT, and on (re)connect
the device publishes a single Home Assistant device-discovery payload — all
entities (mode select, duration select, six per-zone switches, version sensor)
appear grouped under one device, no manual HA configuration required.

## 2. Features

### **AUTO mode** :
- Select zones to activate (checkbox)
- Scheduled watering (start time and duration) for each zone.
- Automatic switching between zones.
- Total duration is displayed on HA

### **MANUAL mode** :

1. On the control box:

    a) Pressing one of the 6 push buttons starts watering the zone and stops watering the other zones.

    b) Pressing it a second time stops watering the zone.

    c) If AUTO mode is active, it switches to manual mode.

    d) The system automatically returns to AUTO mode after 30 minutes without a zone being manually activated.

2. On the HA app:

    Set a duration, then select a zone to water.

### Status LED

- **AUTO mode LED** :
  - ON, the lawn sprinkler is in AUTO mode
  - OFF, MANUAL mode is active.
- **STATUS LED** : gives indication on the status of the lawn sprinkler controller
  - OFF: The device is turned off
  - Solid ON: The device is booting up
  - low flashing (100ms ON, 3000ms period): The device is connected to Wi-Fi/MQTT
  - Fast flashing (500ms ON, 1000ms period): The device was unable to connect to Wi-Fi/MQTT
- **Zone LED** : Each zone has a LED indicating whether the zone is active or not-
  - ON : the zone is currently active
  - OFF: the zone is inactive

## 3. Hardware

Designed around a Waveshare esp32-C6 dev kit N8.
Drives 6 triac BT136-600 in Dpak package
Uses former Rainbird ESP-RXZe controller housing and its 24Vac/0.65A transformer

Electronic board designed using Kicad 9

## 4. Software
Developped with esp idf on freeRTOS architecture

### 4.1. Build & flash

```bash
. $IDF_PATH/export.sh
make build
make flash PORT=/dev/<port> # example: /dev/ttyACM0
make monitor
```

`make_version.py` derives `VERSION` and `BUILD_ID_SHORT` from `git describe`,
so make sure at least one tag exists (e.g. `git tag v0.1.0`).

### 4.2. Project layout

```
main/
  zone/        6 active-high outputs (MOC3041 triac drivers), mutual exclusion
  button/      6 push buttons via iot_button, one click -> controller event
  controller/  AUTO/MANUAL state machine, schedule firing, run/idle timers
  schedule/    up to 32 cron entries persisted in NVS
  wifi/        Wi-Fi STA + SNTP/timezone
  mqtt/        MQTT client, topic dispatcher, Home Assistant discovery
```

### 4.3. Configuration

Set via `idf.py menuconfig` -> *Lawn Sprinkler Controller Settings*:

### 4.4. MQTT API

With `MQTT_TOPIC_PREFIX = "lawn-sprinkler/"`:

#### Subscribed (HA -> device)

| Topic                         | Payload                | Effect                                         |
| ----------------------------- | ---------------------- | ---------------------------------------------- |
| `lawn-sprinkler/mode/set`     | `AUTO` \| `MANUAL`     | Switch mode                                    |
| `lawn-sprinkler/duration/set` | minutes (e.g. `10`)    | Set MANUAL default duration                    |
| `lawn-sprinkler/zone/<n>/set` | `ON` \| `OFF`          | Toggle zone (uses the current MANUAL duration) |
| `lawn-sprinkler/schedule/set` | JSON array (see below) | Replace schedule, persist to NVS               |

#### Published (device -> HA), retained

| Topic                           | Payload                    |
| ------------------------------- | -------------------------- |
| `lawn-sprinkler/mode/state`     | `AUTO` \| `MANUAL`         |
| `lawn-sprinkler/duration/state` | minutes (e.g. `10`)        |
| `lawn-sprinkler/zone/<n>/state` | `ON` \| `OFF`              |
| `lawn-sprinkler/schedule/state` | JSON array (see below)     |
| `lawn-sprinkler/availability`   | `online` (LWT: `offline`)  |
| `lawn-sprinkler/version`        | `vX.Y.Z - <git-short-sha>` |

#### Schedule JSON

```json
[
  {"zone": 0, "hour": 6,  "minute": 30, "dow": 127, "duration_min": 10, "enabled": true},
  {"zone": 3, "hour": 21, "minute": 0,  "dow": 42,  "duration_min": 15, "enabled": true}
]
```

- `zone`: 0..5
- `hour`: 0..23, `minute`: 0..59 (local time, after SNTP sync)
- `dow`: bitmask of days, **bit 0 = Sunday** ... bit 6 = Saturday (`127` = every day)
- `duration_min`: duration of watering
- `enabled`: optional, defaults to `true`

### 4.5. Home Assistant discovery

On every (re)connect the device publishes a single retained message using
Home Assistant's device-based discovery:

- `homeassistant/device/<DEVICE_ID>/config`

The payload describes the device once and lists every component nested under
`components`:

- `mode` - select (`AUTO` / `MANUAL`)
- `duration` - number (`0..60` minutes, `step=1`) — default applied to a manual button press
- `zone_0` ... `zone_5` - one switch per zone
- `version` - diagnostic sensor (firmware version + short build id)

Shared `device`, `origin` and `availability` blocks live at the top of the
payload, so all entities are grouped under one device in HA and follow the
same online/offline state via the `availability` topic.

### 4.6 Home Assistant card

Notes:

- This example uses `custom:button-card` which must be downloaded first. If you want a simpler option using only the UI, you can display the switches from MQTT discovery.

First, we need to create the template in Dashboard UI. This prevent a lot of duplication, as we will need it for each zone button. Go to your dashboard, then click *Edit* > *Raw Configuration Editor*. Paste the following snippet at the top. Modify the colors and style according to your needs.

```yml
button_card_templates:
  lawn_sprinkler_button_base:
    icon: mdi:sprinkler-variant
    show_name: false
    show_state: false
    styles:
      card:
        - padding: 8px
        - height: 48px
        - border-radius: 12px
        - --zone-color: var(--blue-color)
      icon:
        - width: 26px
        - height: 26px
        - color: var(--zone-color)
    state:
      - value: 'on'
        styles:
          card:
            - background-color: var(--zone-color)
          icon:
            - color: white
    tap_action:
      action: call-service
      service: switch.toggle
```

Then, create the cards.

```yml
type: vertical-stack
cards:
  - type: horizontal-stack
    cards:
      - type: tile
        entity: select.lawn_sprinkler_mode
        features_position: bottom
        vertical: false
        color: deep-orange
        name: Mode
        hide_state: false
        state_content:
          - state
          - last_changed
        grid_options:
          columns: full
        icon_tap_action:
          action: none
        tap_action:
          action: more-info
      - type: tile
        entity: select.lawn_sprinkler_duration
        features_position: bottom
        vertical: false
        name: Duration
        hide_state: false
        state_content:
          - state
        icon_tap_action:
          action: none
        tap_action:
          action: more-info
  - type: horizontal-stack
    cards:
      - type: custom:button-card
        template: lawn_sprinkler_button_base
        entity: switch.lawn_sprinkler_zone_0
        styles:
          card:
            - --zone-color: var(--blue-color)
        tap_action:
          action: call-service
          service: switch.toggle
          target:
            entity_id: switch.lawn_sprinkler_zone_0
      - type: custom:button-card
        template: lawn_sprinkler_button_base
        entity: switch.lawn_sprinkler_zone_1
        styles:
          card:
            - --zone-color: var(--green-color)
        tap_action:
          action: call-service
          service: switch.toggle
          target:
            entity_id: switch.lawn_sprinkler_zone_1
      - type: custom:button-card
        template: lawn_sprinkler_button_base
        entity: switch.lawn_sprinkler_zone_2
        styles:
          card:
            - --zone-color: var(--purple-color)
        tap_action:
          action: call-service
          service: switch.toggle
          target:
            entity_id: switch.lawn_sprinkler_zone_2
  - type: horizontal-stack
    cards:
      - type: custom:button-card
        template: lawn_sprinkler_button_base
        entity: switch.lawn_sprinkler_zone_3
        styles:
          card:
            - --zone-color: var(--red-color)
        tap_action:
          action: call-service
          service: switch.toggle
          target:
            entity_id: switch.lawn_sprinkler_zone_3
      - type: custom:button-card
        template: lawn_sprinkler_button_base
        entity: switch.lawn_sprinkler_zone_4
        styles:
          card:
            - --zone-color: var(--cyan-color)
        tap_action:
          action: call-service
          service: switch.toggle
          target:
            entity_id: switch.lawn_sprinkler_zone_4
      - type: custom:button-card
        template: lawn_sprinkler_button_base
        entity: switch.lawn_sprinkler_zone_5
        styles:
          card:
            - --zone-color: var(--yellow-color)
        tap_action:
          action: call-service
          service: switch.toggle
          target:
            entity_id: switch.lawn_sprinkler_zone_5

```

Notes:

- The entity names must match between the MQTT discovery and the yml snippet.

## Project layout

```
main/
  zone/        6 active-high outputs (MOC3041 triac drivers), mutual exclusion
  button/      6 push buttons via iot_button, one click -> controller event
  led/         6 status LEDs (one per zone)
  controller/  AUTO/MANUAL state machine, schedule firing, run-until expiry,
               30-min auto-return to AUTO
  schedule/    up to 32 cron entries persisted in NVS
  wifi/        Wi-Fi STA + SNTP/timezone
  mqtt/        MQTT client, topic dispatcher, Home Assistant discovery
```

GPIO assignments are placeholders in the three `*_config.h` files
(`zone/zone_config.h`, `button/button_config.h`, `led/led_config.h`) - update
once the schematic is final.


## Tunables

Easy-to-tweak knobs in `controller/controller.c`:

- `MANUAL_AUTORETURN_SEC` (default `30 * 60`) - idle timeout before MANUAL
  reverts to AUTO.
- `MANUAL_DEFAULT_DURATION_SEC` (default `0`) - duration applied to a button
  press in MANUAL. `0` means run until the user stops it; set a positive value
  for a safety cap.
- The auto-return rule: by default the 30-min timer only counts while no zone
  is active. The condition is in `tick_manual()` if you want to change it.


## Configuration

Set via `idf.py menuconfig` -> *Lawn Sprinkler Controller Settings*:

| Option              | Purpose                                              |
|---------------------|------------------------------------------------------|
| `WIFI_SSID`         | Wi-Fi credentials                                    |
| `WIFI_PASSWORD`     |                                                      |
| `MQTT_URI`          | `mqtt://user:pass@host:port`                         |
| `MQTT_TOPIC_PREFIX` | Base topic, must end with `/` (e.g. `lawn-sprinkler/`) |
| `MQTT_TOPIC_VERSION`| Version sub-topic name                               |
| `DEVICE_ID`         | Used in HA discovery `unique_id`s                    |
| `DEVICE_NAME`       | Friendly name shown in HA                            |
| `NTP_SERVER`        | e.g. `pool.ntp.org`                                  |
| `NTP_TZ`            | POSIX TZ string (default: Europe/Paris)              |


## MQTT API

With `MQTT_TOPIC_PREFIX = "lawn-sprinkler/"`:

### Subscribed (HA -> device)

| Topic                            | Payload                                | Effect                                       |
|----------------------------------|----------------------------------------|----------------------------------------------|
| `lawn-sprinkler/mode/set`        | `AUTO` \| `MANUAL`                     | Switch mode                                  |
| `lawn-sprinkler/zone/<n>/set`    | `ON` \| `OFF`                          | Start zone (run-forever) / stop zone         |
| `lawn-sprinkler/zone/<n>/run`    | seconds (e.g. `600`)                   | Start zone with timeout (`0` or empty = stop)|
| `lawn-sprinkler/schedule/set`    | JSON array (see below)                 | Replace schedule, persist to NVS             |

### Published (device -> HA), retained

| Topic                              | Payload                       |
|------------------------------------|-------------------------------|
| `lawn-sprinkler/mode/state`        | `AUTO` \| `MANUAL`            |
| `lawn-sprinkler/zone/<n>/state`    | `ON` \| `OFF`                 |
| `lawn-sprinkler/schedule/state`    | JSON array (see below)        |
| `lawn-sprinkler/availability`      | `online` (LWT: `offline`)     |
| `lawn-sprinkler/version`           | `vX.Y.Z - <git-short-sha>`    |

### Schedule JSON

```json
[
  {"zone": 0, "hour": 6,  "minute": 30, "dow": 127, "duration": 600, "enabled": true},
  {"zone": 3, "hour": 21, "minute": 0,  "dow": 42,  "duration": 900, "enabled": true}
]
```

- `zone`: 0..5
- `hour`: 0..23, `minute`: 0..59 (local time, after SNTP sync)
- `dow`: bitmask of days, **bit 0 = Sunday** ... bit 6 = Saturday (`127` = every day)
- `duration`: seconds
- `enabled`: optional, defaults to `true`

### Home Assistant discovery

On every (re)connect the device publishes retained discovery messages under
`homeassistant/`:

- `homeassistant/select/<DEVICE_ID>/mode/config` - mode select (`AUTO`/`MANUAL`)
- `homeassistant/switch/<DEVICE_ID>/zone_<n>/config` - one switch per zone

All entities share the same `device` block so they appear grouped in HA.


## Build & flash

```bash
. $IDF_PATH/export.sh
make build           # or: idf.py build
make flash PORT=/dev/ttyACM0
make monitor
```

`make_version.py` derives `VERSION` and `BUILD_ID_SHORT` from `git describe`,
so make sure at least one tag exists (e.g. `git tag v0.1.0`).
