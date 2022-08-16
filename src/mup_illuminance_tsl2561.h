// mup_illuminance_tsl2561.h
#pragma once

#ifndef tiny_twi_h
#include <Wire.h>
#else
typedef TinyWire TwoWire;
#endif

#include "scheduler.h"
#include "sensors.h"
#include "helpers/mup_i2c_registers.h"

namespace ustd {

// clang-format off
/*! \brief mupplet-sensor luminance with TSL2561

The mup_illuminance_tsl2561 mupplet measures illuminance using the TSL2561 sensor.

This mupplet is a fully asynchronous state-machine with no delay()s, so it never blocks.

#### Messages sent by illuminance_tsl2561 mupplet:

messages are prefixed by `omu/<hostname>`:

| topic | message body | comment |
| ----- | ------------ | ------- |
| `<mupplet-name>/sensor/illuminance` | illumance in lux | Float value encoded as string, sent periodically as available |
| `<mupplet-name>/sensor/unitilluminance` | normalized illumance 0.0(dark)..1.0(max light)  | Float value encoded as string, sent periodically as available |
| `<mupplet-name>/sensor/lightch0` | raw sensor value photo diode ch0 visible light  | Float value encoded as string, sent periodically as available |
| `<mupplet-name>/sensor/irch1` | raw sensor value photo diode ch1 IR light  | Float value encoded as string, sent periodically as available |
| `<mupplet-name>/sensor/mode` | `FAST`, `MEDIUM`, or `LONGTERM` | Integration time for sensor values, external, additional integration |

#### Messages received by illuminance_tsl2561 mupplet:

Need to be prefixed by `<hostname>/`:

| topic | message body | comment |
| ----- | ------------ | ------- |
| `<mupplet-name>/sensor/illuminance/get` | - | Causes current value to be sent. |
| `<mupplet-name>/sensor/unitilluminance/get` | - | Causes current value to be sent. |
| `<mupplet-name>/sensor/lightch0/get` | - | Causes current value to be sent. |
| `<mupplet-name>/sensor/irch1/get` | - | Causes current value to be sent. |
| `<mupplet-name>/sensor/mode/get` | - | Returns filterMode: `FAST`, `MEDIUM`, or `LONGTERM` |
| `<mupplet-name>/sensor/mode/set` | `FAST`, `MEDIUM`, or `LONGTERM` | Set external additional filter values |

#### Sample code

For a complete examples see the `muwerk/examples` project.

```cpp
#include "ustd_platform.h"
#include "scheduler.h"
#include "net.h"
#include "mqtt.h"

#include "mup_illuminance_tsl2561.h"

void appLoop();

ustd::Scheduler sched(10, 16, 32);
ustd::Net net(LED_BUILTIN);
ustd::Mqtt mqtt;

ustd::IlluminanceTSL2561101 TSL("myTSL2561");

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
    TSL.begin(&sched);

    sched.subscribe(tID, "myTSL2561/sensor/illuminance", sensorUpdates);
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
class IlluminanceTSL2561 {
  private:
    String TSL2561_VERSION = "0.1.0";
    Scheduler *pSched;
    TwoWire *pWire;
    I2CRegisters *pI2C;

    int tID;
    String name;

  public:
    enum TSLSensorState { UNAVAILABLE,
                          IDLE,
                          MEASUREMENT_WAIT,
                          WAIT_NEXT_MEASUREMENT };

    /*! Hardware accuracy modes of TSL2561, while the sensor can have different pressure- and temperature oversampling, we use same for both temp and press. */
    enum TSLSampleMode {
        ULTRA_LOW_POWER = 1,       ///< 1 samples, pressure resolution 16bit / 2.62 Pa, rec temperature oversampling: x1
        LOW_POWER = 2,             ///< 2 samples, pressure resolution 17bit / 1.31 Pa, rec temperature oversampling: x1
        STANDARD = 3,              ///< 4 samples, pressure resolution 18bit / 0.66 Pa, rec temperature oversampling: x1
        HIGH_RESOLUTION = 4,       ///< 8 samples, pressure resolution 19bit / 0.33 Pa, rec temperature oversampling: x1
        ULTRA_HIGH_RESOLUTION = 5  ///< 16 samples, pressure resolution 20bit / 0.16 Pa, rec temperature oversampling: x2
    };
    TSLSensorState sensorState;
    unsigned long basePollRateUs = 50000;  // 50ms;
    uint32_t pollRateMs = 2000;            // 2000ms;
    uint32_t lastPollMs = 0;
    enum FilterMode { FAST,
                      MEDIUM,
                      LONGTERM };
    String firmwareVersion;
    FilterMode filterMode;
    uint8_t i2cAddress;

