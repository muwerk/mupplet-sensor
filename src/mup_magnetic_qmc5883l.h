// mup_magnetic_qmc5883l.h
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
| QMC5883L | Honeywell           |                    0.2  |          800 |
| MLX90393 | Melexis             |                    0.5  |        50000 |
| TLV493D  | Infineon            |                   98.0  |       130000 |

So the QMC5883L is a good choice for a high-resolution measurement of earth's magnetic field.
*/

namespace ustd {

// clang-format off
/*! \brief mupplet-sensor for magnetic field (compass) based on QMC5883L sensor

The mup_mag_qmc5883l mupplet reads the magnetic field from a QMC5883L sensor and sends the values for x, y, z

This mupplet is a fully asynchronous state-machine with no delay()s, so it never blocks.

#### Messages sent by mup_magnetic_qmc5883l mupplet:

messages are prefixed by `omu/<hostname>`:

| topic | message body | comment |
| ----- | ------------ | ------- |
| `<mupplet-name>/sensor/magnetic_field_x` |  | Float value encoded as string, sent periodically as available |
| `<mupplet-name>/sensor/magnetic_field_y` |  | Float value encoded as string, sent periodically as available |
| `<mupplet-name>/sensor/magnetic_field_z` |  | Float value encoded as string, sent periodically as available |
| `<mupplet-name>/sensor/magnetic_field_strength` |  | Float value encoded as string, sent periodically as available |
| `<mupplet-name>/sensor/error` |  | Error message |
| `<mupplet-name>/sensor/mode` | `FAST`, `MEDIUM`, or `LONGTERM` | Filter mode |

#### Messages received by mup_magnetic_qmc5883l mupplet:

Need to be prefixed by `<hostname>/`:

| topic | message body | comment |
| ----- | ------------ | ------- |
| `<mupplet-name>/sensor/magnetic_field/get` | - | Causes current value to be sent. |
| `<mupplet-name>/sensor/mode/get` | - | Returns filterMode: `FAST`, `MEDIUM`, or `LONGTERM` |
| `<mupplet-name>/sensor/mode/set` | `FAST`, `MEDIUM`, or `LONGTERM` | Set external additional filter values |
*/
// clang-format on

class MagneticFieldQMC5883L {
  private:
    String QMC5883L_VERSION = "0.1.0";
    Scheduler *pSched;
    TwoWire *pWire;
    I2CRegisters *pI2C;
    int tID;
    String name;
    double magXValue, magYValue, magZValue;
    double magXRawValue, magYRawValue, magZRawValue;
    unsigned long stateMachineClock;

  public:
    enum QMC5883LSensorState { UNAVAILABLE,
                               IDLE,
                               MEASUREMENT_WAIT,
                               WAIT_NEXT_MEASUREMENT };

    //! Hardware accuracy modes of QMC5883L, while the sensor can have different pressure- and temperature oversampling, we use same for both temp and press.
    enum QMC5883LOversampling {
        OVERSAMPLING_512 = 0b00000000,  // 512 samples
        OVERSAMPLING_256 = 0b01000000,  // 256 samples
        OVERSAMPLING_128 = 0b10000000,  // 128 samples
        OVERSAMPLING_64 = 0b11000000    // 64 samples
    };
    enum QMC5883LMeasurementMode {
        MEASUREMENT_MODE_NORMAL = 0x00000000,      // Standby
        MEASUREMENT_MODE_CONTINUOUS = 0x00000001,  // Continuous
    };
    enum QMC5883LDataOutputRate {
        DATA_OUTPUT_RATE_10HZ = 0b00000000,   // 10Hz
        DATA_OUTPUT_RATE_50HZ = 0b00000100,   // 50Hz
        DATA_OUTPUT_RATE_100HZ = 0b00001000,  // 100Hz
        DATA_OUTPUT_RATE_200HZ = 0b00001100,  // 200Hz
    };
    enum QMC5883LRange {
        RANGE_2G = 0b00000000,  // 2G
        RANGE_8G = 0b00010000,  // 8G
    };

    QMC5883LSensorState sensorState;
    unsigned long errs = 0;
    unsigned long oks = 0;
    unsigned long basePollRate = 500000L;
    uint32_t pollRateMs = 2000;
    uint32_t lastPollMs = 0;
    uint8_t qmc5883lmodeRegister = 0x00;

    enum FilterMode { FAST,
                      MEDIUM,
                      LONGTERM };

    FilterMode filterMode;
    uint8_t i2cAddress;
    ustd::sensorprocessor magXSensor = ustd::sensorprocessor(4, 600, 0.005);
    ustd::sensorprocessor magYSensor = ustd::sensorprocessor(4, 600, 0.005);
    ustd::sensorprocessor magZSensor = ustd::sensorprocessor(4, 600, 0.005);
    bool bActive = false;
    int measurement_delay = 6;  // 6ms for measurement

    MagneticFieldQMC5883L(String name, FilterMode filterMode = FilterMode::MEDIUM, uint8_t i2cAddress = 0x0d)
        : name(name), filterMode(filterMode), i2cAddress(i2cAddress) {
        //! Instantiate an QMC5883L sensor mupplet
        //! @param name Name used for pub/sub messages
        //! @param filterMode FAST, MEDIUM or LONGTERM filtering of sensor values
        //! @param i2cAddress 0x0d is the i2c address for the QMC5883L sensor
        sensorState = QMC5883LSensorState::UNAVAILABLE;
        pI2C = nullptr;
        setFilterMode(filterMode, true);
    }

