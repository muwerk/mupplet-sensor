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
    enum InitState {
        INIT_STATE_DISABLED = 0,
        INIT_STATE_START,
        INIT_STATE_WAIT_RESET,
        INIT_STATE_WAIT_APP_START,
        INIT_STATE_APP_STARTED,
        INIT_STATE_APP_RUNNING,
        INIT_STATE_ERROR_WAIT
    };
    InitState initState = INIT_STATE_DISABLED;
    uint32_t stateMachineTicks = 0;
    uint32_t stateMachineErrors = 0;
    uint32_t stateMachineMaxErrors = 10;

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
        initState = INIT_STATE_START;
    }

    void setFilterMode(FilterMode mode, bool silent = false) {
        switch (mode) {
        case FAST:
            filterMode = FAST;
            co2Sensor.update(1, 2, 0.05);  // requires muwerk 0.6.3 API
            vocSensor.update(1, 2, 0.1);
            break;
        case MEDIUM:
            filterMode = MEDIUM;
            co2Sensor.update(4, 30, 0.1);
            vocSensor.update(4, 30, 0.5);
            break;
        case LONGTERM:
        default:
            filterMode = LONGTERM;
            co2Sensor.update(10, 600, 0.1);
            vocSensor.update(50, 600, 0.5);
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

    bool CCSSensorMode(uint8_t mode = 0x10) {  // measure every sec. by default.
        /*! set continuous measurment mode as default (1sec intervals), no IRQs*/
        if (!pI2C->writeRegisterByte(0x01, mode)) {
            pI2C->lastError = I2CRegisters::I2CError::I2C_WRITE_ERR_OTHER;
#ifdef USE_SERIAL_DBG
            Serial.println("CCS811: Failed to set mode");
#endif
            return false;
        }
        return true;
    }

    bool CCSSensorGetStatus(uint8_t *pStatus, uint8_t *pError, bool appTest = false) {
        /*! Bits in status reply:
        Bit 0: 0: no error, 1: error, check pError
        */
        *pError = 0;
        if (!pI2C->readRegisterByte(0x00, pStatus)) {
            pI2C->lastError = I2CRegisters::I2CError::I2C_READ_ERR_OTHER;
#ifdef USE_SERIAL_DBG
            Serial.println("CCS811: Failed to get status");
#endif
            stateMachineRegisterError();
            return false;
        }
        if (*pStatus & 0x01) {
            if (!pI2C->readRegisterByte(0xE0, pError)) {
                pI2C->lastError = I2CRegisters::I2CError::I2C_READ_ERR_OTHER;
#ifdef USE_SERIAL_DBG
                Serial.println("CCS811: GetStatus: Failed to get error");
#endif
                stateMachineRegisterError();
                return false;
            }
#ifdef USE_SERIAL_DBG
            Serial.println("CCS811: GetStatus: Sensor-Error: " + String(*pError));
#endif
        }
        if ((*pStatus & 0x01) || (!(*pStatus & (uint8_t)0x80) && !appTest)) {
            pI2C->lastError = I2CRegisters::I2CError::I2C_HW_ERROR;
#ifdef USE_SERIAL_DBG
            Serial.println("CCS811: GetStatus: HW error ");
            Serial.println(*pStatus);
#endif
            stateMachineRegisterError();
            return false;
        }
        return true;
    }

    bool CCSSensorSWReset() {
        /*! Software-Reset the sensor */
        const uint8_t reset[4] = {0x11, 0xE5, 0x72, 0x8A};
        if (!pI2C->writeRegisterNBytes(0xFF, reset, 4)) {
            pI2C->lastError = I2CRegisters::I2CError::I2C_WRITE_ERR_OTHER;
            return false;
        }
        return true;
    }

    bool CCSSensorDataReady() {
        uint8_t status = 0;
        uint8_t error = 0;
        CCSSensorGetStatus(&status, &error);  //) {
        //    return false;
        //}
        if (status & 0x08) {
            return true;
        }
#ifdef USE_SERIAL_DBG
        Serial.println("CCS811: Data not ready");
#endif
        return false;
    }

    bool CCSSensorGetRevID(uint8_t *pId, uint8_t *pRev, uint16_t *pBootVer, uint16_t *pAppVer) {
        /*! Get CCS811 ID and Revision
         * @param pId Pointer to ID byte, should be 1 for CCS811
         * @param pRev Pointer to Revision byte
         * @return True if successful
         */
        *pId = 0;
        *pRev = 0;
        if (!pI2C->readRegisterByte(0x20, pId)) return false;
        if (!pI2C->readRegisterByte(0x21, pRev)) return false;
        if (!pI2C->readRegisterWord(0x23, pBootVer)) return false;
        if (!pI2C->readRegisterWord(0x24, pAppVer)) return false;
        return true;
    }

    bool CCSSensorAppStart() {
        /*! Start application mode of CCS811 */
        // const uint8_t appStart[4] = {0x00, 0x00, 0x00, 0x00};
        if (!pI2C->writeRegisterNBytes(0xF4, nullptr, 0)) {
            pI2C->lastError = I2CRegisters::I2CError::I2C_WRITE_ERR_OTHER;
            return false;
        }
        return true;
    }

    void stateMachineRegisterError() {
        ++stateMachineErrors;
        if (stateMachineErrors > stateMachineMaxErrors) {
            stateMachineErrors = 0;
            initState = INIT_STATE_ERROR_WAIT;
        }
    }

    bool readCCSSensor(double *pco2Val, double *pvocVal) {
        *pco2Val = 0;
        *pvocVal = 0;
        uint8_t pData[8];  // up to 8 data bytes can be read
        const uint8_t readLen = 8;

        if (!pI2C->readRegisterNBytes(0x02, pData, readLen)) {
            pI2C->lastError = I2CRegisters::I2CError::I2C_READ_ERR_OTHER;
#ifdef USE_SERIAL_DBG
            Serial.println("CCS811: Failed to read sensor data");
#endif
            stateMachineRegisterError();
            return false;
        }
        *pco2Val = (double)((pData[0] << 8) | pData[1]);
        *pvocVal = (double)((pData[2] << 8) | pData[3]);
        stateMachineErrors = 0;
        // XXX: status, error_id, raw_data ffr.
        return true;
    }

    bool CCSSensorEnvData(double temperature, double humidity) {
        /*! Set temperature and humidity compensation data
         * @param temperature Temperature in degrees C, e.g. 20.3
         * @param humidity Relative humidity in %RH, e.g. 51.0
         * @return True if successful
         */
        uint16_t temp = (uint16_t)((temperature + 25.0) * 512);  // offset by 25C
        uint16_t hum = (uint16_t)(humidity * 512);
        uint8_t data[4] = {(uint8_t)(hum >> 8), (uint8_t)(hum & 0xFF), (uint8_t)(temp >> 8), (uint8_t)(temp & 0xFF)};
        if (!pI2C->writeRegisterNBytes(0x05, data, 4)) {
            pI2C->lastError = I2CRegisters::I2CError::I2C_WRITE_ERR_OTHER;
            return false;
        }
        return true;
    }

    void initStateMachine() {
        switch (initState) {
            uint8_t id, rev;
            uint16_t fw_boot, fw_app;
            uint8_t status, error;
        case INIT_STATE_DISABLED:
            return;
        case INIT_STATE_START:
            // #ifdef __ESP__
            //            pWire->setClockStretchLimit(500);
            // #endif
            CCSSensorSWReset();
            stateMachineTicks = millis();
            stateMachineErrors = 0;
            initState = INIT_STATE_WAIT_RESET;
#ifdef USE_SERIAL_DBG
            Serial.println("CCS811: Reset");
#endif
            return;
        case INIT_STATE_WAIT_RESET:
            if (timeDiff(stateMachineTicks, millis()) > 100) {
                if (!CCSSensorGetRevID(&id, &rev, &fw_boot, &fw_app)) {
#ifdef USE_SERIAL_DBG
                    Serial.println("CCS811: Failed to get sensor ID, wrong hardware?");
#endif
                    pI2C->lastError = I2CRegisters::I2CError::I2C_WRONG_HARDWARE_AT_ADDRESS;
                    stateMachineTicks = millis();
                    initState = INIT_STATE_ERROR_WAIT;
                    return;
                }
                if (id != 0x81) {
#ifdef USE_SERIAL_DBG
                    Serial.println("CCS811: Bad sensor ID: " + String(id) + ", expected: 1, revision:  " + String(rev));
#endif
                    pI2C->lastError = I2CRegisters::I2CError::I2C_WRONG_HARDWARE_AT_ADDRESS;
                    stateMachineTicks = millis();
                    initState = INIT_STATE_ERROR_WAIT;
                    return;
                }
                status = 0;
                error = 0;
                if (!CCSSensorGetStatus(&status, &error, true)) {
#ifdef USE_SERIAL_DBG
                    Serial.println("CCS811: sensor status: ");
                    Serial.println(status);
                    Serial.println("CCS811: sensor error: ");
                    Serial.println(error);
#endif
                    stateMachineTicks = millis();
                    initState = INIT_STATE_ERROR_WAIT;
                    return;
                }
                if (!(status & 0x80)) {
#ifdef USE_SERIAL_DBG
                    Serial.println("CCS811: Sensor not in application mode, starting app-mode:");
#endif

                    if (!CCSSensorAppStart()) {
#ifdef USE_SERIAL_DBG
                        Serial.println("CCS811: Failed to start sensor in application mode");
#endif
                        stateMachineTicks = millis();
                        initState = INIT_STATE_ERROR_WAIT;
                        return;
                    } else {
                        stateMachineTicks = millis();
                        initState = INIT_STATE_WAIT_APP_START;
#ifdef USE_SERIAL_DBG
                        Serial.println("CCS811: Sensor started in application mode");
#endif
                        return;
                    }
                } else {
#ifdef USE_SERIAL_DBG
                    Serial.println("CCS811: Sensor already in application mode");
#endif
                    initState = INIT_STATE_APP_STARTED;
                    return;
                }
                return;
            case INIT_STATE_WAIT_APP_START:
                if (timeDiff(stateMachineTicks, millis()) > 500) {
#ifdef USE_SERIAL_DBG
                    Serial.println("CCS811: Sensor started in application mode");
#endif
                    initState = INIT_STATE_APP_STARTED;
                    return;
                }
                return;
            case INIT_STATE_APP_STARTED:
                if (CCSSensorMode()) {
                    bActive = true;
#ifdef USE_SERIAL_DBG
                    Serial.println("CCS811: Powered on continously, HW revision:  " + String(rev) + ", FW boot: " +
                                   String(fw_boot) + ", FW app: " + String(fw_app));
#endif
                } else {
#ifdef USE_SERIAL_DBG
                    Serial.println("CCS811: Continous mode on setting failed");
#endif
                    pI2C->lastError = I2CRegisters::I2CError::I2C_WRITE_ERR_OTHER;
                    stateMachineTicks = millis();
                    initState = INIT_STATE_ERROR_WAIT;
                    return;
                }
                initState = INIT_STATE_APP_RUNNING;
                stateMachineErrors = 0;
                CCSSensorGetStatus(&status, &error, false);  // trigger calculation
                return;
            case INIT_STATE_APP_RUNNING:
                stateMachineTicks = millis();
                return;
            case INIT_STATE_ERROR_WAIT:
                if (timeDiff(stateMachineTicks, millis()) > 5000) {
                    initState = INIT_STATE_START;
                }
                return;
            }
        }
    }

    void loop() {
        double co2Val, vocVal;
        initStateMachine();
        if (timeDiff(lastPollMs, millis()) > pollRateMs) {
            lastPollMs = millis();
            if (initState == INIT_STATE_APP_RUNNING) {
                if (CCSSensorDataReady()) {
                    if (readCCSSensor(&co2Val, &vocVal)) {
                        if (co2Sensor.filter(&co2Val)) {
                            co2Value = co2Val;
                            publishCO2();
                        }
                        if (vocSensor.filter(&vocVal)) {
                            vocValue = vocVal;
                            publishVOC();
                        }
                    }
                }
            }
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == temperatureTopic) {
            temperature = msg.toFloat();
            if (humidity != -99.0) {
                CCSSensorEnvData(temperature, humidity);
            }
        } else if (topic == humidityTopic) {
            humidity = msg.toFloat();
            if (temperature != -99.0) {
                CCSSensorEnvData(temperature, humidity);
            }
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