    double illuminanceValue, unitIlluminanceValue, lightCh0Value, IRCh1Value;
    double unitIlluminanceSensitivity;
    ustd::sensorprocessor illuminanceSensor = ustd::sensorprocessor(4, 600, 0.005);
    ustd::sensorprocessor unitIlluminanceSensor = ustd::sensorprocessor(4, 600, 0.005);
    ustd::sensorprocessor lightCh0Sensor = ustd::sensorprocessor(4, 600, 0.005);
    ustd::sensorprocessor IRCh1Sensor = ustd::sensorprocessor(4, 600, 0.005);
    bool bActive = false;

    IlluminanceTSL2561(String name, FilterMode filterMode = FilterMode::FAST, uint8_t i2cAddress = 0x39)
        : name(name), filterMode(filterMode), i2cAddress(i2cAddress) {
        /*! Instantiate an TSL sensor mupplet
        @param name Name used for pub/sub messages
        @param filterMode FAST, MEDIUM or LONGTERM filtering of sensor values
        @param i2cAddress Should be 0x29, 0x39, 0x49, for TSL2561, depending address selector (three state) config.
        */
        sensorState = TSLSensorState::UNAVAILABLE;
        unitIlluminanceSensitivity = 0.2;
        pI2C = nullptr;
        setFilterMode(filterMode, true);
    }

    ~IlluminanceTSL2561() {
        if (pI2C) {
            delete pI2C;
        }
    }

    double getIlluminance() {
        /*! Get Illuminance
        @return illuminance (lux)
        */
        return illuminanceValue;
    }

    double getUnitIlluminance() {
        /*! Get normalized Illuminance
        @return illuminance [0(dark)..1(full light)]
        */
        return unitIlluminanceValue;
    }

    double getUnitIlluminanceSensitivity() {
        /*! Get normalized Illuminance sensitivity
        @return illuminance sensitivity [0(dark)..1(full light)]
        */
        return unitIlluminanceSensitivity;
    }

    void setUnitIlluminanceSensitivity(double sensitivity) {
        /*! Set normalized Illuminance sensitivity
        @param sensitivity illuminance sensitivity [0.001(no sensitiviy)..0.2(default)..(higher sensitiviy)]
        */
        if (sensitivity <= 0.001) sensitivity = 0.2;
        unitIlluminanceSensitivity = sensitivity;
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

        pI2C->lastError = pI2C->checkAddress(i2cAddress);
        if (pI2C->lastError == I2CRegisters::I2CError::OK) {
            uint8_t id, rev;
            if (TSLSensorGetRevID(&id, &rev)) {
                if (id == 5 || id == 1) {  // TSL2561 id should be either 5 (reality) or 1 (datasheet)
                    if (TSLSensorPower(true)) {
                        sensorState = TSLSensorState::IDLE;
                        bActive = true;
#ifdef USE_SERIAL_DBG
                        Serial.println("TSL2561: Powered on, revision:  " + String(rev));
#endif
                    } else {
#ifdef USE_SERIAL_DBG
                        Serial.println("TSL2561: Power on failed");
#endif
                        pI2C->lastError = I2CRegisters::I2CError::I2C_WRITE_ERR_OTHER;
                    }
                } else {
#ifdef USE_SERIAL_DBG
                    Serial.println("TSL2561: Bad sensor ID: " + String(id) + ", expected: 1, revision:  " + String(rev));
#endif
                    pI2C->lastError = I2CRegisters::I2CError::I2C_WRONG_HARDWARE_AT_ADDRESS;
                }
            } else {
#ifdef USE_SERIAL_DBG
                Serial.println("TSL2561: Failed to get sensor ID, wrong hardware?");
#endif
                pI2C->lastError = I2CRegisters::I2CError::I2C_WRONG_HARDWARE_AT_ADDRESS;
            }
        } else {
#ifdef USE_SERIAL_DBG
            Serial.println("TSL2561: Failed to check I2C address, wrong address?");
#endif
            pI2C->lastError = I2CRegisters::I2CError::I2C_DEVICE_NOT_AT_ADDRESS;
        }
    }

