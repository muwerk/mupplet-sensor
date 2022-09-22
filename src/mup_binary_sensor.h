// mup_binary_sensor.h
#pragma once

#include "scheduler.h"
#include "sensors.h"

namespace ustd {

// clang-format off
/*! \brief mupplet-sensor, binary sensor

The binary_sensor mupplet measures a digital input

#### Messages sent by binary_sensor mupplet:

Note: this mupplet uses two topics in parallel, with `sensor` and `binary_sensor` in the topic path. So applications
can chose which one to use. 

Physical and logical state values of the GPIO assigned to the binary_sensor are opposite, if inverseLogic is set to true during instantiation.
Only logical state values are sent automatically on state changes.

| topic | message body | comment |
| ----- | ------------ | ------- |
| `<mupplet-name>/sensor/<topicName>` | `ON` or `OFF` | state of binary sensor, topicName given at instantiation, default `binary_sensor`, automatically sent on state changes |
| `<mupplet-name>/binary_sensor/<topicName> | `ON` or `OFF` | state of binary sensor using binary_sensor/topicName with topicName given at object instantiation, automatically sent on state changes |
| `<mupplet-name>/sensor/physical/<topicName>` | `ON` or `OFF` | state of binary sensor, topicName given at instantiation, default `binary_sensor`, not sent automatically on state change, but only as reply to the corresponding GET message (s.b.) |
| `<mupplet-name>/binary_sensor/physical/<topicName> | `ON` or `OFF` | state of binary sensor using binary_sensor/topicName with topicName given at object instantiation, not set automatically on state change. |

#### Messages received by binary_sensor mupplet:

| topic | message body | comment |
| ----- | ------------ | ------- |
| `<mupplet-name>/sensor/<topicName>/get` | - | Causes current logical state value to be sent with. |
| `<mupplet-name>/binary_sensor/<topicName>/get` | - | Causes current logical state value to be sent. |
| `<mupplet-name>/sensor/physical/<topicName>/get` | - | Causes current physical state value to be sent with. |
| `<mupplet-name>/binary_sensor/physical/<topicName>/get` | - | Causes current physical state value to be sent. |

<img src="https://github.com/muwerk/mupplet-sensor/blob/master/extras/BinarySensor.png" width="30%"
height="30%"> Hardware: BinarySensor, 10kΩ resistor

#### Sample code
```cpp
#define __ESP__ 1
#include "scheduler.h"
#include "mup_binary_sensor.h"

ustd::Scheduler sched;
ustd::BinarySensor binarySensor("myBinarySensor",D2);

void setup() {
   binarySensor.begin(&sched);
}
```
*/
// clang-format on
class BinarySensor {
  private:
    String BinarySensor_VERSION = "0.1.0";
    Scheduler *pSched;
    int tID;
    String name;
    uint8_t digitalPort;
    bool inverseLogic;
    String topicName;
    bool logicalState;
    bool physicalState;
    unsigned long basePollRate = 500000L;
    uint32_t pollRateMs = 2000;
    uint32_t lastPollMs = 0;
    bool bActive = false;

  public:
    BinarySensor(String name, uint8_t digitalPort, bool inverseLogic = false, String topicName = "state")
        : name(name), digitalPort(digitalPort), inverseLogic(inverseLogic), topicName(topicName) {
        /*! Instantiate an BinarySensor sensor mupplet
        @param name Name used for pub/sub messages
        @param digitalPort GPIO port for binary sensor input
        @param inverseLogic default false. If true, 'ON' is signaled if GPIO is low (inverse logic)
        @param topicName MQTT topic used to send both sensor/<topicName> and binary_sensor/<topicName> messages.
        */
    }

    ~BinarySensor() {
    }

    bool getPhysicalState() {
        /*! get the current physical state of the binary sensor

            Note: this gives true for GPIO state HIGH and false for GPIO state LOW, regardless of inverseLogic setting.
            \ref getBinarySensorLogicalState()

            @return bool, current physical state of GPIO assigned to binary sensor
        */
        physicalState = digitalRead(digitalPort);
        return physicalState;
    }

    bool getBinarySensorLogicalState() {
        /*! get the current logical state of the binary sensor.

        Note: if inverseLogic is set the `true` during object instantiation, true is returned, if GPIO is low (inverted logic).
        \ref getPhysicalState()

        @return bool, current logical state
        */
        physicalState = digitalRead(digitalPort);
        if (inverseLogic) {
            logicalState = !physicalState;
        } else {
            logicalState = physicalState;
        }
        return logicalState;
    }

    void begin(Scheduler *_pSched, uint32_t _pollRateMs = 2000) {
        /*! Start processing of binary sensor input

        @param _pSched pointer to muwerk scheduler object
        @param _pollRateMs measurement interval in ms, default 2secs.
        */
        pSched = _pSched;
        pollRateMs = _pollRateMs;

        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, basePollRate);

        pinMode(digitalPort, INPUT_PULLUP);
        logicalState = !getBinarySensorLogicalState();  // we initialise with the opposite value to cause an initial publish...

        auto fnall = [=](String topic, String msg, String originator) {
            this->subsMsg(topic, msg, originator);
        };
        pSched->subscribe(tID, name + "/sensor/#", fnall);
        pSched->subscribe(tID, name + "/binary_sensor/#", fnall);
        bActive = true;
    }

  private:
    void publishPhysicalState() {
        char buf[32];
        if (getPhysicalState()) {
            strcpy(buf, "ON");
        } else {
            strcpy(buf, "OFF");
        }
        pSched->publish(name + "/sensor/physical/" + topicName, buf);
        pSched->publish(name + "/binary_sensor/physical/" + topicName, buf);
    }

    void publishBinarySensor() {
        char buf[32];
        if (getBinarySensorLogicalState()) {
            strcpy(buf, "ON");
        } else {
            strcpy(buf, "OFF");
        }
        pSched->publish(name + "/sensor/" + topicName, buf);
        pSched->publish(name + "/binary_sensor/" + topicName, buf);
    }

    void loop() {
        if (bActive) {
            if (timeDiff(lastPollMs, millis()) >= pollRateMs) {
                bool hasChanged = false;
                lastPollMs = millis();
                bool oldLogicalState = logicalState;
                bool st = getBinarySensorLogicalState();
                if (st != oldLogicalState) {
                    hasChanged = true;
                }
                if (hasChanged) publishBinarySensor();
            }
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/sensor/" + topicName + "/get" || topic == name + "/binary_sensor/" + topicName + "/get") {
            publishBinarySensor();
        }
        if (topic == name + "/sensor/physical/" + topicName + "/get" || topic == name + "/binary_sensor/physical/" + topicName + "/get") {
            publishBinarySensor();
        }
    }
};  // rainAD

}  // namespace ustd
