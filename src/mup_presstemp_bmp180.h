// mup_presstemp_bmp180.h
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
/*! \brief mupplet-sensor temperature and pressure with BMP085/180

The presstemp_bmp180 mupplet measures temperature and pressure using a BMP085 or BMP180 sensor

#### Messages sent by presstemp_bmp180 mupplet:

| topic | message body | comment |
| ----- | ------------ | ------- |
| `<mupplet-name>/sensor/temperature` | temperature in degree celsius | Float value encoded as string |
| `<mupplet-name>/sensor/pressure` | pressure in hPA | Float value encoded as string |
| `<mupplet-name>/sensor/mode` | `FAST`, `MEDIUM`, or `LONGTERM` | Integration time for sensor values |

#### Messages received by presstemp_bmp180 mupplet:

| topic | message body | comment |
| ----- | ------------ | ------- |
| `<mupplet-name>/sensor/temperature/get` | - | Causes current value to be sent. |
| `<mupplet-name>/sensor/pressure/get` | - | Causes current value to be sent. |
| `<mupplet-name>/sensor/mode/get` | - | Returns filterMode: `FAST`, `MEDIUM`, or `LONGTERM` |
| `<mupplet-name>/sensor/mode/set` | `FAST`, `MEDIUM`, or `LONGTERM` | Set integration time for illuminance values |

#### Sample code

For a complete examples see the `muwerk/examples` project.

```cpp
#define __ESP__ 1
#include "scheduler.h"
#include "mup_presstemp_bmp180.h"

ustd::Scheduler sched;
ustd::PressTempBMP bmp180("myBMP",D4);

void task0(String topic, String msg, String originator) {
    if (topic == "myBMP/sensor/temperature") {
        Serial.print("Temperature: ");
        Serial.prinln(msg);  // String float
    }
}

void setup() {
    bmp180.begin(&sched);
    String name="myBMP";
    auto fnall = [=](String topic, String msg, String originator) {
        this->subsMsg(topic, msg, originator);
    };
    pSched->subscribe(tID, name + "/sensor/#", fnall);
}
```
*/

// clang-format on
class PressTempBMP180 {
  private:
    String BMP180_VERSION = "0.1.0";
    Scheduler *pSched;
    TwoWire *pWire;
    int tID;
    String name;
    uint8_t i2c_address;
    double temperatureValue, pressureValue;
    unsigned long stateMachineClock;
    uint16_t rawTemperature;
    double calibratedTemperature;
    uint32_t rawPressure;
    double calibratedPressure;
    uint16_t oversampleMode=3; // 0..3

    // BMP Sensor calibration data
    int16_t CD_AC1, CD_AC2, CD_AC3, CD_B1, CD_B2, CD_MB, CD_MC, CD_MD;
    uint16_t CD_AC4, CD_AC5, CD_AC6;

  public:
    enum BMPType { BMP085, BMP180 };
    enum BMPError { UNDEFINED, OK, I2C_HW_ERROR, I2C_WRONG_HARDWARE_AT_ADDRESS, I2C_DEVICE_NOT_AT_ADDRESS, I2C_REGISTER_WRITE_ERROR,
                    I2C_VALUE_WRITE_ERROR, I2C_WRITE_DATA_TOO_LONG, I2C_WRITE_NACK_ON_ADDRESS, 
                    I2C_WRITE_NACK_ON_DATA, I2C_WRITE_ERR_OTHER, I2C_WRITE_TIMEOUT, I2C_WRITE_INVALID_CODE,
                    I2C_READ_REQUEST_FAILED,I2C_CALIBRATION_READ_FAILURE};
    enum BMPSensorState {UNAVAILABLE, IDLE, TEMPERATURE_WAIT, PRESSURE_WAIT, WAIT_NEXT_MEASUREMENT};
    BMPType bmp180Type;
    BMPError lastError;
    BMPSensorState sensorState;
    unsigned long errs=0;
    unsigned long oks=0;
    unsigned long pollRateUs = 2000000;
    enum FilterMode { FAST, MEDIUM, LONGTERM };
    FilterMode filterMode;
    ustd::sensorprocessor temperatureSensor = ustd::sensorprocessor(4, 600, 0.005);
    ustd::sensorprocessor pressureSensor = ustd::sensorprocessor(4, 600, 0.005);
    bool bActive=false;

    PressTempBMP180(String name, uint8_t i2c_address=0x77, BMPType bmp180Type=BMPType::BMP180, FilterMode filterMode = FilterMode::MEDIUM)
        : name(name), i2c_address(i2c_address), bmp180Type(bmp180Type), filterMode(filterMode) {
        /*! Instantiate an BMP sensor mupplet
        @param name Name used for pub/sub messages
        @param i2c_address Should always be 0x77 for BMP180, cannot be changed.
        @param bmp180Type BMP085, BMP180
        @param filterMode FAST, MEDIUM or LONGTERM filtering of sensor values
        */
        lastError=BMPError::UNDEFINED;
        sensorState=BMPSensorState::UNAVAILABLE;
        setFilterMode(filterMode, true);
    }

    ~PressTempBMP180() {
    }

    double getTemperature() {
        /*! Get current temperature
        @return Temperature (in degrees celsius)
        */
        return temperatureValue;
    }

    double getPressure() {
        /*! Get current pressure
        @return Pressure (in percent)
        */
        return pressureValue;
    }