    void setFilterMode(FilterMode mode, bool silent = false) {
        switch (mode) {
        case FAST:
            filterMode = FAST;
            illuminanceSensor.smoothInterval = 1;
            illuminanceSensor.pollTimeSec = 2;
            illuminanceSensor.eps = 0.05;
            illuminanceSensor.reset();
            unitIlluminanceSensor.smoothInterval = 1;
            unitIlluminanceSensor.pollTimeSec = 2;
            unitIlluminanceSensor.eps = 0.1;
            unitIlluminanceSensor.reset();
            lightCh0Sensor.smoothInterval = 1;
            lightCh0Sensor.pollTimeSec = 2;
            lightCh0Sensor.eps = 0.1;
            lightCh0Sensor.reset();
            IRCh1Sensor.smoothInterval = 1;
            IRCh1Sensor.pollTimeSec = 2;
            IRCh1Sensor.eps = 0.1;
            IRCh1Sensor.reset();
            break;
        case MEDIUM:
            filterMode = MEDIUM;
            illuminanceSensor.smoothInterval = 4;
            illuminanceSensor.pollTimeSec = 30;
            illuminanceSensor.eps = 0.1;
            illuminanceSensor.reset();
            unitIlluminanceSensor.smoothInterval = 4;
            unitIlluminanceSensor.pollTimeSec = 30;
            unitIlluminanceSensor.eps = 0.5;
            unitIlluminanceSensor.reset();
            lightCh0Sensor.smoothInterval = 4;
            lightCh0Sensor.pollTimeSec = 30;
            lightCh0Sensor.eps = 0.5;
            lightCh0Sensor.reset();
            IRCh1Sensor.smoothInterval = 4;
            IRCh1Sensor.pollTimeSec = 30;
            IRCh1Sensor.eps = 0.5;
            IRCh1Sensor.reset();
            break;
        case LONGTERM:
        default:
            filterMode = LONGTERM;
            illuminanceSensor.smoothInterval = 10;
            illuminanceSensor.pollTimeSec = 600;
            illuminanceSensor.eps = 0.1;
            illuminanceSensor.reset();
            unitIlluminanceSensor.smoothInterval = 50;
            unitIlluminanceSensor.pollTimeSec = 600;
            unitIlluminanceSensor.eps = 0.5;
            unitIlluminanceSensor.reset();
            lightCh0Sensor.smoothInterval = 50;
            lightCh0Sensor.pollTimeSec = 600;
            lightCh0Sensor.eps = 0.5;
            lightCh0Sensor.reset();
            IRCh1Sensor.smoothInterval = 50;
            IRCh1Sensor.pollTimeSec = 600;
            IRCh1Sensor.eps = 0.5;
            IRCh1Sensor.reset();
            break;
        }
        if (!silent)
            publishFilterMode();
    }

  private:
    void publishLightCh0() {
        char buf[32];
        sprintf(buf, "%6.3f", lightCh0Value);
        pSched->publish(name + "/sensor/lightch0", buf);
    }

    void publishIRCh1() {
        char buf[32];
        sprintf(buf, "%6.3f", IRCh1Value);
        pSched->publish(name + "/sensor/irch1", buf);
    }

    void publishIlluminance() {
        char buf[32];
        sprintf(buf, "%6.3f", illuminanceValue);
        pSched->publish(name + "/sensor/illuminance", buf);
    }

    void publishUnitIlluminance() {
        char buf[32];
        sprintf(buf, "%6.3f", unitIlluminanceValue);
        pSched->publish(name + "/sensor/unitilluminance", buf);
    }

