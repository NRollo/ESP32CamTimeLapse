# ESP32Cam Time-Lapse camera
HW modifications has been made to insure < 1mA operation. Pictures (jpeg) are saved on the SD card, Day and Night deep sleep operation implemented in order to filter out useless
night pictures and stay in deep sleep during night time. The camera used is the supported OV5640 (5 megapixel). The system is able to operate autonomously for 3 - 4 month on a construction site with pictures taken every 15 min. during the day time.
# HW modifications
(See HW folder for reference) The 3.3V power supply for the CAM has been moved to Q2 in order to power off the CAM during deep sleep. Also a new battery power supply has been added consisting of a AA backup battery for deep sleep use and a solar charged powerbank to handle normal operation. The system uses ~600 uA in deep sleep which
could be less if also the SD card and the PSRAM was powered down the same way as the CAM as shown with the dotted lines (the SD card uses ~300 uA in standby). The flash LED has
been disconnected and the LED output is now used to wakeup the powerbank which shuts down after ~30 Sec. during deep sleep. The 5V to 3.3V LDO has been changed to low quiescent
current type. The CAM_PWR signal is now also routed to the PWDN pin of the CAM module, in order to have a more controled power down sequence.
# SW
TO DO
