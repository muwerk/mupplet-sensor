// mup_magnetic_hmc5883l.h
#pragma once

#ifndef tiny_twi_h
#include <Wire.h>
#else
typedef TinyWire TwoWire;
#endif

#include "scheduler.h"
#include "sensors.h"

#include "helper/mup_i2c_registers.h"

/* Magnetic field sensor overview

Source: https://hackaday.io/project/11865-3d-magnetic-field-scanner/log/39485-magnetometer-sensor-survey
(plus manual addons)

| Part #   | Manufacturer        | Noise (resolution) (μT) | Max. FS (μT) |
| -------- | ------------------- | ----------------------: | -----------: |
| LIS3MDL  | ST Microelectronics |                   0.32  |         1600 |
| AK8963   | AsahiKASEI          |                  (0.15) |         4900 |
| MAG3110  | Freescale/NXP       |                   0.25  |         1000 |
| HMC5883L | Honeywell           |                    0.2  |          800 |
| MLX90393 | Melexis             |                    0.5  |        50000 |
| TLV493D  | Infineon            |                   98.0  |       130000 |

So the HMC5883L is a good choice for a high-resolution measurement of earth's magnetic field.
*/

namespace ustd {

// clang-format off
/*! \brief mupplet-sensor for magnetic field (compass) based on HMC5883L sensor

The mup_mag_hmc5883l mupplet reads the magnetic field from a HMC5883L sensor and sends the values for x, y, z

Warning: HMC5883L is no longer produced, and Arduino sensors sold as 'HMC5883L sensors' are often QMC5883L sensors.
QMC5883L is not register-compatible with HMC5883L, so this mupplet will not work with QMC5883L sensors. 
Use mup_mag_qmc5883l instead, if your sensor is not recognized by this mupplet. Additionally, the i2c addresses
differ: HMC5883L uses 0x1e, QMC5883L uses 0x0d. It does not work to simply change the i2c address in the mupplet,
since the register layout is different, ChipID will give an error (0,0,1) instead of 'H34'.

This mupplet is a fully asynchronous state-machine with no delay()s, so it never blocks.

#### Messages sent by mup_magnetic_hmc5883l mupplet:

messages are prefixed by `omu/<hostname>`:

| topic | message body | comment |
| ----- | ------------ | ------- |
| `<mupplet-name>/sensor/magnetic_field_x` |  | Float value encoded as string, sent periodically as available |
| `<mupplet-name>/sensor/magnetic_field_y` |  | Float value encoded as string, sent periodically as available |
| `<mupplet-name>/sensor/magnetic_field_z` |  | Float value encoded as string, sent periodically as available |
| `<mupplet-name>/sensor/magnetic_field_strength` |  | Float value encoded as string, sent periodically as available |
| `<mupplet-name>/sensor/error` |  | Error message |
| `<mupplet-name>/sensor/mode` | `FAST`, `MEDIUM`, or `LONGTERM` | Filter mode |

#### Messages received by mup_magnetic_hmc5883l mupplet:

Need to be prefixed by `<hostname>/`:

| topic | message body | comment |
| ----- | ------------ | ------- |
| `<mupplet-name>/sensor/magnetic_field/get` | - | Causes current value to be sent. |
| `<mupplet-name>/sensor/mode/get` | - | Returns filterMode: `FAST`, `MEDIUM`, or `LONGTERM` |
| `<mupplet-name>/sensor/mode/set` | `FAST`, `MEDIUM`, or `LONGTERM` | Set external additional filter values |
*/
// clang-format on

class MagneticFieldHMC5883L {
  private:
    String HMC5883L_VERSION = "0.1.0";
    Scheduler *pSched;
    TwoWire *pWire;
    I2CRegisters *pI2C;
    int tID;
    String name;
    double magXValue, magYValue, magZValue;
    double magXRawValue, magYRawValue, magZRawValue;
    unsigned long stateMachineClock;