    bool initBmpSensorConstants() {
        if (!i2c_readRegisterWord(0xaa,(uint16_t*)&CD_AC1)) return false;
        if (!i2c_readRegisterWord(0xac,(uint16_t*)&CD_AC2)) return false;
        if (!i2c_readRegisterWord(0xae,(uint16_t*)&CD_AC3)) return false;
        if (!i2c_readRegisterWord(0xb0,(uint16_t*)&CD_AC4)) return false;
        if (!i2c_readRegisterWord(0xb2,(uint16_t*)&CD_AC5)) return false;
        if (!i2c_readRegisterWord(0xb4,(uint16_t*)&CD_AC6)) return false;
        if (!i2c_readRegisterWord(0xb6,(uint16_t*)&CD_B1)) return false;
        if (!i2c_readRegisterWord(0xb8,(uint16_t*)&CD_B2)) return false;
        if (!i2c_readRegisterWord(0xba,(uint16_t*)&CD_MB)) return false;
        if (!i2c_readRegisterWord(0xbc,(uint16_t*)&CD_MC)) return false;
        if (!i2c_readRegisterWord(0xbe,(uint16_t*)&CD_MD)) return false;
        return true;
    }

    void begin(Scheduler *_pSched, TwoWire* _pWire=&Wire) {
        pSched = _pSched;
        pWire=_pWire;
        uint8_t data;

        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 500);  // 500us

        auto fnall = [=](String topic, String msg, String originator) {
            this->subsMsg(topic, msg, originator);
        };
        pSched->subscribe(tID, name + "/sensor/#", fnall);

        lastError=i2c_checkAddress(i2c_address);
        if (lastError==BMPError::OK) {
            if (!i2c_readRegisterByte(0xd0, &data)) { // 0xd0: chip-id register
                bActive=false;
            } else {
                if (data==0x55) { // 0x55: signature of BMP180
                    if (!initBmpSensorConstants()) {
                        lastError=BMPError::I2C_CALIBRATION_READ_FAILURE;
                        bActive = false;
                    } else {
                        sensorState=BMPSensorState::IDLE;
                        bActive=true;
                    }
                } else {

                    lastError=BMPError::I2C_WRONG_HARDWARE_AT_ADDRESS;
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
        sprintf(buf, "%6.2f", pressureValue);
        pSched->publish(name + "/sensor/pressure", buf);
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
        bool newData=false;
        switch (sensorState) {
            case BMPSensorState::UNAVAILABLE:
                break;
            case BMPSensorState::IDLE:
                if (!i2c_writeRegisterByte(0xf4,0x2e)) {
                    ++errs;
                    sensorState=BMPSensorState::WAIT_NEXT_MEASUREMENT;
                    stateMachineClock=micros();
                    break;
                } else {  // Start temperature read
                    sensorState=BMPSensorState::TEMPERATURE_WAIT;
                    stateMachineClock=micros();
                }
                break;
            case BMPSensorState::TEMPERATURE_WAIT:
                if (timeDiff(stateMachineClock,micros()) > 4500) { // 4.5ms for temp meas.
                    if (i2c_readRegisterWord(0xf6,&rawTemperature)) {
                        uint8_t cmd=0x34+(oversampleMode<<6);
                        if (!i2c_writeRegisterByte(0xf4,cmd)) {
                            ++errs;
                            sensorState=BMPSensorState::WAIT_NEXT_MEASUREMENT;
                            stateMachineClock=micros();
                        } else {
                            sensorState=BMPSensorState::PRESSURE_WAIT;
                            stateMachineClock=micros();
                        }
                    } else {
                        ++errs;
                        sensorState=BMPSensorState::WAIT_NEXT_MEASUREMENT;
                        stateMachineClock=micros();
                    }
                }
                break;
            case BMPSensorState::PRESSURE_WAIT:
                if (timeDiff(stateMachineClock,micros()) > 4500) { // 4.5ms for press meas.
                    if (i2c_readRegisterTripple(0xf6,&rawPressure)) {
                        ++oks;
                        newData=true;
                        sensorState=BMPSensorState::WAIT_NEXT_MEASUREMENT;
                        stateMachineClock=micros();
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

    bool calibrateRawData() {
        // Temperature
        long x1=((rawTemperature-(long)CD_AC6)*(long)CD_AC5) >> 15;
        long x2=((long)CD_MC << 11)/(x1+(long)CD_MD);
        long b5 = x1+x2;
        calibratedTemperature = ((double)b5+8.0)/160.0;
        // Pressure
        long b6=b5-4000;
        x1=(CD_B2*((b6*b6) >> 12)) >> 11;
        x2=(CD_AC2*b6) >> 11;
        long x3 = x1+x2;
        long b3 = ((CD_AC1*4+x3) << (oversampleMode + 2))/4;
        x1=(CD_AC3*b6) >> 13;
        x2=(CD_B1*((b6*b6) >> 12)) >> 16;
        x3=((x1+x2)+2) >> 2;
        unsigned long b4=(CD_AC4*(unsigned long)(x3+32768)) >> 15;
        long b7=((unsigned long)rawPressure-b3)*(50000>>oversampleMode);
        long p;
        if ((unsigned long)b7>0x80000000) {
            p=(b7*2)/b4;
        } else {
            p=(b7/b4)*2;
        }
        x1=(p >> 8) * (p >> 8);
        x1=(x1*3038)>>16;
        x2=(-7357*p) >> 16;
        calibratedPressure=(p+(x1+x2+3791)/16.0)/100.0; // hPa;
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
                publishPressure();
            }
            //char msg[256];
            //sprintf(msg,"AC1=%d, AC2=%d, AC3=%d, AC4=%u, AC5=%u, AC6=%u, B1=%d, B2=%d, MB=%d, MC=%d, MD=%d",CD_AC1, CD_AC2, CD_AC3, CD_AC4, CD_AC5, CD_AC6, CD_B1, CD_B2, CD_MB, CD_MC, CD_MD);
            //pSched->publish("myBMP180/cali",msg);
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
    }
};  // PressTempBMP

}  // namespace ustd