    ~MagneticFieldQMC5883L() {
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

    bool setQMC5883LMode(uint8_t mode = OVERSAMPLING_512 | MEASUREMENT_MODE_CONTINUOUS | DATA_OUTPUT_RATE_10HZ |
                                        RANGE_2G) {
        if (!pI2C->writeRegisterByte(0x09, mode)) {
#ifdef USE_SERIAL_DBG
            Serial.println("Failed to set QMC5883L mode.");
#endif
            return false;
        }
        qmc5883lmodeRegister = mode;
        return true;
    }

    bool initSensor() {
        uint8_t reg;
        reg = 0x01;  // reset
        if (!pI2C->writeRegisterByte(0x0b, reg)) {
#ifdef USE_SERIAL_DBG
            Serial.println("Failed to reset QMC5883L sensor.");
#endif
            return false;
        }
        return true;
    }

    bool readRelativeTemperature(double *pTemp, double offsetTemp = 0.0) {
        uint8_t buf[2];
        if (!pI2C->readRegisterNBytes(0x07, buf, 2)) {
#ifdef USE_SERIAL_DBG
            Serial.println("Failed to read QMC5883L temperature.");
#endif
            return false;
        }
        int16_t temp = (int16_t)(buf[1] << 8 | buf[0]);
        *pTemp = (double)temp / 100.0 + offsetTemp;
        return true;
    }

    bool readMeasurement(double *pX, double *pY, double *pZ) {
        uint8_t buf[6];
        if (!pI2C->readRegisterNBytes(0x00, buf, 6)) {
#ifdef USE_SERIAL_DBG
            Serial.println("Failed to read QMC5883L single measurement.");
#endif
            return false;
        }
        int x = (int16_t)(buf[1] << 8 | buf[0]);
        int y = (int16_t)(buf[3] << 8 | buf[2]);
        int z = (int16_t)(buf[5] << 8 | buf[4]);

        // XXX Temp compensation
        *pX = (double)x;
        *pY = (double)y;
        *pZ = (double)z;
        return true;
    }

    bool getSensorIdentification() {
        uint8_t id;
        if (!pI2C->readRegisterByte(0x0D, &id)) {
#ifdef USE_SERIAL_DBG
            Serial.println("Failed to read sensor identification.");
#endif
            return false;
        }
        if (id == 0xff) {
#ifdef USE_SERIAL_DBG
            Serial.println("Sensor identification: 0xff, sensor type QMC5883L found.");
#endif
            return true;
        }
#ifdef USE_SERIAL_DBG
        Serial.print("Wrong sensor identification, expected 0xff, got: 0x");
        Serial.println(id, HEX);
#endif
        return false;
    }

    bool
    checkDataReady() {
        uint8_t status;
        if (!pI2C->readRegisterByte(0x06, &status)) {
#ifdef USE_SERIAL_DBG
            Serial.println("Failed to read QMC5883L status.");
#endif
            return false;
        } else {
            if (status & 0x01) {
                if (status & 0x02) {
#ifdef USE_SERIAL_DBG
                    Serial.println("Data ready, but QMC5883L OVERFLOW is set");
#endif
                    return false;
                }
                return true;
            } else {
#ifdef USE_SERIAL_DBG
                Serial.print("QMC5883L data not ready: 0x");
                Serial.println(status, HEX);
#endif
                if (status & 0x02) {
#ifdef USE_SERIAL_DBG
                    Serial.println("QMC5883L OVERFLOW is set");
#endif
                }
                if (status & 0x04) {
#ifdef USE_SERIAL_DBG
                    Serial.println("QMC5883L DATA SKIPPED is set");
#endif
                }
                return false;
            }
        }
    }

    void begin(Scheduler *_pSched, TwoWire *_pWire = &Wire, uint32_t _pollRateMs = 2000) {
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
            } else {
                sensorState = QMC5883LSensorState::IDLE;
                if (!initSensor()) {
#ifdef USE_SERIAL_DBG
                    Serial.println("Failed to initialize (reset) QMC5883L sensor.");
#endif
                    bActive = false;
                } else {
                    bActive = true;
                }
                if (!setQMC5883LMode()) {
#ifdef USE_SERIAL_DBG
                    Serial.println("Failed to set QMC5883L mode, deactivating sensor.");
#endif
                    bActive = false;
                } else {
#ifdef USE_SERIAL_DBG
                    Serial.println("QMC5883L sensor initialized.");
#endif
                    bActive = true;
                }
            }
        } else {
#ifdef USE_SERIAL_DBG
            Serial.print("No QMC5883L sensor found at address 0x");
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
        case QMC5883LSensorState::UNAVAILABLE:
            break;
        case QMC5883LSensorState::IDLE:
            sensorState = QMC5883LSensorState::MEASUREMENT_WAIT;
            stateMachineClock = micros();
            break;
        case QMC5883LSensorState::MEASUREMENT_WAIT:
            if (timeDiff(stateMachineClock, micros()) > measurement_delay) {  // measurement_dealy ms for measurement
                if (checkDataReady()) {
                    if (readMeasurement(&x, &y, &z)) {
                        magXRawValue = x;
                        magYRawValue = y;
                        magZRawValue = z;
                        sensorState = QMC5883LSensorState::WAIT_NEXT_MEASUREMENT;
                        lastPollMs = millis();
                        newData = true;
                    } else {
                        sensorState = QMC5883LSensorState::WAIT_NEXT_MEASUREMENT;
                        lastPollMs = millis();
                    }
                } else {
                    stateMachineClock = micros();
                }
            }
            break;
        case QMC5883LSensorState::WAIT_NEXT_MEASUREMENT:
            if (timeDiff(lastPollMs, millis()) > pollRateMs) {
                sensorState = QMC5883LSensorState::IDLE;  // Start next cycle.
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
        }
    }

};  // MagneticFieldQMC5883L

}  // namespace ustd