  public:
    enum HMC5883LSensorState { UNAVAILABLE,
                               IDLE,
                               MEASUREMENT_WAIT,
                               WAIT_NEXT_MEASUREMENT };

    //! Hardware accuracy modes of HMC5883L, while the sensor can have different pressure- and temperature oversampling, we use same for both temp and press.
    enum HMC5883LSamples {
        SAMPLE_AVERAGE_1 = 0x00,  // 1 sample (Default)
        SAMPLE_AVERAGE_2 = 0x20,  // 2 samples
        SAMPLE_AVERAGE_4 = 0x40,  // 4 samples
        SAMPLE_AVERAGE_8 = 0x60   // 8 samples
    };
    enum HMC5883LMeasurementMode {
        MEASUREMENT_MODE_NORMAL = 0x00,         // Normal mode (Default), pos. and neg. pins of resistive load are left floating
        MEASUREMENT_MODE_POSITIVE_BIAS = 0x01,  // Positive bias fox X, Y, Z axes, a positive current is forced across the resistive load
        MEASUREMENT_MODE_NEGATIVE_BIAS = 0x02   // Negative bias for X, Y, Z axes, a negative current is forced across the resistive load
    };
    enum HMC5883LDataOutputRate {
        DATA_OUTPUT_RATE_0_75 = 0x00,  // 0.75Hz
        DATA_OUTPUT_RATE_1_5 = 0x04,   // 1.5Hz
        DATA_OUTPUT_RATE_3 = 0x08,     // 3Hz
        DATA_OUTPUT_RATE_7_5 = 0x0C,   // 7.5Hz
        DATA_OUTPUT_RATE_15 = 0x10,    // 15Hz (Default)
        DATA_OUTPUT_RATE_30 = 0x14,    // 30Hz
        DATA_OUTPUT_RATE_75 = 0x18     // 75Hz
    };
    enum HMC5883LGain {
        GAIN_0_88 = 0x00,  // +/- 0.88 Ga
        GAIN_1_3 = 0x20,   // +/- 1.3 Ga (Default)
        GAIN_1_9 = 0x40,   // +/- 1.9 Ga
        GAIN_2_5 = 0x60,   // +/- 2.5 Ga
        GAIN_4_0 = 0x80,   // +/- 4.0 Ga
        GAIN_4_7 = 0xA0,   // +/- 4.7 Ga
        GAIN_5_6 = 0xC0,   // +/- 5.6 Ga
        GAIN_8_1 = 0xE0    // +/- 8.1 Ga
    };
    enum HMC5883LMode {
        MODE_CONTINUOUS = 0x00,  // Continuous measurement mode
        MODE_SINGLE = 0x01,      // Single measurement mode  ONLY MODE USED BY THIS DRIVER
        MODE_IDLE = 0x02         // Idle mode
    };
    const uint8_t highSpeedI2C = 0x80;
    bool highSpeedI2CEnabled = false;

    HMC5883LSensorState sensorState;
    unsigned long errs = 0;
    unsigned long oks = 0;
    unsigned long basePollRate = 500000L;
    uint32_t pollRateMs = 2000;
    uint32_t lastPollMs = 0;
    HMC5883LSamples oversampleMode = HMC5883LSamples::SAMPLE_AVERAGE_1;
    HMC5883LMeasurementMode measurementMode = HMC5883LMeasurementMode::MEASUREMENT_MODE_NORMAL;
    HMC5883LDataOutputRate dataOutputRate = HMC5883LDataOutputRate::DATA_OUTPUT_RATE_15;
    HMC5883LGain gain = HMC5883LGain::GAIN_1_3;

    enum FilterMode { FAST,
                      MEDIUM,
                      LONGTERM };
    const int MUP_HMC5883L_OVERFLOW = -4096;
    FilterMode filterMode;
    uint8_t i2cAddress;
    ustd::sensorprocessor magXSensor = ustd::sensorprocessor(4, 600, 0.005);
    ustd::sensorprocessor magYSensor = ustd::sensorprocessor(4, 600, 0.005);
    ustd::sensorprocessor magZSensor = ustd::sensorprocessor(4, 600, 0.005);
    bool bActive = false;
    int measurement_delay = 6;  // 6ms for measurement

