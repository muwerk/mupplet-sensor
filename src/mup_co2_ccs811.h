// mup_co2_tsl2561.h
#pragma once

#ifndef tiny_twi_h
#include <Wire.h>
#else
typedef TinyWire TwoWire;
#endif

#include "scheduler.h"
#include "sensors.h"
#include "helper/mup_i2c_registers.h"

namespace ustd {

// clang-format off
/*! \brief mupplet-sensor co2 and voc with CCS811

The mup_co2_ccs811 mupplet measures co2 using the CCS811 sensor.

Precision and range can be modified using the following parameters:

- `FilterMode` modifies the software filter for `FAST` (no software averages), `MEDIUM`, or `LONGTERM` (more software averages)
   
This mupplet is a fully asynchronous state-machine with no delay()s, so it never blocks.

#### Messages sent by co2_ccs811 mupplet:

messages are prefixed by `omu/<hostname>`:

| topic | message body | comment |
| ----- | ------------ | ------- |
| `<mupplet-name>/sensor/co2` | co2 in ppm | Float value encoded as string, sent periodically as available |
| `<mupplet-name>/sensor/mode` | `FAST`, `MEDIUM`, or `LONGTERM` | Software filter values, external, additional software integration, in addition to hardware integration on sensor |

#### Messages received by co2_ccs811 mupplet:

Need to be prefixed by `<hostname>/`:

| topic | message body | comment |
| ----- | ------------ | ------- |
| `<mupplet-name>/sensor/co2/get` | - | Causes current value to be sent. |
| `<mupplet-name>/sensor/mode/get` | - | Returns software filterMode: `FAST`, `MEDIUM`, or `LONGTERM` |
| `<mupplet-name>/sensor/mode/set` | `FAST`, `MEDIUM`, or `LONGTERM` | Set softwaare filter values, a 2nd stage filter, additional to hardware integration on sensor |

#### Sample code

For a complete examples see the `muwerk/examples` project.

```cpp
#include "ustd_platform.h"
#include "scheduler.h"
#include "net.h"
#include "mqtt.h"

#include "mup_co2_ccs811.h"

void appLoop();

ustd::Scheduler sched(10, 16, 32);
ustd::Net net(LED_BUILTIN);
ustd::Mqtt mqtt;

ustd::CO2CCS811 CCS("myCCS811");

void sensorUpdates(String topic, String msg, String originator) {
    // data is in topic, msg
}

void setup() {
#ifdef USE_SERIAL_DBG
    Serial.begin(115200);
#endif  // USE_SERIAL_DBG
    net.begin(&sched);
    mqtt.begin(&sched);
    ota.begin(&sched);
    int tID = sched.add(appLoop, "main", 1000000);

    // sensors start measuring pressure and temperature
    CCS.begin(&sched);

    sched.subscribe(tID, "myCCS811/sensor/co2", sensorUpdates);
}

void appLoop() {
}

// Never add code to this loop, use appLoop() instead.
void loop() {
    sched.loop();
}
```
*/

// clang-format on
class CO2CCS811 {
  private:
    String CCS811_VERSION = "0.1.0";
    Scheduler *pSched;
    TwoWire *pWire;
    I2CRegisters *pI2C;

    int tID;
    String name;

  public:
    unsigned long basePollRateUs = 50000;  // 50ms;
    uint32_t pollRateMs = 2000;            // 2000ms;
    uint32_t lastPollMs = 0;
    enum FilterMode { FAST,
                      MEDIUM,
                      LONGTERM };
    String firmwareVersion;
    FilterMode filterMode;
    uint8_t i2cAddress;
    String temperatureTopic;
    String humidityTopic;
    double temperature;
    double humidity;

    double co2Value, vocValue;
    ustd::sensorprocessor co2Sensor = ustd::sensorprocessor(4, 600, 0.005);
    ustd::sensorprocessor vocSensor = ustd::sensorprocessor(4, 600, 0.005);
    bool bActive = false;

