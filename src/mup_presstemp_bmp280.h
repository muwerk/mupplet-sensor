// mup_presstemp_bmp280.h
#pragma once

#ifndef tiny_twi_h
#include <Wire.h>
#else
typedef TinyWire TwoWire;
#endif

#include "scheduler.h"
#include "sensors.h"

namespace ustd {


// clang - format off
/*! \brief mupplet-sensor temperature and pressure with Bosch BMP280

The mup_presstemp_bmp280 mupplet measures temperature and pressure using BMP280 sensor.

This mupplet is a fully asynchronous state-machine with no delay()s, so it never blocks.

#### Messages sent by presstemp_bmp280 mupplet:

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

#### Messages received by presstemp_bmp280 mupplet:

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
| `<mupplet-name>/sensor/oversampling/get` | - | Returns samplemode: `ULTRA_LOW_POWER`, `LOW_POWER`, `STANDARD`, `HIGH_RESOLUTION`, `ULTRA_HIGH_RESOLUTION` |
| `<mupplet-name>/sensor/oversampling/set` | `ULTRA_LOW_POWER`, `LOW_POWER`, `STANDARD`, `HIGH_RESOLUTION`, `ULTRA_HIGH_RESOLUTION` | Set internal sensor oversampling mode |
| `<mupplet-name>/sensor/mode/get` | - | Returns filterMode: `FAST`, `MEDIUM`, or `LONGTERM` |
| `<mupplet-name>/sensor/mode/set` | `FAST`, `MEDIUM`, or `LONGTERM` | Set external additional filter values |

#### Sample code

For a complete examples see the `muwerk/examples` project.

```cpp
#include "ustd_platform.h"
#include "scheduler.h"
#include "net.h"
#include "mqtt.h"

#include "mup_presstemp_bmp280.h"

void appLoop();

ustd::Scheduler sched(10, 16, 32);
ustd::Net net(LED_BUILTIN);
ustd::Mqtt mqtt;

ustd::PressTempBMP280 bmp("myBMP280");

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
    bmp.begin(&sched, ustd::PressTempBMP280::BMPSampleMode::ULTRA_HIGH_RESOLUTION);

    sched.subscribe(tID, "myBMP280/sensor/temperature", sensorUpdates);
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
class PressTempBMP280 {
  private:
    String BMP280_VERSION = "0.1.0";
    Scheduler *pSched;
    TwoWire *pWire;
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
    bool captureRelative=false;

    // BMP Sensor calibration data
    int16_t dig_T2, dig_T3, dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
    uint16_t dig_T1, dig_P1, dig_reserved;

  public:
    enum BMPError { UNDEFINED, OK, I2C_HW_ERROR, I2C_WRONG_HARDWARE_AT_ADDRESS, I2C_DEVICE_NOT_AT_ADDRESS, I2C_REGISTER_WRITE_ERROR,
                    I2C_VALUE_WRITE_ERROR, I2C_WRITE_DATA_TOO_LONG, I2C_WRITE_NACK_ON_ADDRESS, 
                    I2C_WRITE_NACK_ON_DATA, I2C_WRITE_ERR_OTHER, I2C_WRITE_TIMEOUT, I2C_WRITE_INVALID_CODE,
                    I2C_READ_REQUEST_FAILED,I2C_CALIBRATION_READ_FAILURE};
    enum BMPSensorState {UNAVAILABLE, IDLE, MEASUREMENT_WAIT, WAIT_NEXT_MEASUREMENT};
    
    /*! Hardware accuracy modes of BMP280, while the sensor can have different pressure- and temperature oversampling, we use same for both temp and press. */
    enum BMPSampleMode {
                        ULTRA_LOW_POWER=1,       ///< 1 samples, pressure resolution 16bit / 2.62 Pa, rec temperature oversampling: x1
                        LOW_POWER=2,             ///< 2 samples, pressure resolution 17bit / 1.31 Pa, rec temperature oversampling: x1
                        STANDARD=3,              ///< 4 samples, pressure resolution 18bit / 0.66 Pa, rec temperature oversampling: x1
                        HIGH_RESOLUTION=4,       ///< 8 samples, pressure resolution 19bit / 0.33 Pa, rec temperature oversampling: x1
                        ULTRA_HIGH_RESOLUTION=5  ///< 16 samples, pressure resolution 20bit / 0.16 Pa, rec temperature oversampling: x2
                        };
    BMPError lastError;
    BMPSensorState sensorState;
    unsigned long errs=0;
    unsigned long oks=0;
    unsigned long pollRateUs = 2000000;
    uint16_t oversampleMode=3; // 1..5, see BMPSampleMode.
    uint16_t oversampleModePressure=3;
    uint16_t oversampleModeTemperature=1;
    enum FilterMode { FAST, MEDIUM, LONGTERM };
    #define MUP_BMP_INVALID_ALTITUDE -1000000.0
    double referenceAltitudeMeters;
    FilterMode filterMode;
    uint8_t i2c_address;
    ustd::sensorprocessor temperatureSensor = ustd::sensorprocessor(4, 600, 0.005);
    ustd::sensorprocessor pressureSensor = ustd::sensorprocessor(4, 600, 0.005);
    bool bActive=false;