    MagneticFieldHMC5883L(String name, FilterMode filterMode = FilterMode::MEDIUM, uint8_t i2cAddress = 0x1e, bool highSpeedI2CEnabled = false)
        : name(name), filterMode(filterMode), i2cAddress(i2cAddress), highSpeedI2CEnabled(highSpeedI2CEnabled) {
        //! Instantiate an HMC5883L sensor mupplet
        //! @param name Name used for pub/sub messages
        //! @param filterMode FAST, MEDIUM or LONGTERM filtering of sensor values
        //! @param i2cAddress 0x1e is the default i2c address for the HMC5883L sensor
        //! @param highSpeedI2CEnabled If true, enable high speed I2C mode, 3400kHz, default false
        sensorState = HMC5883LSensorState::UNAVAILABLE;
        pI2C = nullptr;
        setFilterMode(filterMode, true);
    }

    ~MagneticFieldHMC5883L() {
        if (pI2C) {
            delete pI2C;
        }
    }

    double getMagX() {
        //! Get current magnetic field X value
        //! @return Magnetic field X value (in Gauss)
        return magXValue;
    }

    double getMagY() {
        //! Get current magnetic field Y value
        //! @return Magnetic field Y value (in Gauss)

        return magYValue;
    }

    double getMagZ() {
        //! Get current magnetic field Z value
        //! @return Magnetic field Z value (in Gauss)

        return magZValue;
    }

    double getMagneticFieldStrength() {
        //! Get current magnetic field strength
        //! @return Magnetic field strength (in Gauss)

        return sqrt(magXValue * magXValue + magYValue * magYValue + magZValue * magZValue);
    }

    bool setHMC5883LSamples(HMC5883LSamples samples) {
        //! Set HMC5883L sensor oversampling mode
        //! @param samples Number of samples to average (1..8)

        oversampleMode = samples;
        uint8_t reg;
        reg = oversampleMode | measurementMode | dataOutputRate;
        if (!pI2C->writeRegisterByte(0, reg)) {
#ifdef USE_SERIAL_DBG
            Serial.println("Failed to set HMC5883L samples.");
#endif
            return false;
        }
        return true;
    }

    bool setHMC5883LMeasurementMode(HMC5883LMeasurementMode mode) {
        //! Set HMC5883L sensor measurement mode
        //! @param mode Measurement mode

        measurementMode = mode;
        uint8_t reg;
        reg = oversampleMode | measurementMode | dataOutputRate;
        if (!pI2C->writeRegisterByte(0, reg)) {
#ifdef USE_SERIAL_DBG
            Serial.println("Failed to set HMC5883L measurement mode.");
#endif
            return false;
        }
        return true;
    }

    bool setHMC5883LDataOutputRate(HMC5883LDataOutputRate rate) {
        //! Set HMC5883L sensor data output rate
        //! @param rate Data output rate

        dataOutputRate = rate;
        uint8_t reg;
        reg = oversampleMode | measurementMode | dataOutputRate;
        if (!pI2C->writeRegisterByte(0, reg)) {
#ifdef USE_SERIAL_DBG
            Serial.println("Failed to set HMC5883L data output rate.");
#endif
            return false;
        }
        return true;
    }

    bool setHMC5883LGain(HMC5883LGain gain) {
        //! Set HMC5883L sensor gain
        //! @param gain Gain value

        uint8_t reg;
        reg = gain;
        if (!pI2C->writeRegisterByte(0x01, reg)) {
#ifdef USE_SERIAL_DBG
            Serial.println("Failed to set HMC5883L gain.");
#endif
            return false;
        }
        return true;
    }

