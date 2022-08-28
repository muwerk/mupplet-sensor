// mup_rain_ad.h
#pragma once

#include "scheduler.h"
#include "sensors.h"

namespace ustd {

// clang-format off
/*! \brief mupplet-sensor analog, digital rain sensor

The rain_ad mupplet measures rain using a simple analog and digital MH-AD rain sensor
(rain detector with analog and digital output)

#### Messages sent by rain_ad mupplet:

| topic | message body | comment |
| ----- | ------------ | ------- |
| `<mupplet-name>/sensor/rain` | `ON` or `OFF` | state of digital rain sensor |
| `<mupplet-name>/sensor/unitrain` | normalized rain [0.0-1.0] | Float value encoded as string |
|  `<mupplet-name>/sensor/mode` | `FAST`, `MEDIUM`, or `LONGTERM` | Integration time for rain values |

#### Messages received by rain_ad mupplet:

| topic | message body | comment |
| ----- | ------------ | ------- |
| `<mupplet-name>/sensor/rain/get` | - | Causes current value to be sent with. |
| `<mupplet-name>/sensor/unitrain/get` | - | Causes current value to be sent with. |
| `<mupplet-name>/sensor/mode/get` | - | Returns filterMode: `FAST`, `MEDIUM`, or `LONGTERM` |
| `<mupplet-name>/sensor/mode/set` | `FAST`, `MEDIUM`, or `LONGTERM` | Set integration time for rain values |

<img src="https://github.com/muwerk/mupplet-sensor/blob/master/extras/RainAD.png" width="30%"
height="30%"> Hardware: RainAD, 10kΩ resistor

#### Sample code
```cpp
#define __ESP__ 1
#include "scheduler.h"
#include "mup_rain_ad.h"

ustd::Scheduler sched;
ustd::rainAD RainAD("myRainAD",A0,D2);

void task0(String topic, String msg, String originator) {
    if (topic == "myRainAD/sensor/unitrain") {
        Serial.print("rain: ");
        Serial.prinln(msg);  // String float [0.0, ..., 1.0]
    }
}

void setup() {
   RainAD.begin(&sched);
}
```

Note: For ESP32 make sure to use a port connected to ADC #1, since ADC #2 conflicts with Wifi
and ports connected to ADC #2 cannot be used concurrently with Wifi!
*/
// clang-format on
class RainAD {
  private:
    String RainAD_VERSION = "0.1.0";
    Scheduler *pSched;
    int tID;
    String name;
    uint8_t analogPort;
    uint8_t digitalPort;
    bool digitalState;
    double RainADvalue;
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
    ustd::sensorprocessor rainSensor = ustd::sensorprocessor(4, 600, 0.005);

    RainAD(String name, uint8_t analogPort, uint8_t digitalPort, FilterMode filterMode = FilterMode::MEDIUM)
        : name(name), analogPort(analogPort), digitalPort(digitalPort), filterMode(filterMode) {
        /*! Instantiate an RainAD sensor mupplet
        @param name Name used for pub/sub messages
        @param analogPort GPIO port with A/D converter capabilities.
        @param digitalPort GPIO port for digital rain input
        @param filterMode FAST, MEDIUM or LONGTERM filtering of sensor values of analog port
        */
        setFilterMode(filterMode, true);
    }

    ~RainAD() {
    }

    double getUnitRain() {
        /*! Get current unit illumance
        @return Unit-rain [0.0(dark) - 1.0(max. rain)]
        */
        return RainADvalue;
    }

    void begin(Scheduler *_pSched, uint32_t _pollRateMs = 2000) {
        /*! Start processing of A/D input from RainAD */
        pSched = _pSched;
        pollRateMs = _pollRateMs;

        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, basePollRate);

        pinMode(digitalPort, INPUT_PULLUP);
        digitalState = digitalRead(digitalPort);  // this is inverse logic, but we initialise with the opposite value to cause an initial publish...

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
            rainSensor.update(1, 15, 0.001);
            break;
        case MEDIUM:
            filterMode = MEDIUM;
            rainSensor.update(4, 300, 0.005);
            break;
        case LONGTERM:
        default:
            filterMode = LONGTERM;
            rainSensor.update(50, 600, 0.01);
            break;
        }
        if (!silent)
            publishFilterMode();
    }

  private:
    void publishRain() {
        char buf[32];
        sprintf(buf, "%5.3f", RainADvalue);
        pSched->publish(name + "/sensor/unitrain", buf);
        if (digitalState) {
            strcpy(buf, "ON");
        } else {
            strcpy(buf, "OFF");
        }
        pSched->publish(name + "/sensor/rain", buf);
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
                bool hasChanged = false;
                lastPollMs = millis();
                double val = 1.0 - (analogRead(analogPort) / (adRange - 1.0));
                if (rainSensor.filter(&val)) {
                    RainADvalue = val;
                    hasChanged = true;
                }
                bool st = !digitalRead(digitalPort);  // inverse logic.
                if (st != digitalState) {
                    digitalState = st;
                    hasChanged = true;
                }
                if (hasChanged) publishRain();
            }
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/sensor/unitrain/get" || topic == name + "/sensor/rain/get") {
            publishRain();
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
};  // rainAD

}  // namespace ustd
