[![PlatformIO CI][image_CI]][badge_CI] [![Dev Docs][image_DOC]][badge_DOC]

mupplet-sensor
==============

**mu**werk a**pplet**s; mupplets: functional units that support specific hardware or reusable
applications.

**mupplets** use muwerks MQTT-style messaging to pass information between each other on the
same device. If connected to an MQTT-server via munet, all functionallity is externally
available through an MQTT server such as Mosquitto.

The `mupplet-sensor` library consists of the following modules:

<img src="https://github.com/muwerk/mupplet-sensor/blob/master/extras/ldr.png" align="right" width="5%" height="5%">

* [mup_illuminance_ldr][IlluminanceLdr_DOC] The `IlluminanceLdr` mupplet implements a simple LDR
  connected to analog port. See [IlluminanceLdr Application Notes][IlluminanceLdr_NOTES].

<img src="https://github.com/muwerk/mupplet-sensor/blob/master/extras/dht22.png" align="right" width="7%" height="7%">

* [mup_temphum_dht][TempHum_DOC] A mupplet for DHT22 temperature and humidity. See [DHT22 Applications Notes][TempHum_NOTES].

<img src="https://github.com/muwerk/mupplet-sensor/blob/master/extras/oled.png" align="right" width="7%" height="7%">

* [mup_gfx_panel][Gfx_panel_DOC] The `Oled` mupplet allows to display 2-4 sensor values on an Oled display. 
  The values displayed can either be generated locally or imported via MQTT. A JSON file discribes
  format and data sources. See [GFX Panel Application Notes][Gfx_panel_NOTES].

![RealTime](https://github.com/muwerk/mupplet-sensor/blob/master/extras/tft.gif)

Dependencies
------------

* Note: **All** mupplets require the libraries [ustd][gh_ustd], [muwerk][gh_muwerk] and
 [mupplet-core][gh_mupcore]

For ESP8266 and ESP32, it is recommended to use [munet][gh_munet] for network connectivity.

### Additional hardware-dependent libraries ###

Note: third-party libraries may be subject to different licensing conditions.

| Mupplet                     | Function | Hardware | Dependencies    |
| --------------------------- | -------- | -------- | --------------- |
| `mup_illuminance_ldr.h`     | Illuminance | LDR connected to analog port | none |
| `mup_illuminance_tsl2561.h` | Illuminance | TSL2561 | Wire |
| `mup_presstemp_bmp180.h` | Pressure, temperature | [Bosch BMP180][1] | Wire |
| `mup_presstemp_bmp280.h` | Pressure, temperature | Bosch BMP280 | Wire |
| `mup_presstemp_bme280.h` | Pressure, temperature, humidity | Bosch BME280 | Wire |
| `mup_temphum_dht` | Temperature, humidity | DHT22 | none |
| `mup_gamma_gdk101` | Gamma radiation uS/h | GDK101 | Wire |
| `mup_gfx_panel` | Oled or TFT display for sensor values and plots | SSD1306, ST7735 | Wire, SPI, Adafruit BusIO, Adafruit GFX Library, Adafruit SSD1306, Adafruit ST7735 and ST7789 Library |

History
-------
- 0.1.4 ongoing...
- 0.1.3 (2022-07-28) Oled and doc upgrades.
- 0.1.2 (2022-07-27) DHT and Oled added.
- 0.1.0 (2021-02-XX) (Not yet Released) Illuminance LDR Sensor, BMP180 pressure sensor

More mupplet libraries
----------------------

- [mupplet-core][gh_mupcore] microWerk Core mupplets
- [mupplet-display][gh_mupdisplay] microWerk Display mupplets

References
----------

- [ustd][gh_ustd] microWerk standard library
- [muwerk][gh_muwerk] microWerk scheduler
- [munet][gh_munet] microWerk networking


[badge_CI]: https://github.com/muwerk/mupplet-sensor/actions
[image_CI]: https://github.com/muwerk/mupplet-sensor/workflows/PlatformIO%20CI/badge.svg
[badge_DOC]: https://muwerk.github.io/mupplet-sensor/docs/index.html
[image_DOC]: https://img.shields.io/badge/docs-dev-blue.svg

[IlluminanceLdr_DOC]: https://muwerk.github.io/mupplet-sensor/docs/classustd_1_1IlluminanceLdr.html
[IlluminanceLdr_NOTES]: https://github.com/muwerk/mupplet-sensor/blob/master/extras/illuminance-ldr-notes.md
[TempHum_DOC]: https://muwerk.github.io/mupplet-sensor/docs/classustd_1_1TempHumDHT.html
[TempHum_Notes]: https://github.com/muwerk/mupplet-sensor/blob/master/extras/temphum-dht-notes.md
[Gfx_panel_DOC]: https://muwerk.github.io/mupplet-sensor/docs/classustd_1_1GfxPanel.html
[Gfx_panel_NOTES]: https://github.com/muwerk/mupplet-sensor/blob/master/extras/gfx-panel-notes.md

[gh_ustd]: https://github.com/muwerk/ustd
[gh_muwerk]: https://github.com/muwerk/muwerk
[gh_munet]: https://github.com/muwerk/munet
[gh_mufonts]: https://github.com/muwerk/mufonts
[gh_mupcore]: https://github.com/muwerk/mupplet-core
[gh_mupdisplay]: https://github.com/muwerk/mupplet-display
[gh_mupsensor]: https://github.com/muwerk/mupplet-sendsor

[1]: https://www.digikey.com/htmldatasheets/production/856385/0/0/1/bmp180-datasheet.html