    CO2CCS811(String name, FilterMode filterMode = FilterMode::FAST, uint8_t i2cAddress = 0x5a,
              String temperatureTopic = "", String humidityTopic = "")
        : name(name), filterMode(filterMode), i2cAddress(i2cAddress), temperatureTopic(temperatureTopic), humidityTopic(humidityTopic) {
        /*! Instantiate an CCS sensor mupplet
        @param name Name used for pub/sub messages
        @param filterMode FAST, MEDIUM or LONGTERM filtering of sensor values
        @param i2cAddress Should be 0x5a, 0x5b, for CCS811, depending address selector config.
        @param temperatureTopic Topic that receives temperature values which are used for compensation (optional)
        @param humidityTopic Topic that receives humidity values which are used for compensation (optional)
        */
        pI2C = nullptr;
        temperature = -99.0;
        humidity = -99.0;
        setFilterMode(filterMode, true);
    }

    ~CO2CCS811() {
        if (pI2C) {
            delete pI2C;
        }
    }

    double getCO2() {
        /*! Get CO2
        @return co2 (ppm)
        */
        return co2Value;
    }

    double getVOC() {
        /*! Get VOC
        @return voc (ppb)
        */
        return vocValue;
    }

    void begin(Scheduler *_pSched, TwoWire *_pWire = &Wire, uint32_t _pollRateMs = 2000) {
        pSched = _pSched;
        pWire = _pWire;
        pollRateMs = _pollRateMs;

        pWire->begin();  // required!
        pI2C = new I2CRegisters(pWire, i2cAddress);

        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, basePollRateUs);  // 1s

        auto fnall = [=](String topic, String msg, String originator) {
            this->subsMsg(topic, msg, originator);
        };
        pSched->subscribe(tID, name + "/sensor/#", fnall);
        if (temperatureTopic.length() > 0) {
            pSched->subscribe(tID, temperatureTopic, fnall);
        }
        if (humidityTopic.length() > 0) {
            pSched->subscribe(tID, humidityTopic, fnall);
        }

        pI2C->lastError = pI2C->checkAddress(i2cAddress);
        if (pI2C->lastError == I2CRegisters::I2CError::OK) {
            uint8_t id, rev;
            if (CCSSensorGetRevID(&id, &rev)) {
                if (id == 5 || id == 1) {  // CCS811 id should be either 5 (reality) or 1 (datasheet)
                    if (CCSSensorPower(true)) {
                        bActive = true;
#ifdef USE_SERIAL_DBG
                        Serial.println("CCS811: Powered on, revision:  " + String(rev));
#endif
                        if (CCSSensorGainIntegrationSet()) {
#ifdef USE_SERIAL_DBG
                            Serial.println("CCS811: Integration- and Gain-Mode set.");
#endif
                        } else {
#ifdef USE_SERIAL_DBG
                            Serial.println("CCS811: Integration- and Gain-Mode set ERROR.");
#endif
                            pI2C->lastError = I2CRegisters::I2C_WRITE_ERR_OTHER;
                        }
                    } else {
#ifdef USE_SERIAL_DBG
                        Serial.println("CCS811: Power on failed");
#endif
                        pI2C->lastError = I2CRegisters::I2CError::I2C_WRITE_ERR_OTHER;
                    }
                } else {
#ifdef USE_SERIAL_DBG
                    Serial.println("CCS811: Bad sensor ID: " + String(id) + ", expected: 1, revision:  " + String(rev));
#endif
                    pI2C->lastError = I2CRegisters::I2CError::I2C_WRONG_HARDWARE_AT_ADDRESS;
                }
            } else {
#ifdef USE_SERIAL_DBG
                Serial.println("CCS811: Failed to get sensor ID, wrong hardware?");
#endif
                pI2C->lastError = I2CRegisters::I2CError::I2C_WRONG_HARDWARE_AT_ADDRESS;
            }
        } else {
#ifdef USE_SERIAL_DBG
            Serial.println("CCS811: Failed to check I2C address, wrong address?");
#endif
            pI2C->lastError = I2CRegisters::I2CError::I2C_DEVICE_NOT_AT_ADDRESS;
        }
    }

