[![PlatformIO CI][image_CI]][badge_CI] [![Dev Docs][image_DOC]][badge_DOC]

mupplet-sensor
==============

**mu**werk a**pplet**s; mupplets: functional units that support specific hardware or reusable
applications.

**mupplets** use muwerks MQTT-style messaging to pass information between each other on the
same device. If connected to an MQTT-server via munet, all functionallity is externally
available through an MQTT server such as Mosquitto.

The `mupplet-sensor` library consists of the following modules:

* [IlluminanceLdr][IlluminanceLdr_DOC] The `IlluminanceLdr` mupplet implements a simple LDR
  connected to analog port. See [IlluminanceLdr Application Notes][IlluminanceLdr_NOTES]

Dependencies
------------

* Note: **All** mupplets require the libraries [ustd][gh_ustd], [muwerk][gh_muwerk] and
 [mupplet-core][gh_mupcore]

For ESP8266 and ESP32, it is recommended to use [munet][gh_munet] for network connectivity.

### Additional hardware-dependent libraries ###

Note: third-party libraries may be subject to different licensing conditions.

Mupplet                     | Function | Hardware | Dependencies
--------------------------- | -------- | -------- | ---------------
`mup_illuminance_ldr.h`     | Illuminance | LDR connected to analog port |
`mup_illuminance_tsl2561.h` | Illuminance | [Adafruit TSL2561][2] | Wire, [Adafruit Unified Sensor][1], [Adafruit TSL2561][2]

History
-------

- 0.1.0 (2021-02-XX) (Not yet Released) Illuminance LDR Sensor

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

[gh_ustd]: https://github.com/muwerk/ustd
[gh_muwerk]: https://github.com/muwerk/muwerk
[gh_munet]: https://github.com/muwerk/munet
[gh_mufonts]: https://github.com/muwerk/mufonts
[gh_mupcore]: https://github.com/muwerk/mupplet-core
[gh_mupdisplay]: https://github.com/muwerk/mupplet-display
[gh_mupsensor]: https://github.com/muwerk/mupplet-sendsor

[1]: https://github.com/adafruit/Adafruit_Sensor
[2]: https://github.com/adafruit/Adafruit_TSL2561
