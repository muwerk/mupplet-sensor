// mup_illuminance_tsl2561.h
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
/*! \brief mupplet-sensor luminance with TSL2561

The mup_illuminance_tsl2561 mupplet measures illuminance using the TSL2561 sensor.

Precision and range can be modified using the following parameters:

- `FilterMode` modifies the software filter for `FAST` (no software averages), `MEDIUM`, or `LONGTERM` (more software averages)
- `IntegrationMode` modifies the sensor's internal integration time from `FAST` (13ms), `MEDIUM` (101ms) to `LONGTERM` (402ms).
- `GainMode` selects the sensor hardware gain amplification between `LOW` (1x) and `HIGH` (16x)
- `unitilluminancesensitivity` is a software amplification factor that shifts the range of calculated `unitilluminance` values using a model that is perceived by humans as linear.
   (The value for `illuminance` is in lux, which is not perceived as linear.)
   
This mupplet is a fully asynchronous state-machine with no delay()s, so it never blocks.

#### Messages sent by illuminance_tsl2561 mupplet:

messages are prefixed by `omu/<hostname>`:

| topic | message body | comment |
| ----- | ------------ | ------- |
| `<mupplet-name>/sensor/illuminance` | illumance in lux | Float value encoded as string, sent periodically as available |
| `<mupplet-name>/sensor/unitilluminance` | normalized illumance 0.0(dark)..1.0(max light)  | Float value encoded as string, sent periodically as available |
| `<mupplet-name>/sensor/lightch0` | raw sensor value photo diode ch0 visible light  | Float value encoded as string, sent periodically as available |
| `<mupplet-name>/sensor/irch1` | raw sensor value photo diode ch1 IR light  | Float value encoded as string, sent periodically as available |
| `<mupplet-name>/sensor/mode` | `FAST`, `MEDIUM`, or `LONGTERM` | Software filter values, external, additional software integration, in addition to hardware integration on sensor |
| `<mupplet-name>/sensor/integration` | `FAST`, `MEDIUM`, or `LONGTERM` | Integration on sensor hardware: FAST: 13.7 ms, MEDIUM: 101ms, LONG: 402ms (default) |
| `<mupplet-name>/sensor/gain` / `LOW` or `HIGH` | Low is 1x gain, high is 16x gain |
| `<mupplet-name>/sensor/unitilluminancesensitivity` | <sensitivity> | Sensitivity-factor for unitilluminance, default 0.2. |

#### Messages received by illuminance_tsl2561 mupplet:

Need to be prefixed by `<hostname>/`:

| topic | message body | comment |
| ----- | ------------ | ------- |
| `<mupplet-name>/sensor/illuminance/get` | - | Causes current value to be sent. |
| `<mupplet-name>/sensor/unitilluminance/get` | - | Causes current value to be sent. |
| `<mupplet-name>/sensor/lightch0/get` | - | Causes current value to be sent. |
| `<mupplet-name>/sensor/irch1/get` | - | Causes current value to be sent. |
| `<mupplet-name>/sensor/mode/get` | - | Returns software filterMode: `FAST`, `MEDIUM`, or `LONGTERM` |
| `<mupplet-name>/sensor/integration/get` | - | Returns hardware integration: `FAST` (13.7ms), `MEDIUM` (101ms), or `LONGTERM` (402ms) |
| `<mupplet-name>/sensor/mode/set` | `FAST`, `MEDIUM`, or `LONGTERM` | Set softwaare filter values, a 2nd stage filter, additional to hardware integration on sensor |
| `<mupplet-name>/sensor/integration/set` | `FAST`, `MEDIUM`, or `LONGTERM` | Set hardware integration values values, default is LONGTERM, 402ms |
| `<mupplet-name>/sensor/gain/set` | `LOW` or `HIGH` | Set 1x (low, default) or 16x (high) gain |
| `<mupplet-name>/sensor/gain/get` | - | Causes `gain` message to be sent. |
| `<mupplet-name>/sensor/unitilluminancesensitivity/set` | <sensitivity> | Default 0.2. If decreased, lower values for unitilluminance are generated, and vice versa. Uses logarithmic model in order to match human perception. |
| `<mupplet-name>/sensor/unitilluminancesensitivity/get` | - | Get current value |

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
    unsigned long basePollRateUs = 50000;  // 50ms;
    uint32_t pollRateMs = 2000;            // 2000ms;
    uint32_t lastPollMs = 0;
    enum FilterMode { FAST,
                      MEDIUM,
                      LONGTERM };
    enum IntegrationMode { FAST13ms,
                           MEDIUM101ms,
                           LONGTERM402ms };
    enum GainMode { LOW1x,
                    HIGH16x };
    String firmwareVersion;
    FilterMode filterMode;
    IntegrationMode integrationMode;
    GainMode gainMode;
    uint8_t i2cAddress;

    double illuminanceValue, unitIlluminanceValue, lightCh0Value, IRCh1Value;
    double unitIlluminanceSensitivity;
    ustd::sensorprocessor illuminanceSensor = ustd::sensorprocessor(4, 600, 0.005);
    ustd::sensorprocessor unitIlluminanceSensor = ustd::sensorprocessor(4, 600, 0.005);
    ustd::sensorprocessor lightCh0Sensor = ustd::sensorprocessor(4, 600, 0.005);
    ustd::sensorprocessor IRCh1Sensor = ustd::sensorprocessor(4, 600, 0.005);
    bool bActive = false;

    IlluminanceTSL2561(String name, FilterMode filterMode = FilterMode::FAST, IntegrationMode integrationMode = IntegrationMode::LONGTERM402ms,
                       GainMode gainMode = GainMode::LOW1x, uint8_t i2cAddress = 0x39)
        : name(name), filterMode(filterMode), integrationMode(integrationMode), gainMode(gainMode), i2cAddress(i2cAddress) {
        /*! Instantiate an TSL sensor mupplet
        @param name Name used for pub/sub messages
        @param filterMode FAST, MEDIUM or LONGTERM filtering of sensor values
        @param i2cAddress Should be 0x29, 0x39, 0x49, for TSL2561, depending address selector (three state) config.
        */
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

        Lower values (<0.2) generate low `unitilluminance` values, higher values amplify.

        @return illuminance sensitivity
        */
        return unitIlluminanceSensitivity;
    }

    void setUnitIlluminanceSensitivity(double sensitivity) {
        /*! Set normalized Illuminance sensitivity

        This can be used to modify the range use by unitilluminance, which is always [0..1]. The sensitivity
        reduces (lower sensitivity) or amplifies (higher sensitivity) the values generated for `unitilluminance`.
        A logarithmic model is used to match human perception.

        Default is 0.2 on startup.

        @param sensitivity illuminance sensitivity [0.001(no sensitivity)..0.2(default)..(higher sensitivity)]
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
                        bActive = true;
#ifdef USE_SERIAL_DBG
                        Serial.println("TSL2561: Powered on, revision:  " + String(rev));
#endif
                        if (TSLSensorGainIntegrationSet()) {
#ifdef USE_SERIAL_DBG
                            Serial.println("TSL2561: Integration- and Gain-Mode set.");
#endif
                        } else {
#ifdef USE_SERIAL_DBG
                            Serial.println("TSL2561: Integration- and Gain-Mode set ERROR.");
#endif
                            pI2C->lastError = I2CRegisters::I2C_WRITE_ERR_OTHER;
                        }
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
            illuminanceSensor.update(1, 2, 0.05);  // requires muwerk 0.6.3 API
            unitIlluminanceSensor.update(1, 2, 0.1);
            lightCh0Sensor.update(1, 2, 0.1);
            IRCh1Sensor.update(1, 2, 0.1);
            break;
        case MEDIUM:
            filterMode = MEDIUM;
            illuminanceSensor.update(4, 30, 0.1);
            unitIlluminanceSensor.update(4, 30, 0.5);
            lightCh0Sensor.update(4, 30, 0.5);
            IRCh1Sensor.update(4, 30, 0.5);
            break;
        case LONGTERM:
        default:
            filterMode = LONGTERM;
            illuminanceSensor.update(10, 600, 0.1);
            unitIlluminanceSensor.update(50, 600, 0.5);
            lightCh0Sensor.update(50, 600, 0.5);
            IRCh1Sensor.update(50, 600, 0.5);
            break;
        }
        if (!silent)
            publishFilterMode();
    }

    void setGainIntegrationMode(GainMode gMode, IntegrationMode iMode, bool silent = false) {
        gainMode = gMode;
        integrationMode = iMode;
        TSLSensorGainIntegrationSet();
        if (!silent) {
            publishGainMode();
            publishIntegrationMode();
        }
    }

    void setIntegrationMode(IntegrationMode mode, bool silent = false) {
        integrationMode = mode;
        TSLSensorGainIntegrationSet();
        if (!silent)
            publishIntegrationMode();
    }

    void setGainMode(GainMode mode, bool silent = false) {
        gainMode = mode;
        TSLSensorGainIntegrationSet();
        if (!silent)
            publishGainMode();
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

    void publishIntegrationMode() {
        switch (integrationMode) {
        case IntegrationMode::FAST13ms:
            pSched->publish(name + "/sensor/integration", "FAST");
            break;
        case IntegrationMode::MEDIUM101ms:
            pSched->publish(name + "/sensor/integration", "MEDIUM");
            break;
        case IntegrationMode::LONGTERM402ms:
            pSched->publish(name + "/sensor/integration", "LONGTERM");
            break;
        }
    }

    void publishGainMode() {
        switch (gainMode) {
        case GainMode::LOW1x:
            pSched->publish(name + "sensor/gain", "LOW");
            break;
        case GainMode::HIGH16x:
            pSched->publish(name + "sensor/gain", "HIGH");
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

    bool TSLSensorGainIntegrationSet() {
        uint8_t mx = 0;
        switch (gainMode) {
        case GainMode::LOW1x:
            mx = 0x00;
            break;
        case GainMode::HIGH16x:
            mx = 0x10;
        }
        switch (integrationMode) {
        case IntegrationMode::FAST13ms:
            mx |= 0x00;
            break;
        case IntegrationMode::MEDIUM101ms:
            mx |= 0x01;
            break;
        case IntegrationMode::LONGTERM402ms:
            mx |= 0x02;
        }
        return pI2C->writeRegisterByte(0x81, mx, true);
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
        } else if (topic == name + "/sensor/integration/get") {
            publishIntegrationMode();
        } else if (topic == name + "/sensor/integration/set") {
            if (msg == "fast" || msg == "FAST") {
                setIntegrationMode(IntegrationMode::FAST13ms);
            } else {
                if (msg == "medium" || msg == "MEDIUM") {
                    setIntegrationMode(IntegrationMode::MEDIUM101ms);
                } else {
                    setIntegrationMode(IntegrationMode::LONGTERM402ms);
                }
            }
        } else if (topic == name + "/sensor/gain/get") {
            publishGainMode();
        } else if (topic == name + "/sensor/gain/set") {
            if (msg == "low" || msg == "LOW") {
                setGainMode(GainMode::LOW1x);
            } else {
                setGainMode(GainMode::HIGH16x);
            }
        } else if (topic == name + "/sensor/unitilluminancesensitivity/set") {
            setUnitIlluminanceSensitivity(msg.toFloat());
        } else if (topic == name + "/sensor/unitilluminancesensitivity/get") {
            publishUnitIlluminanceSensitivity();
        }
    }

};  // IlluminanceTSL2561

}  // namespace ustd