    void setFilterMode(FilterMode mode, bool silent = false) {
        switch (mode) {
        case FAST:
            filterMode = FAST;
            co2Sensor.update(1, 2, 0.05);  // requires muwerk 0.6.3 API
            unitCO2Sensor.update(1, 2, 0.1);
            lightCh0Sensor.update(1, 2, 0.1);
            IRCh1Sensor.update(1, 2, 0.1);
            break;
        case MEDIUM:
            filterMode = MEDIUM;
            co2Sensor.update(4, 30, 0.1);
            unitCO2Sensor.update(4, 30, 0.5);
            lightCh0Sensor.update(4, 30, 0.5);
            IRCh1Sensor.update(4, 30, 0.5);
            break;
        case LONGTERM:
        default:
            filterMode = LONGTERM;
            co2Sensor.update(10, 600, 0.1);
            unitCO2Sensor.update(50, 600, 0.5);
            lightCh0Sensor.update(50, 600, 0.5);
            IRCh1Sensor.update(50, 600, 0.5);
            break;
        }
        if (!silent)
            publishFilterMode();
    }

  private:
    void publishCO2() {
        char buf[32];
        sprintf(buf, "%6.3f", co2Value);
        pSched->publish(name + "/sensor/co2", buf);
    }

    void publishVOC() {
        char buf[32];
        sprintf(buf, "%6.3f", vocValue);
        pSched->publish(name + "/sensor/voc", buf);
    }

    void publishError(String errMsg) {
        pSched->publish(name + "/sensor/error", errMsg);
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

    bool CCSSensorPower(bool powerOn) {
        if (powerOn) {
            if (!pI2C->writeRegisterByte(0x80, 0x03, true)) return false;
        } else {
            if (!pI2C->writeRegisterByte(0x80, 0x00, true)) return false;
        }
        return true;
    }

    bool CCSSensorGetRevID(uint8_t *pId, uint8_t *pRev) {
        /*! Get CCS811 ID and Revision
         * @param pId Pointer to ID byte, should be 1 for CCS811
         * @param pRev Pointer to Revision byte
         * @return True if successful
         */
        *pId = 0;
        *pRev = 0;
        uint8_t idbyte = 0;
        if (!pI2C->readRegisterByte(0x8a, &idbyte)) return false;
        *pId = idbyte >> 4;
        *pRev = idbyte & 0x0f;
        return true;
    }

    bool readCCSSensorMeasurement(uint8_t reg, double *pMeas) {
        uint16_t data;
        *pMeas = 0.0;
        // if (!pI2C->readRegisterWordLE(reg, &data, true)) {
        return false;
        //} else {
        //    *pMeas = (double)data;
        //    return true;
        //}
    }

    bool readCCSSensor(double *pco2Val, double *pvocVal) {
        // if (!readCCSSensorMeasurement()) return false;
        // if (!readCCSSensorMeasurement()) return false;
        return false;
    }

    void loop() {
        double co2Val, vocVal;
        if (timeDiff(lastPollMs, millis()) > pollRateMs) {
            lastPollMs = millis();
            if (bActive) {
                if (readCCSSensor(&co2Val, &vocVal)) {
                    if (co2Sensor.filter(&co2Val)) {
                        co2Value = co2Val;
                        publishCO2();
                    }
                    if (vocSensor.filter(&vocVal)) {
                        vocValue = bocVal;
                        publishUnitVOC();
                    }
                }
            }
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == temperatureTopic) {
            temperature = msg.toFloat();
        } else if (topic == humidityTopic) {
            humidity = msg.toFloat();
        } else if (topic == name + "/sensor/co2/get") {
            publishCO2();
        } else if (topic == name + "/sensor/voc/get") {
            publishVOC();
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
    }

};  // CO2CCS811

}  // namespace ustd
