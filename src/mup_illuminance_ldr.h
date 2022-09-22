// mup_illuminance_ldr.h
#pragma once

/*! \mainpage mupplet-sensor is a collection of hardware sensor applets for the muwerk scheduler

\section Introduction

mupplet-sensor implements the following classes based on the cooperative scheduler muwerk:

* * \ref ustd::BinarySensor
* * \ref ustd::AnalogSensor
* * \ref ustd::IlluminanceLdr
* * \ref ustd::IlluminanceTSL2561
* * \ref ustd::PressTempBMP180
* * \ref ustd::PressTempBMP280
* * \ref ustd::PressTempHumBME280
* * \ref ustd::TempHumDHT
* * \ref ustd::GammaGDK101
* * \ref ustd::RainAD
* * \ref ustd::PowerBl0937

Helper:

* * \ref ustd::I2CRegisters

For an overview, see:
<a href="https://github.com/muwerk/mupplet-sensor/blob/master/README.md">mupplet-sensor readme</a>

Libraries are header-only and should work with any c++11 compiler and
and support platforms esp8266 and esp32.

This library requires the libraries ustd, muwerk and requires a
<a href="https://github.com/muwerk/ustd/blob/master/README.md">platform
define</a>.

\section Reference
* * <a href="https://github.com/muwerk/mupplet-sensor">mupplet-sensor github repository</a>
* * <a href="https://github.com/muwerk/mupplet-core">mupplet-core github repository</a>

depends on:
* * <a href="https://github.com/muwerk/ustd">ustd github repository</a>
* * <a href="https://github.com/muwerk/muwerk">muwerk github repository</a>
* * <a href="https://github.com/muwerk/munet">munet github repository</a>

*/

#include "scheduler.h"
#include "sensors.h"

namespace ustd {

// clang-format off
/*! \brief mupplet-sensor analog LDR illumance sensor

The illuminance_ldr mupplet measures illuminance using a simple analog LDR
(light dependent resistor)

#### Messages sent by illuminance_ldr mupplet:

| topic | message body | comment |
| ----- | ------------ | ------- |
| `<mupplet-name>/sensor/unitilluminance` | normalized illuminance [0.0-1.0] | Float value encoded as string |
|  `<mupplet-name>/sensor/mode` | `FAST`, `MEDIUM`, or `LONGTERM` | Integration time for illuminance values |

#### Messages received by illuminance_ldr mupplet:

| topic | message body | comment |
| ----- | ------------ | ------- |
| `<mupplet-name>/sensor/unitilluminance/get` | - | Causes current value to be sent with. |
| `<mupplet-name>/sensor/mode/get` | - | Returns filterMode: `FAST`, `MEDIUM`, or `LONGTERM` |
| `<mupplet-name>/sensor/mode/set` | `FAST`, `MEDIUM`, or `LONGTERM` | Set integration time for illuminance values |

<img src="https://github.com/muwerk/mupplet-sensor/blob/master/extras/ldr.png" width="30%"
height="30%"> Hardware: LDR, 10kΩ resistor

#### Sample code
```cpp
#define __ESP__ 1
#include "scheduler.h"
#include "mup_illuminance_ldr.h"

ustd::Scheduler sched;
ustd::IlluminanceLdr ldr("myLDR",A0);

void task0(String topic, String msg, String originator) {
    if (topic == "myLDR/sensor/unitilluminance") {
        Serial.print("Illuminance: ");
        Serial.prinln(msg);  // String float [0.0, ..., 1.0]
    }
}

void setup() {
   ldr.begin(&sched);
}
```

Note: For ESP32 make sure to use a port connected to ADC #1, since ADC #2 conflicts with Wifi
and ports connected to ADC #2 cannot be used concurrently with Wifi!
*/
// clang-format on
class IlluminanceLdr {
  private:
    String LDR_VERSION = "0.1.0";
    Scheduler *pSched;
    int tID;
    String name;
    uint8_t port;
    double ldrvalue;
    unsigned long basePollRate = 500000L;
    uint32_t pollRateMs = 2000;
    uint32_t lastPollMs = 0;
    bool bActive = false;
#ifdef __ESP32__
    double adRange = 4096.0;  // 12 bit default
#else
    double adRange = 1024.0;  // 10 bit default
#endif

