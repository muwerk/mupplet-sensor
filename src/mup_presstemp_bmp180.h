// mup_presstemp_bmp180.h
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
    /*! \brief mupplet-sensor temperature and pressure with Bosch BMP180

    The mup_presstemp_bmp180 mupplet measures temperature and pressure using a or BMP180 sensor. It should
    also work with the outdated BMP085.

    This mupplet is a fully asynchronous state-machine with no delay()s, so it never blocks.

    #### Messages sent by presstemp_bmp180 mupplet:

    messages are prefixed by `omu/<hostname>`:

    | topic | message body | comment |
    | ----- | ------------ | ------- |
    | `<mupplet-name>/sensor/temperature` | temperature in degree celsius | Float value encoded as string, sent periodically as available |
    | `<mupplet-name>/sensor/pressure` | pressure in hPA for current altitude | Float value encoded as string, sent periodically as available |
    | `<mupplet-name>/sensor/pressureNN` | pressure in hPA adjusted for sea level (requires setReferenceAltitude() to be called) | Float value encoded as string, sent periodically as available |
    | `<mupplet-name>/sensor/calibrationdata` | a string with values of all internal calibration variables | descriptive string |
    | `<mupplet-name>/sensor/referencealtitude` | altitude above sea level as set with setReferenceAltitude() | Float value encoded as string |
    | `<mupplet-name>/sensor/relativealtitude` | current altitude in meters | Current altitude in comparison to the set reference in meters, requires referencealtitude/set and relativealtitude/set msgs being sent. |
    | `<mupplet-name>/sensor/deltaaltitude` | current altitude in meters | Current  altitude delta in meters, requirements as with relativealtitude |
    | `<mupplet-name>/sensor/oversampling` | `ULTRA_LOW_POWER`, `STANDARD`, `HIGH_RESOLUTION`, `ULTRA_HIGH_RESOLUTION` | Internal sensor oversampling mode (sensor hardware) |
    | `<mupplet-name>/sensor/mode` | `FAST`, `MEDIUM`, or `LONGTERM` | Integration time for sensor values, external, additional integration |

    #### Messages received by presstemp_bmp180 mupplet:

    Need to be prefixed by `<hostname>/`:

    | topic | message body | comment |
    | ----- | ------------ | ------- |
    | `<mupplet-name>/sensor/temperature/get` | - | Causes current value to be sent. |
    | `<mupplet-name>/sensor/pressure/get` | - | Causes current value to be sent. |
    | `<mupplet-name>/sensor/pressureNN/get` | - | Causes current value to be sent. |
    | `<mupplet-name>/sensor/referencealtitude/get` | - | Causes current value to be sent. |
    | `<mupplet-name>/sensor/referencealtitude/set` | float encoded as string of current altitude in meters | Once the reference altitude is set, pressureNN values can be calculated. |
    | `<mupplet-name>/sensor/relativealtitude/set` | - | Save current pressureNN values as reference, start generating relative altitude-change messages, requires reference altitude to be set |
    | `<mupplet-name>/sensor/relativealtitude/get` | - | Get current altitude in comparison to the set reference and an altitude delta in meters |
    | `<mupplet-name>/sensor/calibrationdata/get` | - | Causes current values to be sent. |
    | `<mupplet-name>/sensor/oversampling/get` | - | Returns samplemode: `ULTRA_LOW_POWER`, `STANDARD`, `HIGH_RESOLUTION`, `ULTRA_HIGH_RESOLUTION` |
    | `<mupplet-name>/sensor/oversampling/set` | `ULTRA_LOW_POWER`, `STANDARD`, `HIGH_RESOLUTION`, `ULTRA_HIGH_RESOLUTION` | Set internal sensor oversampling mode |
    | `<mupplet-name>/sensor/mode/get` | - | Returns filterMode: `FAST`, `MEDIUM`, or `LONGTERM` |
    | `<mupplet-name>/sensor/mode/set` | `FAST`, `MEDIUM`, or `LONGTERM` | Set external additional filter values |

    #### Sample code

    For a complete examples see the `muwerk/examples` project.

    ```cpp
    #include "ustd_platform.h"
    #include "scheduler.h"
    #include "net.h"
    #include "mqtt.h"

    #include "mup_presstemp_bmp180.h"

    void appLoop();

    ustd::Scheduler sched(10, 16, 32);
    ustd::Net net(LED_BUILTIN);
    ustd::Mqtt mqtt;

    ustd::PressTempBMP180 bmp("myBMP180");

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
        bmp.setReferenceAltitude(518.0); // 518m above NN, now we also receive PressureNN values for sea level.
        bmp.begin(&sched, ustd::PressTempBMP180::BMPSampleMode::ULTRA_HIGH_RESOLUTION);

        sched.subscribe(tID, "myBMP180/sensor/temperature", sensorUpdates);
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
class PressTempBMP180 {
  private:
    String BMP180_VERSION = "0.1.0";
    Scheduler *pSched;
    TwoWire *pWire;
    I2CRegisters *pI2C;
    int tID;
    String name;
    double temperatureValue, pressureValue, pressureNNValue;
    unsigned long stateMachineClock;
    int32_t rawTemperature;
    double calibratedTemperature;
    int32_t rawPressure;
    double calibratedPressure;
    double baseRelativeNNPressure;
    bool relativeAltitudeStarted;
    bool captureRelative = false;
    unsigned long basePollRateUs = 500000L;
    uint32_t pollRateMs;
    uint32_t lastPollMs;

    // BMP Sensor calibration data
    int16_t CD_AC1, CD_AC2, CD_AC3, CD_B1, CD_B2, CD_MB, CD_MC, CD_MD;
    uint16_t CD_AC4, CD_AC5, CD_AC6;

  public:
    enum BMPSensorState { UNAVAILABLE,
                          IDLE,
                          TEMPERATURE_WAIT,
                          PRESSURE_WAIT,
                          WAIT_NEXT_MEASUREMENT };

    /*! Hardware accuracy modes of BMP180 */
    enum BMPSampleMode {
        ULTRA_LOW_POWER = 0,       ///< 1 samples, 4.5ms conversion time, 3uA current at 1 sample/sec, 0.06 RMS noise typ. [hPA]
        STANDARD = 1,              ///< 2 samples, 7.5ms conversion time, 5uA current at 1 sample/sec, 0.05 RMS noise typ. [hPA]
        HIGH_RESOLUTION = 2,       ///< 4 samples, 13.5ms conversion time, 7uA current at 1 sample/sec, 0.04 RMS noise typ. [hPA]
        ULTRA_HIGH_RESOLUTION = 3  ///< 8 samples, 25.5ms conversion time, 12uA current at 1 sample/sec, 0.03 RMS noise typ. [hPA]
    };
    BMPSensorState sensorState;
    unsigned long errs = 0;
    unsigned long oks = 0;
    uint16_t oversampleMode = 2;  // 0..3, see BMPSampleMode.
    enum FilterMode { FAST,
                      MEDIUM,
                      LONGTERM };