    bool setHMC5883LMode() {
        if (!setHMC5883LSamples(oversampleMode)) {
            return false;
        }
        if (!setHMC5883LMeasurementMode(measurementMode)) {
            return false;
        }
        if (!setHMC5883LDataOutputRate(dataOutputRate)) {
            return false;
        }
        if (!setHMC5883LGain(gain)) {
            return false;
        }
        return true;
    }

    bool initSingleMeasurement() {
        uint8_t reg;
        reg = HMC5883LMode::MODE_SINGLE;
        if (highSpeedI2CEnabled) {
            reg |= highSpeedI2C;
        }
        if (!pI2C->writeRegisterByte(0x02, reg)) {
#ifdef USE_SERIAL_DBG
            Serial.println("Failed to set HMC5883L mode for single measurement.");
#endif
            return false;
        }
        // after 6ms the data is ready
        return true;
    }

    bool readSingleMeasurement(double *pX, double *pY, double *pZ) {
        uint8_t buf[6];
        if (!pI2C->readRegisterNBytes(0x06, buf, 6)) {
#ifdef USE_SERIAL_DBG
            Serial.println("Failed to read HMC5883L single measurement.");
#endif
            return false;
        }
        int x = (int16_t)(buf[0] << 8 | buf[1]);
        int y = (int16_t)(buf[2] << 8 | buf[3]);
        int z = (int16_t)(buf[4] << 8 | buf[5]);
        if (x == MUP_HMC5883L_OVERFLOW || y == MUP_HMC5883L_OVERFLOW || z == MUP_HMC5883L_OVERFLOW) {
#ifdef USE_SERIAL_DBG
            Serial.println("HMC5883L overflow detected.");
#endif
            return false;
        }
        // XXX Temp compensation
        *pX = (double)x;
        *pY = (double)y;
        *pZ = (double)z;
        return true;
    }

    bool getSensorIdentification() {
        char buf[3];
        if (!pI2C->readRegisterTripple(0x0a, (uint32_t *)buf)) {
#ifdef USE_SERIAL_DBG
            Serial.println("Failed to read sensor identification.");
#endif
            return false;
        }
        if (!strncmp(buf, "34H", 3)) {
#ifdef USE_SERIAL_DBG
            Serial.println("Sensor identification: H43");
#endif
            return true;
        }
#ifdef USE_SERIAL_DBG
        Serial.print("Wrong sensor identification: ");
        for (int i = 0; i < 3; i++) {
            Serial.print(buf[i]);
        }
        Serial.println();
#endif
        return false;
    }

    bool checkDataReady() {
        uint8_t status;
        if (!pI2C->readRegisterByte(0x09, &status)) {
#ifdef USE_SERIAL_DBG
            Serial.println("Failed to read HMC5883L status.");
#endif
            return false;
        } else {
            if (status & 0x01) {
                return true;
            } else {
#ifdef USE_SERIAL_DBG
                Serial.print("HMC5883L data not ready: 0x");
                Serial.println(status, HEX);
#endif
                if (status & 0x02) {
#ifdef USE_SERIAL_DBG
                    Serial.println("HMC5883L LOCK is set");
#endif
                }
                return false;
            }
        }
    }

    void begin(Scheduler *_pSched, TwoWire *_pWire = &Wire, uint32_t _pollRateMs = 2000, HMC5883LSamples _sampleMode = HMC5883LSamples::SAMPLE_AVERAGE_1,
               HMC5883LMeasurementMode _measurementMode = HMC5883LMeasurementMode::MEASUREMENT_MODE_NORMAL,
               HMC5883LDataOutputRate _dataOutputRate = HMC5883LDataOutputRate::DATA_OUTPUT_RATE_0_75, HMC5883LGain _gain = HMC5883LGain::GAIN_1_3) {
        pSched = _pSched;
        pWire = _pWire;
        uint8_t data;

#ifdef SDA_PIN
        pWire->begin(SDA_PIN, SCL_PIN);
#endif

        pollRateMs = _pollRateMs;
        pI2C = new I2CRegisters(pWire, i2cAddress);

        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, basePollRate);  // 500us

