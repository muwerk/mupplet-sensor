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
| `mup_co2_ccs811` | CO2, VOC | CCS811 | Wire |
| `mup_rain_ad` | Analog and digital rain sensor | MH-RD rain sensor (china) | none |

History
-------
- 0.2.2 (unpublished) New mode `inverseLogic` for `IlluminanceLdr`, new sensor CO2/VOC CCS811.
- 0.2.1 (2022-09-27) Fixes for sensor BL0397 power measurement for Gosund SP1 tests. 
- 0.2.0 (2022-09-23) GfxPanel moved to mupplet-display, multiple sensors added, documentation still incomplete and ongoing.
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


[gh_ustd]: https://github.com/muwerk/ustd
[gh_muwerk]: https://github.com/muwerk/muwerk
[gh_munet]: https://github.com/muwerk/munet
[gh_mufonts]: https://github.com/muwerk/mufonts
[gh_mupcore]: https://github.com/muwerk/mupplet-core
[gh_mupdisplay]: https://github.com/muwerk/mupplet-display
[gh_mupsensor]: https://github.com/muwerk/mupplet-sendsor

[1]: https://www.digikey.com/htmldatasheets/production/856385/0/0/1/bmp180-datasheet.html