#define MUP_BMP_INVALID_ALTITUDE -1000000.0
    double referenceAltitudeMeters;
    FilterMode filterMode;
    uint8_t i2cAddress;
    ustd::sensorprocessor temperatureSensor = ustd::sensorprocessor(4, 600, 0.005);
    ustd::sensorprocessor pressureSensor = ustd::sensorprocessor(4, 600, 0.005);
    bool bActive = false;

    PressTempBMP180(String name, FilterMode filterMode = FilterMode::MEDIUM, uint8_t i2cAddress = 0x77)
        : name(name), filterMode(filterMode), i2cAddress(i2cAddress) {
        /*! Instantiate an BMP sensor mupplet
        @param name Name used for pub/sub messages
        @param filterMode FAST, MEDIUM or LONGTERM filtering of sensor values
        @param i2cAddress Should always be 0x77 for BMP180, cannot be changed.
        */
        sensorState = BMPSensorState::UNAVAILABLE;
        referenceAltitudeMeters = MUP_BMP_INVALID_ALTITUDE;
        relativeAltitudeStarted = false;
        captureRelative = false;
        pI2C = nullptr;
        lastPollMs = 0;
        setFilterMode(filterMode, true);
    }

    ~PressTempBMP180() {
        if (pI2C) {
            delete pI2C;
        }
    }

    void setReferenceAltitude(double _referenceAltitudeMeters) {
        /*! Set the altitude at current sensor location as reference
        @param _referenceAltitudeMeters The current altitude above sea level in meters
        */
        referenceAltitudeMeters = _referenceAltitudeMeters;
    }

    void startRelativeAltitude() {
        /*! Store current pressure for a reference altitude to start relative altitude measurement

        Once a reference altitude is defined (see \ref setReferenceAltitude()), measurement of relative
        altitude can be started by calling this function.
        */
        if (referenceAltitudeMeters != MUP_BMP_INVALID_ALTITUDE) {
            captureRelative = true;
        }
    }

    double getTemperature() {
        /*! Get current temperature
        @return Temperature (in degrees celsius)
        */
        return temperatureValue;
    }

    double getPressure() {
        /*! Get current pressure
        @return Pressure (in hPa)
        */
        return pressureValue;
    }

    double getPressureNN(double _pressure) {
        /*! Get current pressure at sea level (NN)

        Once a reference altitude is defined (see \ref setReferenceAltitude()), pressure
        at sea level NN can be calculated with this function.

        @return Pressure at sea level (in hPa)
        */
        if (referenceAltitudeMeters != MUP_BMP_INVALID_ALTITUDE) {
            double prNN = _pressure / pow((1.0 - (referenceAltitudeMeters / 44330.0)), 5.255);
            return prNN;
        }
        return 0.0;
    }

    void setSampleMode(BMPSampleMode _sampleMode) {
        oversampleMode = (uint16_t)_sampleMode;
    }

    bool initBmpSensorConstants() {
        if (!pI2C->readRegisterWord(0xaa, (uint16_t *)&CD_AC1)) return false;
        if (!pI2C->readRegisterWord(0xac, (uint16_t *)&CD_AC2)) return false;
        if (!pI2C->readRegisterWord(0xae, (uint16_t *)&CD_AC3)) return false;
        if (!pI2C->readRegisterWord(0xb0, (uint16_t *)&CD_AC4)) return false;
        if (!pI2C->readRegisterWord(0xb2, (uint16_t *)&CD_AC5)) return false;
        if (!pI2C->readRegisterWord(0xb4, (uint16_t *)&CD_AC6)) return false;
        if (!pI2C->readRegisterWord(0xb6, (uint16_t *)&CD_B1)) return false;
        if (!pI2C->readRegisterWord(0xb8, (uint16_t *)&CD_B2)) return false;
        if (!pI2C->readRegisterWord(0xba, (uint16_t *)&CD_MB)) return false;
        if (!pI2C->readRegisterWord(0xbc, (uint16_t *)&CD_MC)) return false;
        if (!pI2C->readRegisterWord(0xbe, (uint16_t *)&CD_MD)) return false;
        return true;
    }

    void begin(Scheduler *_pSched, TwoWire *_pWire = &Wire, uint32_t _pollRateMs = 2000, BMPSampleMode _sampleMode = BMPSampleMode::STANDARD) {
        pSched = _pSched;
        setSampleMode(_sampleMode);
        pWire = _pWire;
        pollRateMs = _pollRateMs;
        uint8_t data;

        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, basePollRateUs);  // 500us

        auto fnall = [=](String topic, String msg, String originator) {
            this->subsMsg(topic, msg, originator);
        };
        pSched->subscribe(tID, name + "/sensor/#", fnall);

        pI2C = new I2CRegisters(pWire, i2cAddress);

        pI2C->lastError = pI2C->checkAddress(i2cAddress);
        if (pI2C->lastError == I2CRegisters::I2CError::OK) {
            if (!pI2C->readRegisterByte(0xd0, &data)) {  // 0xd0: chip-id register
                bActive = false;
            } else {
                if (data == 0x55) {  // 0x55: signature of BMP180
                    if (!initBmpSensorConstants()) {
                        pI2C->lastError = I2CRegisters::I2CError::I2C_HW_ERROR;
                        bActive = false;
                    } else {
                        sensorState = BMPSensorState::IDLE;
                        bActive = true;
                    }
                } else {

                    pI2C->lastError = I2CRegisters::I2CError::I2C_WRONG_HARDWARE_AT_ADDRESS;
                    bActive = false;
                }
            }
        } else {
            bActive = false;
        }
    }

    void setFilterMode(FilterMode mode, bool silent = false) {
        switch (mode) {
        case FAST:
            filterMode = FAST;
            temperatureSensor.smoothInterval = 1;
            temperatureSensor.pollTimeSec = 2;
            temperatureSensor.eps = 0.05;
            temperatureSensor.reset();
            pressureSensor.smoothInterval = 1;
            pressureSensor.pollTimeSec = 2;
            pressureSensor.eps = 0.1;
            pressureSensor.reset();
            break;
        case MEDIUM:
            filterMode = MEDIUM;
            temperatureSensor.smoothInterval = 4;
            temperatureSensor.pollTimeSec = 30;
            temperatureSensor.eps = 0.1;
            temperatureSensor.reset();
            pressureSensor.smoothInterval = 4;
            pressureSensor.pollTimeSec = 30;
            pressureSensor.eps = 0.5;
            pressureSensor.reset();
            break;
        case LONGTERM:
        default:
            filterMode = LONGTERM;
            temperatureSensor.smoothInterval = 10;
            temperatureSensor.pollTimeSec = 600;
            temperatureSensor.eps = 0.1;
            temperatureSensor.reset();
            pressureSensor.smoothInterval = 50;
            pressureSensor.pollTimeSec = 600;
            pressureSensor.eps = 0.5;
            pressureSensor.reset();
            break;
        }
        if (!silent)
            publishFilterMode();
    }

  private:
    void publishTemperature() {
        char buf[32];
        sprintf(buf, "%6.2f", temperatureValue);
        pSched->publish(name + "/sensor/temperature", buf);
    }

    void publishPressure() {
        char buf[32];
        sprintf(buf, "%7.2f", pressureValue);
        pSched->publish(name + "/sensor/pressure", buf);
        if (referenceAltitudeMeters != MUP_BMP_INVALID_ALTITUDE) {
            sprintf(buf, "%7.2f", pressureNNValue);
            pSched->publish(name + "/sensor/pressureNN", buf);
        }
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

    void publishOversampling() {
        switch ((BMPSampleMode)oversampleMode) {
        case BMPSampleMode::ULTRA_LOW_POWER:
            pSched->publish(name + "/sensor/oversampling", "ULTRA_LOW_POWER");
            break;
        case BMPSampleMode::STANDARD:
            pSched->publish(name + "/sensor/oversampling", "STANDARD");
            break;
        case BMPSampleMode::HIGH_RESOLUTION:
            pSched->publish(name + "/sensor/oversampling", "HIGH_RESOLUTION");
            break;
        case BMPSampleMode::ULTRA_HIGH_RESOLUTION:
            pSched->publish(name + "/sensor/oversampling", "ULTRA_HIGH_RESOLUTION");
            break;
        default:
            pSched->publish(name + "/sensor/oversampling", "INVALID");
            break;
        }
    }

    void publishCalibrationData() {
        char msg[256];
        sprintf(msg, "AC1=%d, AC2=%d, AC3=%d, AC4=%u, AC5=%u, AC6=%u, B1=%d, B2=%d, MB=%d, MC=%d, MD=%d", CD_AC1, CD_AC2, CD_AC3, CD_AC4, CD_AC5, CD_AC6, CD_B1, CD_B2, CD_MB, CD_MC, CD_MD);
        pSched->publish("sensor/calibrationdata", msg);
    }

    void publishReferenceAltitude() {
        if (referenceAltitudeMeters != MUP_BMP_INVALID_ALTITUDE) {
            char buf[32];
            sprintf(buf, "%7.2f", referenceAltitudeMeters);
            pSched->publish(name + "/sensor/referencealtitude", buf);
        } else {
            pSched->publish(name + "/sensor/referencealtitude", "unknown");
        }
    }

    void publishRelativeAltitude() {
        char buf[32];
        if (relativeAltitudeStarted) {
            double ralt = 44330.0 * (1.0 - pow(pressureValue / baseRelativeNNPressure, 1. / 5.255));
            sprintf(buf, "%7.2f", ralt);
            pSched->publish(name + "/sensor/relativealtitude", buf);
            double dalt = ralt - referenceAltitudeMeters;
            sprintf(buf, "%7.2f", dalt);
            pSched->publish(name + "/sensor/deltaaltitude", buf);
        }
    }

    bool sensorStateMachine() {
        bool newData = false;
        uint16_t rt;
        uint32_t rp;
        unsigned long convTimeOversampling[4] = {4500, 7500, 13500, 25500};  // sample time dependent on oversample-mode for pressure.
        switch (sensorState) {
        case BMPSensorState::UNAVAILABLE:
            break;
        case BMPSensorState::IDLE:
            if (!pI2C->writeRegisterByte(0xf4, 0x2e)) {
                ++errs;
                sensorState = BMPSensorState::WAIT_NEXT_MEASUREMENT;
                stateMachineClock = micros();
                break;
            } else {  // Start temperature read
                sensorState = BMPSensorState::TEMPERATURE_WAIT;
                stateMachineClock = micros();
            }
            break;
        case BMPSensorState::TEMPERATURE_WAIT:
            if (timeDiff(stateMachineClock, micros()) > 4500) {  // 4.5ms for temp meas.
                rt = 0;
                if (pI2C->readRegisterWord(0xf6, &rt)) {
                    rawTemperature = rt;
                    uint8_t cmd = 0x34 | (oversampleMode << 6);
                    if (!pI2C->writeRegisterByte(0xf4, cmd)) {
                        ++errs;
                        sensorState = BMPSensorState::WAIT_NEXT_MEASUREMENT;
                        stateMachineClock = micros();
                    } else {
                        sensorState = BMPSensorState::PRESSURE_WAIT;
                        stateMachineClock = micros();
                    }
                } else {
                    ++errs;
                    sensorState = BMPSensorState::WAIT_NEXT_MEASUREMENT;
                    stateMachineClock = micros();
                }
            }
            break;
        case BMPSensorState::PRESSURE_WAIT:
            if (timeDiff(stateMachineClock, micros()) > convTimeOversampling[oversampleMode]) {  // Oversamp. dep. for press meas.
                rp = 0;
                if (pI2C->readRegisterTripple(0xf6, &rp)) {
                    rawPressure = rp >> (8 - oversampleMode);
                    ++oks;
                    newData = true;
                    sensorState = BMPSensorState::WAIT_NEXT_MEASUREMENT;
                    stateMachineClock = micros();
                } else {
                    ++errs;
                    sensorState = BMPSensorState::WAIT_NEXT_MEASUREMENT;
                    stateMachineClock = micros();
                }
            }
            break;
        case BMPSensorState::WAIT_NEXT_MEASUREMENT:
            if (timeDiff(lastPollMs, millis()) > pollRateMs) {
                sensorState = BMPSensorState::IDLE;  // Start next cycle.
                lastPollMs = millis();
            }
            break;
        }
        return newData;
    }

    bool calibrateRawData() {
        // Temperature
        int32_t x1 = (((uint32_t)rawTemperature - (uint32_t)CD_AC6) * (uint32_t)CD_AC5) >> 15;
        int32_t x2 = ((int32_t)CD_MC << 11) / (x1 + (int32_t)CD_MD);
        int32_t b5 = x1 + x2;
        calibratedTemperature = ((double)b5 + 8.0) / 160.0;
        // Pressure
        int32_t b6 = b5 - (int32_t)4000;
        x1 = ((int32_t)CD_B2 * ((b6 * b6) >> 12)) >> 11;
        x2 = ((int32_t)CD_AC2 * b6) >> 11;
        int32_t x3 = x1 + x2;
        int32_t b3 = ((((int32_t)CD_AC1 * 4L + x3) << oversampleMode) + 2L) / 4;

        // char msg[128];
        // sprintf(msg,"x1=%d,x2=%d, x3=%d,b3=%d,rt=%d,rp=%d",x1,x2,x3,b3,rawTemperature,rawPressure);
        // pSched->publish("myBMP180/sensor/debug1",msg);

        x1 = ((int32_t)CD_AC3 * b6) >> 13;
        x2 = ((int32_t)CD_B1 * ((b6 * b6) >> 12)) >> 16;
        x3 = ((x1 + x2) + 2) >> 2;
        uint32_t b4 = ((uint32_t)CD_AC4 * (uint32_t)(x3 + (uint32_t)32768)) >> 15;
        uint32_t b7 = ((uint32_t)rawPressure - b3) * ((uint32_t)50000 >> oversampleMode);

        // sprintf(msg,"x1=%d,x2=%d, x3=%d,b4=%u,b7=%u",x1,x2,x3,b4,b7);
        // pSched->publish("myBMP180/sensor/debug2",msg);

        int32_t p;
        if (b7 < (uint32_t)0x80000000) {
            p = (b7 * 2) / b4;
        } else {
            p = (b7 / b4) * 2;
        }
        x1 = (p >> 8) * (p >> 8);
        x1 = (x1 * (int32_t)3038) >> 16;
        x2 = (((int32_t)-7357) * p) >> 16;

        int32_t cp = p + ((x1 + x2 + (int32_t)3791) >> 4);

        // sprintf(msg,"x1=%d,x2=%d,p=%d,cp=%d",x1,x2,p,cp);
        // pSched->publish("myBMP180/sensor/debug3",msg);

        calibratedPressure = cp / 100.0;  // hPa;
        return true;
    }

    void loop() {
        double tempVal, pressVal;
        if (bActive) {
            if (!sensorStateMachine()) return;  // no new data
            calibrateRawData();
            tempVal = (double)calibratedTemperature;
            pressVal = (double)calibratedPressure;
            if (temperatureSensor.filter(&tempVal)) {
                temperatureValue = tempVal;
                publishTemperature();
            }
            if (pressureSensor.filter(&pressVal)) {
                pressureValue = pressVal;
                if (referenceAltitudeMeters != MUP_BMP_INVALID_ALTITUDE) {
                    pressureNNValue = getPressureNN(pressureValue);
                    if (captureRelative) {
                        baseRelativeNNPressure = pressureNNValue;
                        relativeAltitudeStarted = true;
                        captureRelative = false;
                    }
                }
                publishPressure();
                if (relativeAltitudeStarted) publishRelativeAltitude();
            }
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/sensor/temperature/get") {
            publishTemperature();
        }
        if (topic == name + "/sensor/pressure/get") {
            publishPressure();
        }
        if (topic == name + "/sensor/mode/get") {
            publishFilterMode();
        }
        if (topic == name + "/sensor/calibrationdata/get") {
            publishCalibrationData();
        }
        if (topic == name + "/sensor/referencealtitude/get") {
            publishReferenceAltitude();
        }
        if (topic == name + "/sensor/relativealtitude/get") {
            publishRelativeAltitude();
        }
        if (topic == name + "/sensor/relativealtitude/set") {
            startRelativeAltitude();
        }
        if (topic == name + "/sensor/oversampling/get") {
            publishOversampling();
        }
        if (topic == name + "/sensor/referencealtitude/set") {
            double alt = atof(msg.c_str());
            setReferenceAltitude(alt);
        }
        if (topic == name + "/sensor/mode/set") {
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
        if (topic == name + "/sensor/oversampling/set") {
            if (msg == "ULTRA_LOW_POWER") {
                setSampleMode(BMPSampleMode::ULTRA_LOW_POWER);
            } else {
                if (msg == "STANDARD") {
                    setSampleMode(BMPSampleMode::STANDARD);
                } else {
                    if (msg == "HIGH_RESOLUTION") {
                        setSampleMode(BMPSampleMode::HIGH_RESOLUTION);
                    } else {
                        setSampleMode(BMPSampleMode::ULTRA_HIGH_RESOLUTION);
                    }
                }
            }
        }
    }
};  // PressTempBMP

}  // namespace ustd
