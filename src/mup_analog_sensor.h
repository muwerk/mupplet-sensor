// mup_analog_sensor_ad.h
#pragma once

#include "scheduler.h"
#include "sensors.h"

namespace ustd {

/*! \brief mupplet-sensor analog sensor sensor

The analog_sensor mupplet measures analog sensor using a simple analog sensor sensor

#### Messages sent by analog_sensor mupplet:

| topic | message body | comment |
| ----- | ------------ | ------- |
| `<mupplet-name>/sensor/unitanalogsensor` | normalized analog sensor [0.0..1.0] or [b..a+b] | Float value encoded as string, rescaled if a non-default linear transformation is given at begin() |
| `<mupplet-name>/sensor/<topicName>` | normalized analog sensor [0.0..1.0] or [b..a+b] | Float value encoded as string, topicName is an optional parameter of the object initialization |
|  `<mupplet-name>/sensor/mode` | `FAST`, `MEDIUM`, or `LONGTERM` | Integration time for analog sensor values |

#### Messages received by analog_sensor mupplet:

| topic | message body | comment |
| ----- | ------------ | ------- |
| `<mupplet-name>/sensor/unitanalogsensor/get` | - | Causes current value to be sent. |
| `<mupplet-name>/sensor/<topicName>/get` | - | Causes current value to be sent, if an additional topicName was given at object creation. |
| `<mupplet-name>/sensor/mode/get` | - | Returns filterMode: `FAST`, `MEDIUM`, or `LONGTERM` |
| `<mupplet-name>/sensor/mode/set` | `FAST`, `MEDIUM`, or `LONGTERM` | Set integration time for analog sensor values |

<img src="https://github.com/muwerk/mupplet-sensor/blob/master/extras/AnalogSensor.png" width="30%"
height="30%"> Hardware: AnalogSensor, 10kΩ resistor

#### Sample code
```cpp
#define __ESP__ 1
#include "scheduler.h"
#include "mup_analog_sensor.h"

ustd::Scheduler sched;
ustd::AnalogSensor analogSensor("myAnalogSensor",A0);

void setup() {
   analogSensor.begin(&sched);
}
```

Note: For ESP32 make sure to use a port connected to ADC #1, since ADC #2 conflicts with Wifi
and ports connected to ADC #2 cannot be used concurrently with Wifi!
*/
// clang-format on
class AnalogSensor {
  private:
    String AnalogSensor_VERSION = "0.1.0";
    Scheduler *pSched;
    int tID;
    String name;
    uint8_t analogPort;
    String topicName;
    double analogSensorValue;
    double linTransA = 1.0;  // a*x+b linear transformation of measured values, default no change.
    double linTransB = 0.0;
    unsigned long basePollRate = 500000L;
    uint32_t pollRateMs = 2000;
    uint32_t lastPollMs = 0;
    bool bActive = false;
    bool initialPublish = false;
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
    ustd::sensorprocessor analogSensor = ustd::sensorprocessor(4, 600, 0.005);

    AnalogSensor(String name, uint8_t analogPort, FilterMode filterMode = FilterMode::MEDIUM, String topicName = "")
        : name(name), analogPort(analogPort), topicName(topicName), filterMode(filterMode) {
        /*! Instantiate an AnalogSensor sensor mupplet
        @param name Name used for pub/sub messages
        @param analogPort GPIO port with A/D converter capabilities.
        @param filterMode FAST, MEDIUM or LONGTERM filtering of sensor values of analog port
        @param topicName optional additional mqtt topic name, generating additional mqtt messages
        */
        setFilterMode(filterMode, true);
    }

    ~AnalogSensor() {
    }

    double getUnitAnalogSensor() {
        /*! Get current analog sensor value
        @return Unit-analogsensor [0.0 - 1.0], if a linear transformation is used, the range is [b - a+b]
        */
        return analogSensorValue;
    }

    void begin(Scheduler *_pSched, uint32_t _pollRateMs = 2000, double _linTransA = 1.0, double _linTransB = 0.0) {
        /*! Start processing of A/D input from AnalogSensor

        The analog sensor mupplet by default measures the analog port, and maps values always into the range
        [0..1]. Optionally, a linear transformation can be applied to map the range value into a different
        range: if a value v is measured, v is by default [0..1] and now a * v + b is applied. With the defaults
        of a=1, b=0, no change is made by the transformation, since 1*v+0 = v. By changing _linTransA (for a)
        and/or _linTransB (for b), the output range can be rescaled linearily.

        Note: if you are not using the linear transformation feature, make sure that _linTransA is set to 1.0 and _linTransB is set to 0.0!

        @param _pSched pointer to muwerk scheduler object
        @param _pollRateMs Optional measurement pollrate in ms, default 2000, a measurement every 2 secs.
        @param _linTransA Optional transformation factor a, default 1.0, values v read from analog sensor are transformed with a*v+b.
        @param _linTransB Optional transformation factor b, default 0.0, values v read from analog sensor are transformed with a*v+b.
        */
        pSched = _pSched;
        pollRateMs = _pollRateMs;
        linTransA = _linTransA;
        linTransB = _linTransB;
        initialPublish = false;

        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, basePollRate);

        auto fnall = [=](String topic, String msg, String originator) {
            this->subsMsg(topic, msg, originator);
        };
        pSched->subscribe(tID, name + "/sensor/#", fnall);
        pSched->subscribe(tID, name + "/mqtt/state", fnall);
        bActive = true;
    }

    void setFilterMode(FilterMode mode, bool silent = false) {
        switch (mode) {
        case FAST:
            filterMode = FAST;
            analogSensor.update(1, 15, 0.001);
            break;
        case MEDIUM:
            filterMode = MEDIUM;
            analogSensor.update(4, 300, 0.005);
            break;
        case LONGTERM:
        default:
            filterMode = LONGTERM;
            analogSensor.update(50, 600, 0.01);
            break;
        }
        if (!silent)
            publishFilterMode();
    }

  private:
    void publishAnalogSensor() {
        char buf[32];
        sprintf(buf, "%5.3f", analogSensorValue);
        pSched->publish(name + "/sensor/unitanalogsensor", buf);
        if (topicName != "unitanalogsensor" && topicName != "") {
            pSched->publish(name + "/sensor/" + topicName, buf);
        }
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
            if (timeDiff(lastPollMs, millis()) >= pollRateMs || !initialPublish) {
                bool hasChanged = false;
                lastPollMs = millis();
                double val = analogRead(analogPort) / (adRange - 1.0);
                if (val < 0.0) val = 0.0;
                if (val > 1.0) val = 1.0;
                if (linTransA != 1.0 || linTransB != 0.0) {
                    val = linTransA * val + linTransB;
                }
                if (analogSensor.filter(&val) || !initialPublish) {
                    analogSensorValue = val;
                    hasChanged = true;
                }
                if (hasChanged) {
                    publishAnalogSensor();
                    initialPublish = true;
                }
            }
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/sensor/unitanalogsensor/get" || topic == name + "/sensor/" + topicName + "/get") {
            publishAnalogSensor();
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
        } else if (topic == name + "mqtt/state") {
            if (msg == "connected") {
                initialPublish = false;  // republish for mqtt.
            }
        }
    };
};  // analogsensorAD

}  // namespace ustd
