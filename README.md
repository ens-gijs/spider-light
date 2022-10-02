# Spider Light
An ESP8266 based lighting control for my jumping spiders.

**PROJECT IS IN EARLY DEVELOPMENT**

# Parts and Libraries

## Consumer Ready Components
* [ESP8266 NodeMCU CP2102 ESP-12E](https://www.amazon.com/dp/B081CSJV2V)
* [0.96 Inch 128x64 Pixel OLED I2C Display Module](https://www.amazon.com/dp/B09C5K91H7)
* [COB LED Strip Lights White CCT Tunable 640LEDs/m, Dimmable 2700K-6500K 24V LED Tape Lights Kit CRI90+](https://www.amazon.com/dp/B08QMSVTY3)
* [DC-DC buck converter](https://www.amazon.com/dp/B076H3XHXP)
* [17.2x7mm Led Aluminum Channel System U Shape with Cover](https://www.amazon.com/dp/B08XM6MTTS)
* [5.5mm x 2.1mm Pigtails](https://www.amazon.com/dp/B07C7VSRBG) *or whatever size you need to match your LED power supply*
* [3-Wire 22AWG](https://www.amazon.com/dp/B08JTZKN4M)
* [1/8 inch PET Expandable Braided Cable Sleeve](https://www.amazon.com/dp/B07K1Y1Q7P)
* [Adhesive Lined Shrink Tubing](https://www.amazon.com/dp/B084GDLSCK)

## Addtional BOM Items
* [N-ch MOSFET 2N7002WT1G](https://www.mouser.com/ProductDetail/onsemi/2N7002WT1G)
  * *Any small signal N-ch MOSFET with a Vgs-th between 1 & 2V with Vds >= 40V should work fine, but don't pick one with a Vgs-th below 1V.*
* 10k resistors (pull-down)
* 1.2k resistors (gate drive)
* Custom PCB *(future item)*
* Custom 3D print encloser *(future item)*

## Arduino Libraries
* [ArduinoJson](https://arduinojson.org/?utm_source=meta&utm_medium=library.properties) by Benoit Blanchon
* [OneBitDisplay](https://github.com/bitbank2/OneBitDisplay) by bitbank2
  * [BitBang_I2C](https://github.com/bitbank2/BitBang_I2C)
