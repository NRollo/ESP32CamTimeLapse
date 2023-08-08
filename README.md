# ESP32Cam Time-Lapse camera
HW modifications has been made to insure < 1mA operation. Pictures (jpeg) are saved on the SD card, Day and Night deep sleep operation implemented in order to filter out useless
night pictures and stay in deep sleep during night time. The camera used is the ESP32Cam supported OV5640 (5 megapixel). The system was designed to be able to operate autonomously for 3 - 4 month on a construction site with pictures taken every 15 min. during the day time.
# HW modifications
(See HW folder for reference)
The 3.3V power supply for the CAM module has been moved from the Q2 source to the drain in order to power off the CAM module during deep sleep. Also a new battery power supply has been added consisting of a AA backup battery for deep sleep use and a solar charged powerbank to handle peak consumption during normal operation (see seperate schematic). The system uses ~600 uA in deep sleep which could be less if also the SD card and the PSRAM was powered down the same way as the CAM module (Q2 drain), as shown with the dotted lines in the schematic (the SD card alone uses ~300 uA in standby). The flash LED has been disconnected and the 'LED' digital output is now used to wakeup the powerbank which (by it self) shuts down after ~30 Sec. when load drops below
~100 mA, during deep sleep. The 5V to 3.3V LDO has been changed from the to low quiescent current type. The CAM_PWR signal is now also routed to the PWDN pin of the CAM module, in order to have a more controled power down sequence.
# SW
TO DO
