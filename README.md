# Lawn sprinkler controller


### Overview


### Features
  - **AUTO mode** :
    - Scheduled watering for each zone (Scheduled are defined on HA, not in the control box) 
    - Automatic switching between zones  (only one zone active at a time) 
<br>
  - **MANUAL mode** :
  1. On the control box:
    a) Pressing one of the 6 push buttons starts watering the zone and stops watering the other zones
    b) Pressing it a second time stops watering the zone
    c) If AUTO mode is active, it switches to MANUAL mode
    d) The system automatically returns to AUTO mode after 30 minutes without a zone being manually activated  
  2. On the HA app:
    Select the zone, enter a duration, press START
  <br>
  - **Watering Schedule (HA only)**
  Programmed parameters are displayed ine a table 
    - one line per zone (1 to 6)  
    - for each zone 3 colomns : Start time, watering duration, days of the week  
    - autmatic checking that schedules do not overlap from one zone to another 
    - total duration and end time of watering cycle are listed below the table 
<br>
- **LEDs control**
1. status LED (bright blue)
    - Off: The control unit is powered down
    - Solid ON: During bootup and in the event of a serious fault
    - Slowly flashing (ON 100 ms, period 2 s): WiFi and MQTT are operational
    - Fast flashing (ON 100 ms, period 200 ms): WiFi connection lost or MQTT not responding  
2. Mode LED (green)
    - ON : AUTO mode is active
    - OFF : MANUAL mode is active  
3. EVs LED (bright red)
    6 LEDs, one per zone above push button
    - ON : the zone is active
    - OFF : the zone is not active
<br>
### Hardware


### Software
Developped with esp idf on freeRTOS architecture