        auto fnall = [=](String topic, String msg, String originator) {
            this->subsMsg(topic, msg, originator);
        };
        pSched->subscribe(tID, name + "/sensor/#", fnall);

        pI2C->lastError = pI2C->checkAddress(i2cAddress);
        if (pI2C->lastError == I2CRegisters::I2CError::OK) {
            if (!getSensorIdentification()) {
                bActive = false;
                if (i2cAddress == 0x0d) {
#ifdef USE_SERIAL_DBG
                    Serial.println("You are probably trying to use a QMC5883L sensor with this driver. This driver is not compatible. Use mup_mag_qmc5883l instead.");
#endif
                }
            } else {
                sensorState = HMC5883LSensorState::IDLE;
                oversampleMode = _sampleMode;
                measurementMode = _measurementMode;
                dataOutputRate = _dataOutputRate;
                gain = _gain;
                if (!setHMC5883LMode()) {
#ifdef USE_SERIAL_DBG
                    Serial.println("Failed to set HMC5883L mode, deactivating sensor.");
#endif
                    bActive = false;
                } else {
#ifdef USE_SERIAL_DBG
                    Serial.println("HMC5883L sensor initialized.");
#endif
                    bActive = true;
                }
            }
        } else {
#ifdef USE_SERIAL_DBG
            Serial.print("No HMC5883L sensor found at address 0x");
            Serial.println(i2cAddress, HEX);
#endif
            bActive = false;
        }
    }

    void setFilterMode(FilterMode mode, bool silent = false) {
        switch (mode) {
        case FAST:
            filterMode = FAST;
            magXSensor.update(1, 2, 0.05);
            magYSensor.update(1, 2, 0.05);
            magZSensor.update(1, 2, 0.05);
            break;
        case MEDIUM:
            filterMode = MEDIUM;
            magXSensor.update(4, 30, 0.1);
            magYSensor.update(4, 30, 0.1);
            magZSensor.update(4, 30, 0.1);
            break;
        case LONGTERM:
        default:
            filterMode = LONGTERM;
            magXSensor.update(10, 600, 0.1);
            magYSensor.update(10, 600, 0.1);
            magZSensor.update(10, 600, 0.1);
            break;
        }
        if (!silent)
            publishFilterMode();
    }

  private:
    void publishMagX() {
        char buf[32];
        sprintf(buf, "%6.3f", magXValue);
        pSched->publish(name + "/sensor/magnetic_field_x", buf);
    }

    void publishMagY() {
        char buf[32];
        sprintf(buf, "%6.3f", magYValue);
        pSched->publish(name + "/sensor/magnetic_field_y", buf);
    }

    void publishMagZ() {
        char buf[32];
        sprintf(buf, "%6.3f", magZValue);
        pSched->publish(name + "/sensor/magnetic_field_z", buf);
    }

