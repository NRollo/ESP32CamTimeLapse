# ESP32Cam Time-Lapse camera
Pictures (jpeg) are saved on the SD card, Day and Night deep sleep operation implemented in order to filter out useless night pictures and stay in deep sleep during night
time. The camera used is the ESP32Cam supported OV5640 (5 megapixel). The system was designed to be able to operate autonomously for 3 - 4 month on a construction site
with pictures taken every 15 min. during the day time. HW modifications has been made to insure < 1 mA operation. 
# HW modifications
(See HW folder for reference)
The 3.3V power supply for the CAM module has been moved from the Q2 source to the drain in order to power off the CAM module during deep sleep. the CAM module is quite power
hungry and can produce peak current of +150 mA. A battery power supply has been added consisting of a AA backup battery for deep sleep use and a solar charged powerbank to
handle peak consumption during normal operation (see seperate schematic).
The system uses ~600 uA in deep sleep which could be reduced even more if also the SD card and the PSRAM was powered down the same way as the CAM module (Q2 drain controlled
by GPIO32) as shown with the dotted lines in the schematic (the SD card alone uses ~300 uA in standby). The flash LED has been disconnected and the 'LED' (GPIO33) digital
output is used to wakeup the powerbank which (by it self) shuts down after ~30 Sec. when load drops below ~100 mA, during deep sleep. The 5V to 3.3V LDO (U2) has been changed
from the AMS1117 to the low quiescent current type MCP1702-33, reducing quiescent current in the LDO from ~5 mA to ~3 uA. The CAM_PWR signal is now also routed to the PWDN
pin of the CAM module, in order to have a more controled power down sequence.
# SW
The esp-idf framework has been used, no task creation is made in the application which runs on a single thread. The program flow is: 'boot --> execution --> deep_sleep -->
boot --> execution --> deep_sleep' etc. In order to ease the load on the AA battery backup pack a esp_wake_deep_sleep() function has been implemented, this will start the wakeup
of the powerbank as quickly as possible. A few variables are keept in the RTC memory in order to stay valid during deep sleep and boot. To distinguish between day and night mode the average light level from the picture take (OV5640 reg. 0x56A1) is used and compared to a defined 'NIGHT_LEVEL', this will trigger night mode and the system will enter 8 hours of deep sleep. When waking up the light level will be tested again and a 1 hour deep sleep will be used if it is still too dark for taking pictures. Pictures are stored on the SD card together with som log information about the pictures average light level, sleep time, timestamp and remarks like "Entering 8 hours deep sleep". This has primarily been used for debugging and tuning purposes. 

