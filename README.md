# epHamClock
Epaper Ham Clock

## Parts

1. [Epaper Display](https://www.waveshare.com/product/displays/e-paper/epaper-2/2.9inch-e-paper-module.htm?___SID=U) - [wiki](https://www.waveshare.com/wiki/2.9inch_e-Paper_Module)
2. [DS18B20 Temperature Sensor](https://datasheets.maximintegrated.com/en/ds/DS18B20.pdf) (PDF Link)
3. [NEO 6M GPS](https://www.amazon.com/HiLetgo-GY-NEO6MV2-Controller-Ceramic-Antenna/dp/B01D1D0F5M) (Amazon link). Any 4-pin UART device should work.
4. An [Arduino](https://www.arduino.cc/) that sports an ATMEGA328. I did initial development on a Nano, and then built my clock using a naked ATMEGA328P 28 pin IC.


## Libraries
1. For ereader: [epd2in9_V2](https://github.com/waveshare/e-Paper/tree/master/Arduino/epd2in9_V2)
2. For DS18B20: [DS18B20](https://github.com/matmunk/DS18B20)
3. [TinyGps+](http://arduiniana.org/libraries/tinygpsplus/)
4. [maidenhead](https://www.arduino.cc/reference/en/libraries/maidenhead/)

## Notes

<b>DO NOT USE DIGITAL PIN 4</b> for the temperature sensor or anything else. Some weird interplay there was causing I2C to freeze. Moving off that pin fixed the problem.