    void publishMagFieldStrength() {
        char buf[32];
        sprintf(buf, "%6.3f", getMagneticFieldStrength());
        pSched->publish(name + "/sensor/magnetic_field_strength", buf);
#ifdef USE_SERIAL_DBG
        Serial.print("Magnetic field strength: ");
        Serial.println(buf);
#endif
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

    bool sensorStateMachine() {
        bool newData = false;
        uint32_t rt;
        uint32_t rp;
        double x, y, z;
        // uint8_t reg_data[3] = {0x3c, 0x02, 0x01};  // 0x3c: control register, 0x02: one-shot mode, 0x01: single measurement
        switch (sensorState) {
        case HMC5883LSensorState::UNAVAILABLE:
            break;
        case HMC5883LSensorState::IDLE:
            if (initSingleMeasurement()) {
                sensorState = HMC5883LSensorState::MEASUREMENT_WAIT;
                stateMachineClock = micros();
            } else {
                sensorState = HMC5883LSensorState::WAIT_NEXT_MEASUREMENT;
                stateMachineClock = micros();
            }
            break;
        case HMC5883LSensorState::MEASUREMENT_WAIT:
            if (timeDiff(stateMachineClock, micros()) > measurement_delay) {  // measurement_dealy ms for measurement
                if (checkDataReady()) {
                    if (readSingleMeasurement(&x, &y, &z)) {
                        magXRawValue = x;
                        magYRawValue = y;
                        magZRawValue = z;
                        sensorState = HMC5883LSensorState::WAIT_NEXT_MEASUREMENT;
                        lastPollMs = millis();
                        newData = true;
                    } else {
                        sensorState = HMC5883LSensorState::WAIT_NEXT_MEASUREMENT;
                        lastPollMs = millis();
                    }
                } else {
                    stateMachineClock = micros();
                }
            }
            break;
        case HMC5883LSensorState::WAIT_NEXT_MEASUREMENT:
            if (timeDiff(lastPollMs, millis()) > pollRateMs) {
                sensorState = HMC5883LSensorState::IDLE;  // Start next cycle.
                lastPollMs = millis();
            }
            break;
        }
        return newData;
    }

    void loop() {
        double tempVal, pressVal;
        bool valChanged = false;
        if (bActive) {
            if (!sensorStateMachine()) return;  // no new data
            if (magXSensor.filter(&magXRawValue)) {
                magXValue = magXRawValue;
                publishMagX();
                valChanged = true;
            }
            if (magYSensor.filter(&magYRawValue)) {
                magYValue = magYRawValue;
                publishMagY();
                valChanged = true;
            }
            if (magZSensor.filter(&magZRawValue)) {
                magZValue = magZRawValue;
                publishMagZ();
                valChanged = true;
            }
            if (valChanged) {
                publishMagFieldStrength();
            }
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/sensor/magnetic_field/get") {
            publishMagX();
            publishMagY();
            publishMagZ();
            publishMagFieldStrength();
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
        } else if (topic == name + "/sensor/oversampling/set") {
            if (msg == "1") {
                setHMC5883LSamples(HMC5883LSamples::SAMPLE_AVERAGE_1);
            } else {
                if (msg == "2") {
                    setHMC5883LSamples(HMC5883LSamples::SAMPLE_AVERAGE_2);
                } else {
                    if (msg == "4") {
                        setHMC5883LSamples(HMC5883LSamples::SAMPLE_AVERAGE_4);
                    } else {
                        if (msg == "8") {
                            setHMC5883LSamples(HMC5883LSamples::SAMPLE_AVERAGE_8);
                        } else {  // default
                            setHMC5883LSamples(HMC5883LSamples::SAMPLE_AVERAGE_1);
                        }
                    }
                }
            }
        } else if (topic == name + "/sensor/gain/set") {
            if (msg == "0.88") {
                setHMC5883LGain(HMC5883LGain::GAIN_0_88);
            } else {
                if (msg == "1.3") {
                    setHMC5883LGain(HMC5883LGain::GAIN_1_3);
                } else {
                    if (msg == "1.9") {
                        setHMC5883LGain(HMC5883LGain::GAIN_1_9);
                    } else {
                        if (msg == "2.5") {
                            setHMC5883LGain(HMC5883LGain::GAIN_2_5);
                        } else {
                            if (msg == "4.0") {
                                setHMC5883LGain(HMC5883LGain::GAIN_4_0);
                            } else {
                                if (msg == "4.7") {
                                    setHMC5883LGain(HMC5883LGain::GAIN_4_7);
                                } else {
                                    if (msg == "5.6") {
                                        setHMC5883LGain(HMC5883LGain::GAIN_5_6);
                                    } else {
                                        if (msg == "8.1") {
                                            setHMC5883LGain(HMC5883LGain::GAIN_8_1);
                                        } else {  // default
                                            setHMC5883LGain(HMC5883LGain::GAIN_1_3);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

};  // MagneticFieldHMC5883L

}  // namespace ustd