  public:
    enum FilterMode { FAST,
                      MEDIUM,
                      LONGTERM };
    FilterMode filterMode;
    ustd::sensorprocessor illuminanceSensor = ustd::sensorprocessor(4, 600, 0.005);

    IlluminanceLdr(String name, uint8_t port, FilterMode filterMode = FilterMode::MEDIUM)
        : name(name), port(port), filterMode(filterMode) {
        /*! Instantiate an LDR sensor mupplet
        @param name Name used for pub/sub messages
        @param port GPIO port with A/D converter capabilities.
        @param filterMode FAST, MEDIUM or LONGTERM filtering of sensor values
        */
        setFilterMode(filterMode, true);
    }

    ~IlluminanceLdr() {
    }

    double getUnitIlluminance() {
        /*! Get current unit illumance
        @return Unit-Illuminance [0.0(dark) - 1.0(max. illuminance)]
        */
        return ldrvalue;
    }

    void begin(Scheduler *_pSched, uint32_t _pollRateMs = 2000) {
        /*! Start processing of A/D input from LDR */
        pSched = _pSched;
        pollRateMs = _pollRateMs;

        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, basePollRate);

        auto fnall = [=](String topic, String msg, String originator) {
            this->subsMsg(topic, msg, originator);
        };
        pSched->subscribe(tID, name + "/sensor/#", fnall);
        bActive = true;
    }

    void setFilterMode(FilterMode mode, bool silent = false) {
        switch (mode) {
        case FAST:
            filterMode = FAST;
            illuminanceSensor.update(1, 15, 0.001);
            break;
        case MEDIUM:
            filterMode = MEDIUM;
            illuminanceSensor.update(4, 300, 0.005);
            break;
        case LONGTERM:
        default:
            filterMode = LONGTERM;
            illuminanceSensor.update(50, 600, 0.01);
            break;
        }
        if (!silent)
            publishFilterMode();
    }

  private:
    void publishIlluminance() {
        char buf[32];
        sprintf(buf, "%5.3f", ldrvalue);
        pSched->publish(name + "/sensor/unitilluminance", buf);
    }

    void publishFilterMode() {
        switch (filterMode) {
        case FilterMode::FAST:
            pSched->publish(name + "/sensor/mode", "FAST");
            break;
        case FilterMode::MEDIUM:
            pSched->publish(name + "/sensor/mode", "MEDIUM");
            break;
        case FilterMode::LONGTERM:
            pSched->publish(name + "/sensor/mode", "LONGTERM");
            break;
        }
    }

    void loop() {
        if (bActive) {
            if (timeDiff(lastPollMs, millis()) >= pollRateMs) {
                lastPollMs = millis();
                double val = 1.0 - (analogRead(port) / (adRange - 1.0));
                if (illuminanceSensor.filter(&val)) {
                    ldrvalue = val;
                    publishIlluminance();
                }
            }
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/sensor/unitilluminance/get") {
            publishIlluminance();
        } else if (topic == name + "/sensor/mode/get") {
            publishFilterMode();
        } else if (topic == name + "/sensor/mode/set") {
            if (msg == "fast" || msg == "FAST") {
                setFilterMode(FilterMode::FAST);
            } else {
                if (msg == "medium" || msg == "MEDIUM") {
                    setFilterMode(FilterMode::MEDIUM);
                } else {
                    setFilterMode(FilterMode::LONGTERM);
                }
            }
        }
    };
};  // IlluminanceLdr

}  // namespace ustd