    PressTempBMP280(String name, FilterMode filterMode = FilterMode::MEDIUM, uint8_t i2c_address=0x76)
        : name(name), filterMode(filterMode), i2c_address(i2c_address) {
        /*! Instantiate an BMP sensor mupplet
        @param name Name used for pub/sub messages
        @param filterMode FAST, MEDIUM or LONGTERM filtering of sensor values
        @param i2c_address Should always be 0x76 or 0x77 for BMP280, depending on SDO pin.
        */
        lastError=BMPError::UNDEFINED;
        sensorState=BMPSensorState::UNAVAILABLE;
        referenceAltitudeMeters = MUP_BMP_INVALID_ALTITUDE;
        relativeAltitudeStarted = false;
        captureRelative = false;
        setFilterMode(filterMode, true);
    }

    ~PressTempBMP280() {
    }

    void setReferenceAltitude(double _referenceAltitudeMeters) {
        /*! Set the altitude at current sensor location as reference
        @param _referenceAltitudeMeters The current altitude above sea level in meters
        */
        referenceAltitudeMeters=_referenceAltitudeMeters;
    }

    void startRelativeAltitude() {
        /*! Store current pressure for a reference altitude to start relative altitude measurement

        Once a reference altitude is defined (see \ref setReferenceAltitude()), measurement of relative
        altitude can be started by calling this function.
        */
        if (referenceAltitudeMeters!=MUP_BMP_INVALID_ALTITUDE) {
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
        if (referenceAltitudeMeters!=MUP_BMP_INVALID_ALTITUDE) {
            double prNN=_pressure/pow((1.0-(referenceAltitudeMeters/44330.0)),5.255);
            return prNN;
        }
        return 0.0;
    }

    void setSampleMode(BMPSampleMode _sampleMode) {
        oversampleMode=(uint16_t)_sampleMode;
        oversampleModePressure=oversampleMode;
        if (oversampleModePressure==BMPSampleMode::ULTRA_HIGH_RESOLUTION) {
            oversampleModeTemperature=2;  // as per datasheet recommendations.
        } else {
            oversampleModeTemperature=1;
        }
    }

    bool initBmpSensorConstants() {
        // Bosch made a long, winded paragraph about calibration data, 
        // but didn't manage to mention that this uses the other endianness :-/ .
        if (!i2c_readRegisterWordLE(0x88,(uint16_t*)&dig_T1)) return false;
        if (!i2c_readRegisterWordLE(0x8A,(uint16_t*)&dig_T2)) return false;
        if (!i2c_readRegisterWordLE(0x8C,(uint16_t*)&dig_T3)) return false;
        if (!i2c_readRegisterWordLE(0x8E,(uint16_t*)&dig_P1)) return false;
        if (!i2c_readRegisterWordLE(0x90,(uint16_t*)&dig_P2)) return false;
        if (!i2c_readRegisterWordLE(0x92,(uint16_t*)&dig_P3)) return false;
        if (!i2c_readRegisterWordLE(0x94,(uint16_t*)&dig_P4)) return false;
        if (!i2c_readRegisterWordLE(0x96,(uint16_t*)&dig_P5)) return false;
        if (!i2c_readRegisterWordLE(0x98,(uint16_t*)&dig_P6)) return false;
        if (!i2c_readRegisterWordLE(0x9A,(uint16_t*)&dig_P7)) return false;
        if (!i2c_readRegisterWordLE(0x9C,(uint16_t*)&dig_P8)) return false;
        if (!i2c_readRegisterWordLE(0x9E,(uint16_t*)&dig_P9)) return false;
        //if (!i2c_readRegisterWordLE(0xA0,(uint16_t*)&dig_reserved)) return false;
        dig_reserved=0xffff; // Don't try to read.
        return true;
    }

    void begin(Scheduler *_pSched, BMPSampleMode _sampleMode=BMPSampleMode::STANDARD, TwoWire *_pWire=&Wire) {
        pSched = _pSched;
        pWire=_pWire;
        uint8_t data;

        setSampleMode(_sampleMode);
        
        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 500);  // 500us

        auto fnall = [=](String topic, String msg, String originator) {
            this->subsMsg(topic, msg, originator);
        };
        pSched->subscribe(tID, name + "/sensor/#", fnall);

        lastError=i2c_checkAddress(i2c_address);
        if (lastError==BMPError::OK) {
            if (!i2c_readRegisterByte(0xd0, &data)) { // 0xd0: chip-id register
#ifdef USE_SERIAL_DBG
                Serial.print("Failed to inquire BMP280 chip-id at address 0x");
                Serial.println(i2c_address, HEX);
#endif
                bActive=false;
            } else {
                if (data==0x58) { // 0x58: signature of BMP280
                    if (!initBmpSensorConstants()) {
#ifdef USE_SERIAL_DBG
                        Serial.print("Failed to read calibration data for sensor BMP280 at address 0x");
                        Serial.println(i2c_address, HEX);
#endif
                        lastError=BMPError::I2C_CALIBRATION_READ_FAILURE;
                        bActive = false;
                    } else {
#ifdef USE_SERIAL_DBG
                        Serial.print("BMP280 sensor active at address 0x");
                        Serial.println(i2c_address, HEX);
#endif
                        sensorState=BMPSensorState::IDLE;
                        bActive=true;
                    }
                } else {
#ifdef USE_SERIAL_DBG
                    Serial.print("Wrong hardware (not BMP280) found at address 0x");
                    Serial.print(i2c_address, HEX);
                    Serial.print(" chip-id is ");
                    Serial.print(data, HEX);
                    Serial.println(" expected: 0x58 for BMP280.");
#endif
                    lastError=BMPError::I2C_WRONG_HARDWARE_AT_ADDRESS;
                    bActive = false;
                }
            }
        } else {
#ifdef USE_SERIAL_DBG
            Serial.print("No BMP280 sensor found at address 0x");
            Serial.println(i2c_address, HEX);
#endif
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

    BMPError i2c_checkAddress(uint8_t address) {
        pWire->beginTransmission(address);
        byte error = pWire->endTransmission();
        if (error == 0) {
            lastError=BMPError::OK;
            return lastError;
        } else if (error == 4) {
            lastError=I2C_HW_ERROR;
        }
        lastError=I2C_DEVICE_NOT_AT_ADDRESS;
        return lastError;
    }

    bool i2c_endTransmission(bool releaseBus) {
        uint8_t retCode=pWire->endTransmission(releaseBus); // true: release bus, send stop
        switch (retCode) {
            case 0:
                lastError=BMPError::OK;
                return true;
            case 1:
                lastError=BMPError::I2C_WRITE_DATA_TOO_LONG;
                return false;
            case 2:
                lastError=BMPError::I2C_WRITE_NACK_ON_ADDRESS;
                return false;
            case 3:
                lastError=BMPError::I2C_WRITE_NACK_ON_DATA;
                return false;
            case 4:
                lastError=BMPError::I2C_WRITE_ERR_OTHER;
                return false;
            case 5:
                lastError=BMPError::I2C_WRITE_TIMEOUT;
                return false;
            default:
                lastError=BMPError::I2C_WRITE_INVALID_CODE;
                return false;
        }
        return false;
    }

    bool i2c_readRegisterByte(uint8_t reg, uint8_t* pData) {
        *pData=(uint8_t)-1;
        pWire->beginTransmission(i2c_address);
        if (pWire->write(&reg,1)!=1) {
            lastError=BMPError::I2C_REGISTER_WRITE_ERROR;
            return false;
        }
        if (i2c_endTransmission(true)==false) return false;
        uint8_t read_cnt=pWire->requestFrom(i2c_address,(uint8_t)1,(uint8_t)true);
        if (read_cnt!=1) {
            lastError=I2C_READ_REQUEST_FAILED;
            return false;
        }
        *pData=pWire->read();
        return true;
    }

    bool i2c_readRegisterWord(uint8_t reg, uint16_t* pData) {
        *pData=(uint16_t)-1;
        pWire->beginTransmission(i2c_address);
        if (pWire->write(&reg,1)!=1) {
            lastError=BMPError::I2C_REGISTER_WRITE_ERROR;
            return false;
        }
        if (i2c_endTransmission(true)==false) return false;
        uint8_t read_cnt=pWire->requestFrom(i2c_address,(uint8_t)2,(uint8_t)true);
        if (read_cnt!=2) {
            lastError=I2C_READ_REQUEST_FAILED;
            return false;
        }
        uint8_t hb=pWire->read();
        uint8_t lb=pWire->read();
        uint16_t data=(hb<<8) | lb;
        *pData=data;
        return true;
    }
    
    bool i2c_readRegisterWordLE(uint8_t reg, uint16_t* pData) {
        *pData=(uint16_t)-1;
        pWire->beginTransmission(i2c_address);
        if (pWire->write(&reg,1)!=1) {
            lastError=BMPError::I2C_REGISTER_WRITE_ERROR;
            return false;
        }
        if (i2c_endTransmission(true)==false) return false;
        uint8_t read_cnt=pWire->requestFrom(i2c_address,(uint8_t)2,(uint8_t)true);
        if (read_cnt!=2) {
            lastError=I2C_READ_REQUEST_FAILED;
            return false;
        }
        uint8_t lb=pWire->read();
        uint8_t hb=pWire->read();
        uint16_t data=(hb<<8) | lb;
        *pData=data;
        return true;
    }
    
    bool i2c_readRegisterTripple(uint8_t reg, uint32_t* pData) {
        *pData=(uint32_t)-1;
        pWire->beginTransmission(i2c_address);
        if (pWire->write(&reg,1)!=1) {
            lastError=BMPError::I2C_REGISTER_WRITE_ERROR;
            return false;
        }
        if (i2c_endTransmission(true)==false) return false;
        uint8_t read_cnt=pWire->requestFrom(i2c_address,(uint8_t)3,(uint8_t)true);
        if (read_cnt!=3) {
            lastError=I2C_READ_REQUEST_FAILED;
            return false;
        }
        uint8_t hb=pWire->read();
        uint8_t lb=pWire->read();
        uint8_t xlb=pWire->read();
        uint32_t data=(hb<<16) | (lb<<8) | xlb;
        *pData=data;
        return true;
    }
    
    bool i2c_writeRegisterByte(uint8_t reg, uint8_t val, bool releaseBus=true) {
        pWire->beginTransmission(i2c_address);
        if (pWire->write(&reg,1)!=1) {
            lastError=BMPError::I2C_REGISTER_WRITE_ERROR;
            return false;
        }
        if (pWire->write(&val,1)!=1) {
            lastError=BMPError::I2C_VALUE_WRITE_ERROR;
            return false;
        }
        return i2c_endTransmission(releaseBus);
    }

    void publishTemperature() {
        char buf[32];
        sprintf(buf, "%6.2f", temperatureValue);
        pSched->publish(name + "/sensor/temperature", buf);
    }

    void publishPressure() {
        char buf[32];
        sprintf(buf, "%7.2f", pressureValue);
        pSched->publish(name + "/sensor/pressure", buf);
        if (referenceAltitudeMeters!=MUP_BMP_INVALID_ALTITUDE) {
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
        case BMPSampleMode::LOW_POWER:
            pSched->publish(name + "/sensor/oversampling", "LOW_POWER");
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
        sprintf(msg,"dig_T1=%u, dig_T2=%d, dig_T3=%d, dig_P1=%u, dig_P2=%d, dig_P3=%d, dig_P4=%d, dig_P5=%d, dig_P6=%d, dig_P7=%d, dig_P8=%d, dig_P9=%d, dig_reserved=%u",
                dig_T1, dig_T2, dig_T3, dig_P1, dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9, dig_reserved);
        pSched->publish("sensor/calibrationdata",msg);
    }

    void publishReferenceAltitude() {
        if (referenceAltitudeMeters!=MUP_BMP_INVALID_ALTITUDE) {
            char buf[32];
            sprintf(buf, "%7.2f", referenceAltitudeMeters);
            pSched->publish(name+"/sensor/referencealtitude",buf);
        } else {
            pSched->publish(name+"/sensor/referencealtitude", "unknown");
        }
    }

    void publishRelativeAltitude() {
        char buf[32];
        if (relativeAltitudeStarted) {
            double ralt = 44330.0 * ( 1.0 - pow(pressureValue/baseRelativeNNPressure,1./5.255));
            sprintf(buf, "%7.2f", ralt);
            pSched->publish(name+"/sensor/relativealtitude",buf);
            double dalt=ralt-referenceAltitudeMeters;
            sprintf(buf, "%7.2f", dalt);
            pSched->publish(name+"/sensor/deltaaltitude",buf);
        }
    }

    bool sensorStateMachine() {
        bool newData=false;
        uint32_t rt;
        uint32_t rp;
        uint8_t reg_data;
        const uint8_t status_register=0xf3;
        const uint8_t measure_mode_register=0xf4;
        const uint8_t config_register=0xf5;
        const uint8_t temperature_registers=0xfa;
        const uint8_t pressure_registers=0xf7;
        uint8_t status;
        uint8_t normalmodeInactivity=0, IIRfilter=0; // not used.
        switch (sensorState) {
            case BMPSensorState::UNAVAILABLE:
                break;
            case BMPSensorState::IDLE:
                reg_data=(normalmodeInactivity<<5) + (IIRfilter << 2) + 0;
                if (!i2c_writeRegisterByte(config_register, reg_data)) {
                    ++errs;
                    sensorState=BMPSensorState::WAIT_NEXT_MEASUREMENT;
                    stateMachineClock=micros();
                    break;
                }
                reg_data=(oversampleModeTemperature << 5) + (oversampleModePressure << 2) + 0x1;  // 0x3: normal mode, 0x1 one-shot
                if (!i2c_writeRegisterByte(measure_mode_register, reg_data)) {
                    ++errs;
                    sensorState=BMPSensorState::WAIT_NEXT_MEASUREMENT;
                    stateMachineClock=micros();
                    break;
                } else {  // Start temperature read
                    sensorState=BMPSensorState::MEASUREMENT_WAIT;
                    stateMachineClock=micros();
                }
                break;
            case BMPSensorState::MEASUREMENT_WAIT:
                if (!i2c_readRegisterByte(status_register, &status)) {
                    // no status
                    ++errs;
                    sensorState=BMPSensorState::WAIT_NEXT_MEASUREMENT;
                    stateMachineClock=micros();
                    break;
                } 
                status = status & 0x09;
                if (timeDiff(stateMachineClock,micros()) > 1 && status==0) { // 1ms for meas, no status set.
                    rt=0; rp=0;
                    if (i2c_readRegisterTripple(temperature_registers,&rt) && i2c_readRegisterTripple(pressure_registers, &rp)) {
                        rawTemperature=rt>>4;
                        rawPressure=rp>>4;
                        sensorState=BMPSensorState::WAIT_NEXT_MEASUREMENT;
                        stateMachineClock=micros();
                        ++oks;
                        newData=true;
                    } else {
                        ++errs;
                        sensorState=BMPSensorState::WAIT_NEXT_MEASUREMENT;
                        stateMachineClock=micros();
                    }
                } 
                break;
            case BMPSensorState::WAIT_NEXT_MEASUREMENT:
                if (timeDiff(stateMachineClock,micros()) > pollRateUs) {
                    sensorState=BMPSensorState::IDLE; // Start next cycle.
                }
                break;
        }
        return newData;
    }

    // From BOSCH datasheet BMA150 data sheet Rev. 1.4
    // Returns temperature in DegC, double precision. Output value of “51.23” equals 51.23 DegC.
    // t_fine carries fine temperature used by pressure
    //int32_t t_fine;
    double bmp280_compensate_T_double(int32_t adc_T, int32_t *pt_fine) {
        double var1, var2, T;
        var1 = (((double)adc_T)/16384.0 - ((double)dig_T1)/1024.0) * ((double)dig_T2);
        var2 = ((((double)adc_T)/131072.0 - ((double)dig_T1)/8192.0) *
        (((double)adc_T)/131072.0 - ((double)dig_T1)/8192.0)) * ((double)dig_T3);
        *pt_fine = (int32_t)(var1 + var2);
        T = (var1 + var2) / 5120.0;
        return T;
    }

    // From BOSCH datasheet BMA150 data sheet Rev. 1.4
    // Returns pressure in Pa as double. Output value of “96386.2” equals 96386.2 Pa = 963.862 hPa
    double bmp280_compensate_P_double(int32_t adc_P, int32_t t_fine) {
        double var1, var2, p;
        var1 = ((double)t_fine/2.0) - 64000.0;
        var2 = var1 * var1 * ((double)dig_P6) / 32768.0;
        var2 = var2 + var1 * ((double)dig_P5) * 2.0;
        var2 = (var2/4.0)+(((double)dig_P4) * 65536.0);
        var1 = (((double)dig_P3) * var1 * var1 / 524288.0 + ((double)dig_P2) * var1) / 524288.0;
        var1 = (1.0 + var1 / 32768.0)*((double)dig_P1);
        if (var1 == 0.0) {
            return 0; // avoid exception caused by division by zero
        }
        p = 1048576.0 - (double)adc_P;
        p = (p - (var2 / 4096.0)) * 6250.0 / var1;
        var1 = ((double)dig_P9) * p * p / 2147483648.0;
        var2 = p * ((double)dig_P8) / 32768.0;
        p = p + (var1 + var2 + ((double)dig_P7)) / 16.0;
        return p;
    }


    bool calibrateRawData() {
        int32_t t_fine;
        // Temperature
        calibratedTemperature = bmp280_compensate_T_double(rawTemperature, &t_fine);
        // Pressure
        calibratedPressure= bmp280_compensate_P_double(rawPressure, t_fine)/100.0;
        return true;
    }

    void loop() {
        double tempVal, pressVal;
        if (bActive) {
            if (!sensorStateMachine()) return; // no new data
            calibrateRawData();
            tempVal=(double)calibratedTemperature; 
            pressVal=(double)calibratedPressure;
            if (temperatureSensor.filter(&tempVal)) {
                temperatureValue = tempVal;
                publishTemperature();
            }
            if (pressureSensor.filter(&pressVal)) {
                pressureValue = pressVal;
                if (referenceAltitudeMeters!=MUP_BMP_INVALID_ALTITUDE) {
                    pressureNNValue=getPressureNN(pressureValue);
                    if (captureRelative) {
                        baseRelativeNNPressure=pressureNNValue;
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
            double alt=atof(msg.c_str());
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
                if (msg == "LOW_POWER") {
                    setSampleMode(BMPSampleMode::LOW_POWER);
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
    }

};  // PressTempBMP

}  // namespace ustd