    void publishUnitIlluminanceSensitivity() {
        char buf[32];
        sprintf(buf, "%6.3f", unitIlluminanceSensitivity);
        pSched->publish(name + "/sensor/unitilluminancesensitivity", buf);
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

    bool TSLSensorPower(bool powerOn) {
        if (powerOn) {
            if (!pI2C->writeRegisterByte(0x80, 0x03, true)) return false;
        } else {
            if (!pI2C->writeRegisterByte(0x80, 0x00, true)) return false;
        }
        return true;
    }

    bool TSLSensorGetRevID(uint8_t *pId, uint8_t *pRev) {
        /*! Get TSL2561 ID and Revision
         * @param pId Pointer to ID byte, should be 1 for TSL2561
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

    double calculateLux(uint16_t ch0, uint16_t ch1) {
        /*! Calculate Lux from CH0 and CH1
         * @param ch0 CH0 value
         * @param ch1 CH1 value
         * @return Lux value
         */
        if (ch0 == 0) return 0;
        if (ch0 > 65000 || ch1 > 65000) return 10000.0;  // overflow of sensor.
        double lux;
        double ratio = (double)ch1 / (double)ch0;
        if (ratio <= 0.5) {
            lux = 0.0304 * (double)ch0 - 0.062 * (double)ch0 * pow(ratio, 1.4);
        } else if (ratio <= 0.61) {
            lux = 0.0224 * (double)ch0 - 0.031 * (double)ch1;
        } else if (ratio <= 0.80) {
            lux = 0.0128 * (double)ch0 - 0.0153 * (double)ch1;
        } else if (ratio <= 1.30) {
            lux = 0.00146 * (double)ch0 - 0.00112 * (double)ch1;
        } else {
            lux = 0;
        }
        return lux;
    }

    bool readTSLSensorMeasurement(uint8_t reg, double *pMeas) {
        uint16_t data;
        *pMeas = 0.0;
        if (!pI2C->readRegisterWordLE(reg, &data, true)) {
#ifdef USE_SERIAL_DBG
            Serial.print("Failed to read TSL2561 at address 0x");
            Serial.print(i2cAddress, HEX);
            Serial.print(" data: ");
            Serial.print(data, HEX);
            Serial.print(" lasterr: ");
            Serial.println(pI2C->lastError, HEX);
#endif
            return false;
        } else {
            *pMeas = (double)data;
#ifdef USE_SERIAL_DBG
            Serial.println("TSL2561 Measurement: " + String(data));
#endif
            return true;
        }
    }

    bool readTSLSensor(double *pIlluminanceCh0, double *pIlluminanceCh1,
                       double *pLux, double *pUnitIlluminance) {
        *pIlluminanceCh0 = 0.0;
        *pIlluminanceCh1 = 0.0;
        *pLux = 0.0;
        *pUnitIlluminance = 0.0;
        if (!readTSLSensorMeasurement(0xac, pIlluminanceCh0)) return false;
        if (!readTSLSensorMeasurement(0xae, pIlluminanceCh1)) return false;
        *pLux = calculateLux(*pIlluminanceCh0, *pIlluminanceCh1);
        double ui;
        if (*pLux > 1.0) {
            ui = log(*pLux) * unitIlluminanceSensitivity;
        } else {
            ui = 0.0;
        }
        if (ui < 0.0) ui = 0.0;
        if (ui > 1.0) ui = 1.0;
        *pUnitIlluminance = ui;
        return true;
    }

    void loop() {
        double illuminanceVal, unitIlluminanceVal, lightCh0Val, IRCh1Val;
        if (timeDiff(lastPollMs, millis()) > pollRateMs) {
            lastPollMs = millis();
            if (bActive) {
                if (readTSLSensor(&lightCh0Val, &IRCh1Val, &illuminanceVal, &unitIlluminanceVal)) {
                    if (lightCh0Sensor.filter(&lightCh0Val)) {
                        lightCh0Value = lightCh0Val;
                        publishLightCh0();
                    }
                    if (IRCh1Sensor.filter(&IRCh1Val)) {
                        IRCh1Value = IRCh1Val;
                        publishIRCh1();
                    }
                    if (illuminanceSensor.filter(&illuminanceVal)) {
                        illuminanceValue = illuminanceVal;
                        publishIlluminance();
                    }
                    if (unitIlluminanceSensor.filter(&unitIlluminanceVal)) {
                        unitIlluminanceValue = unitIlluminanceVal;
                        publishUnitIlluminance();
                    }
                }
            }
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/sensor/illuminance/get") {
            publishIlluminance();
        } else if (topic == name + "/sensor/unitilluminance/get") {
            publishUnitIlluminance();
        } else if (topic == name + "/sensor/lightch0/get") {
            publishLightCh0();
        } else if (topic == name + "/sensor/irch1/get") {
            publishIRCh1();
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
        } else if (topic == name + "/sensor/unitilluminancesensitivity/set") {
            setUnitIlluminanceSensitivity(msg.toFloat());
        } else if (topic == name + "/sensor/unitilluminancesensitivity/get") {
            publishUnitIlluminanceSensitivity();
        }
    }

};  // IlluminanceTSL2561

}  // namespace ustd
