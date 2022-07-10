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
/*! \brief mupplet-sensor temperature and pressure with BMP180

The mup_presstemp_bmp180 mupplet measures temperature and pressure using a or BMP180 sensor. It should
also work with the outdated BMP085.

#### Messages sent by presstemp_bmp180 mupplet:

messages are prefixed by `omu\<hostname>`:

| topic | message body | comment |
| ----- | ------------ | ------- |
| `<mupplet-name>/sensor/temperature` | temperature in degree celsius | Float value encoded as string, sent periodically as available |
| `<mupplet-name>/sensor/pressure` | pressure in hPA for current altitude | Float value encoded as string, sent periodically as available |
| `<mupplet-name>/sensor/pressureNN` | pressure in hPA adjusted for sea level (requires setAltitude() to be called) | Float value encoded as string, sent periodically as available |
| `<mupplet-name>/sensor/calibrationdata` | a string with values of all internal calibration variables | descriptive string |
| `<mupplet-name>/sensor/altitude` | altitude above sea level as set with setAltitude() | Float value encoded as string |
| `<mupplet-name>/sensor/oversampling` | `ULTRA_LOW_POWER`, `STANDARD`, `HIGH_RESOLUTION`, `ULTRA_HIGH_RESOLUTION` | Internal sensor oversampling mode (sensor hardware) |
| `<mupplet-name>/sensor/mode` | `FAST`, `MEDIUM`, or `LONGTERM` | Integration time for sensor values, external, additional integration |

#### Messages received by presstemp_bmp180 mupplet:

Need to be prefixed by `<hostname>/`:

| topic | message body | comment |
| ----- | ------------ | ------- |
| `<mupplet-name>/sensor/temperature/get` | - | Causes current value to be sent. |
| `<mupplet-name>/sensor/pressure/get` | - | Causes current value to be sent. |
| `<mupplet-name>/sensor/pressureNN/get` | - | Causes current value to be sent. |
| `<mupplet-name>/sensor/altitude/get` | - | Causes current value to be sent. |
| `<mupplet-name>/sensor/altitude/set` | float encoded as string of current altitude in meters | Once altitude is set, pressureNN values can be calculated. |
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
    bmp.setAltitude(518.0); // 518m above NN, now we also receive PressureNN values for sea level.
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
    int tID;
    String name;
    uint8_t i2c_address;
    double temperatureValue, pressureValue, pressureNNValue;
    unsigned long stateMachineClock;
    int32_t rawTemperature;
    double calibratedTemperature;
    int32_t rawPressure;
    double calibratedPressure;

    // BMP Sensor calibration data
    int16_t CD_AC1, CD_AC2, CD_AC3, CD_B1, CD_B2, CD_MB, CD_MC, CD_MD;
    uint16_t CD_AC4, CD_AC5, CD_AC6;

  public:
    enum BMPError { UNDEFINED, OK, I2C_HW_ERROR, I2C_WRONG_HARDWARE_AT_ADDRESS, I2C_DEVICE_NOT_AT_ADDRESS, I2C_REGISTER_WRITE_ERROR,
                    I2C_VALUE_WRITE_ERROR, I2C_WRITE_DATA_TOO_LONG, I2C_WRITE_NACK_ON_ADDRESS, 
                    I2C_WRITE_NACK_ON_DATA, I2C_WRITE_ERR_OTHER, I2C_WRITE_TIMEOUT, I2C_WRITE_INVALID_CODE,
                    I2C_READ_REQUEST_FAILED,I2C_CALIBRATION_READ_FAILURE};
    enum BMPSensorState {UNAVAILABLE, IDLE, TEMPERATURE_WAIT, PRESSURE_WAIT, WAIT_NEXT_MEASUREMENT};
    
    /*! Hardware accuracy modes of BMP180 */
    enum BMPSampleMode {ULTRA_LOW_POWER=0,      //!> 1 samples, 4.5ms conversion time, 3uA current at 1 sample/sec, 0.06 RMS noise typ. [hPA]
                        STANDARD=1,             //!> 2 samples, 7.5ms conversion time, 5uA current at 1 sample/sec, 0.05 RMS noise typ. [hPA]
                        HIGH_RESOLUTION=2,      //!> 4 samples, 13.5ms conversion time, 7uA current at 1 sample/sec, 0.04 RMS noise typ. [hPA]
                        ULTRA_HIGH_RESOLUTION=3 //!> 8 samples, 25.5ms conversion time, 12uA current at 1 sample/sec, 0.03 RMS noise typ. [hPA]
                        };
    BMPError lastError;
    BMPSensorState sensorState;
    unsigned long errs=0;
    unsigned long oks=0;
    unsigned long pollRateUs = 2000000;
    uint16_t oversampleMode=2; // 0..3, see BMPSampleMode.
    enum FilterMode { FAST, MEDIUM, LONGTERM };
    #define MUP_BMP_INVALID_ALTITUDE -1000000.0
    double altitudeMeters;
    FilterMode filterMode;
    ustd::sensorprocessor temperatureSensor = ustd::sensorprocessor(4, 600, 0.005);
    ustd::sensorprocessor pressureSensor = ustd::sensorprocessor(4, 600, 0.005);
    bool bActive=false;

    PressTempBMP180(String name, uint8_t i2c_address=0x77, FilterMode filterMode = FilterMode::MEDIUM)
        : name(name), i2c_address(i2c_address), bmp180Type(bmp180Type), filterMode(filterMode) {
        /*! Instantiate an BMP sensor mupplet
        @param name Name used for pub/sub messages
        @param i2c_address Should always be 0x77 for BMP180, cannot be changed.
        @param bmp180Type BMP085, BMP180
        @param filterMode FAST, MEDIUM or LONGTERM filtering of sensor values
        */
        lastError=BMPError::UNDEFINED;
        sensorState=BMPSensorState::UNAVAILABLE;
        altitudeMeters = MUP_BMP_INVALID_ALTITUDE;
        setFilterMode(filterMode, true);
    }

    ~PressTempBMP180() {
    }

    void setAltitude(double _altitudeMeters) {
        altitudeMeters=_altitudeMeters;
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

    double getPressureNN(double _pressure) {
        if (altitudeMeters!=MUP_BMP_INVALID_ALTITUDE) {
            double prNN=_pressure/pow((1.0-(altitudeMeters/44330.0)),5.255);
            return prNN;
        }
        return 0.0;
    }

    void setSampleMode(BMPSampleMode _sampleMode) {
        oversampleMode=(uint16_t)_sampleMode;
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

    void begin(Scheduler *_pSched, BMPSampleMode _sampleMode=BMPSampleMode::STANDARD, TwoWire *_pWire=&Wire) {
        pSched = _pSched;
        setSampleMode(_sampleMode);
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
        sprintf(buf, "%7.2f", pressureValue);
        pSched->publish(name + "/sensor/pressure", buf);
        if (altitudeMeters!=MUP_BMP_INVALID_ALTITUDE) {
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
        sprintf(msg,"AC1=%d, AC2=%d, AC3=%d, AC4=%u, AC5=%u, AC6=%u, B1=%d, B2=%d, MB=%d, MC=%d, MD=%d",CD_AC1, CD_AC2, CD_AC3, CD_AC4, CD_AC5, CD_AC6, CD_B1, CD_B2, CD_MB, CD_MC, CD_MD);
        pSched->publish("sensor/calibrationdata",msg);
    }

    void publishAltitude() {
        if (altitudeMeters!=MUP_BMP_INVALID_ALTITUDE) {
            char buf[32];
            sprintf(buf, "%7.2f", altitudeMeters);
            pSched->publish(name+"/sensor/altitude",buf);
        } else {
            pSched->publish(name+"/sensor/altitude", "unknown");
        }
    }

    bool sensorStateMachine() {
        bool newData=false;
        uint16_t rt;
        uint32_t rp;
        unsigned long convTimeOversampling[4]={4500, 7500, 13500, 25500}; // sample time dependent on oversample-mode for pressure.
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
                    rt=0;
                    if (i2c_readRegisterWord(0xf6,&rt)) {
                        rawTemperature=rt;
                        uint8_t cmd=0x34 | (oversampleMode<<6);
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
                if (timeDiff(stateMachineClock,micros()) > convTimeOversampling[oversampleMode]) { // Oversamp. dep. for press meas.
                    rp=0;
                    if (i2c_readRegisterTripple(0xf6,&rp)) {
                        rawPressure=rp >> (8-oversampleMode);
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
        int32_t x1=(((uint32_t)rawTemperature-(uint32_t)CD_AC6)*(uint32_t)CD_AC5) >> 15;
        int32_t x2=((int32_t)CD_MC << 11)/(x1+(int32_t)CD_MD);
        int32_t b5 = x1+x2;
        calibratedTemperature = ((double)b5+8.0)/160.0;
        // Pressure
        int32_t b6=b5-(int32_t)4000;
        x1=((int32_t)CD_B2*((b6*b6) >> 12)) >> 11;
        x2=((int32_t)CD_AC2*b6) >> 11;
        int32_t x3 = x1+x2;
        int32_t b3 = ((((int32_t)CD_AC1*4L+x3) << oversampleMode) + 2L)/4;
        
        //char msg[128];
        //sprintf(msg,"x1=%d,x2=%d, x3=%d,b3=%d,rt=%d,rp=%d",x1,x2,x3,b3,rawTemperature,rawPressure);
        //pSched->publish("myBMP180/sensor/debug1",msg);

        x1=((int32_t)CD_AC3*b6) >> 13;
        x2=((int32_t)CD_B1*((b6*b6) >> 12)) >> 16;
        x3=((x1+x2)+2) >> 2;
        uint32_t b4=((uint32_t)CD_AC4*(uint32_t)(x3+(uint32_t) 32768)) >> 15;
        uint32_t b7=((uint32_t)rawPressure-b3)*((uint32_t)50000>>oversampleMode);
        
        //sprintf(msg,"x1=%d,x2=%d, x3=%d,b4=%u,b7=%u",x1,x2,x3,b4,b7);
        //pSched->publish("myBMP180/sensor/debug2",msg);

        int32_t p;
        if (b7<(uint32_t)0x80000000) {
            p=(b7*2)/b4;
        } else {
            p=(b7/b4)*2;
        }
        x1=(p >> 8) * (p >> 8);
        x1=(x1*(int32_t)3038)>>16;
        x2=(((int32_t)-7357)*p) >> 16;

        int32_t cp=p+ ((x1+x2+(int32_t)3791) >> 4);
        
        //sprintf(msg,"x1=%d,x2=%d,p=%d,cp=%d",x1,x2,p,cp);
        //pSched->publish("myBMP180/sensor/debug3",msg);

        calibratedPressure=cp/100.0; // hPa;
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
                if (altitudeMeters!=MUP_BMP_INVALID_ALTITUDE) {
                    pressureNNValue=getPressureNN(pressureValue);
                }
                publishPressure();
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
        if (topic == name + "/sensor/altitude/get") {
            publishAltitude();
        }
        if (topic == name + "/sensor/oversampling/get") {
            publishOversampling();
        }
        if (topic == name + "/sensor/altitude/set") {
            double alt=atof(msg.c_str());
            setAltitude(alt);
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
                setSampleMode(BMPSampleMode::ULTRA_HIGH_RESOLUTION);
            } else {
                if (msg == "STANDARD") {
                    setSampleMode(BMPSampleMode::ULTRA_HIGH_RESOLUTION);
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
