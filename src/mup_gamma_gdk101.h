// http://allsmartlab.com/eng/294-2/
// http://allsmartlab.com/wp-content/uploads/2017/download/GDK101datasheet_v1.5.pdf
// http://allsmartlab.com/wp-content/uploads/2017/download/GDK101_Application_Note.zip


// mup_gamma_gdk101.h
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
/*! \brief mupplet-sensor temperature and pressure with Bosch GDK101

The mup_gamma_gdk101 mupplet measures temperature, pressure, and humity using the GDK101 sensor.

This mupplet is a fully asynchronous state-machine with no delay()s, so it never blocks.

#### Messages sent by gamma_gdk101 mupplet:

messages are prefixed by `omu/<hostname>`:

| topic | message body | comment |
| ----- | ------------ | ------- |
| `<mupplet-name>/sensor/gamma10minavg` | gamma radiation 10 min average μSv/h | Float value encoded as string, sent periodically as available |
| `<mupplet-name>/sensor/gamma1minavg` | gamma radiation 1 min average μSv/h  | Float value encoded as string, sent periodically as available |
| `<mupplet-name>/sensor/mode` | `FAST`, `MEDIUM`, or `LONGTERM` | Integration time for sensor values, external, additional integration |

#### Messages received by gamma_gdk101 mupplet:

Need to be prefixed by `<hostname>/`:

| topic | message body | comment |
| ----- | ------------ | ------- |
| `<mupplet-name>/sensor/gamma10minavg/get` | - | Causes current value to be sent. |
| `<mupplet-name>/sensor/gamma10minavg/get` | - | Causes current value to be sent. |
| `<mupplet-name>/sensor/mode/get` | - | Returns filterMode: `FAST`, `MEDIUM`, or `LONGTERM` |
| `<mupplet-name>/sensor/mode/set` | `FAST`, `MEDIUM`, or `LONGTERM` | Set external additional filter values |

#### Sample code

For a complete examples see the `muwerk/examples` project.

```cpp
#include "ustd_platform.h"
#include "scheduler.h"
#include "net.h"
#include "mqtt.h"

#include "mup_gamma_gdk101.h"

void appLoop();

ustd::Scheduler sched(10, 16, 32);
ustd::Net net(LED_BUILTIN);
ustd::Mqtt mqtt;

ustd::GammaGDK101 gdk("myGDK101");

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
    gdk.begin(&sched);

    sched.subscribe(tID, "myGDK101/sensor/gamma10minavg", sensorUpdates);
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
class GammaGDK101 {
  private:
    String GDK101_VERSION = "0.1.0";
    Scheduler *pSched;
    TwoWire *pWire;
    int tID;
    String name;
    double gamma10minavgValue, gamma1minavgValue;

  public:
    enum GDKError { UNDEFINED, OK, I2C_HW_ERROR, I2C_WRONG_HARDWARE_AT_ADDRESS, I2C_DEVICE_NOT_AT_ADDRESS, I2C_REGISTER_WRITE_ERROR,
                    I2C_VALUE_WRITE_ERROR, I2C_WRITE_DATA_TOO_LONG, I2C_WRITE_NACK_ON_ADDRESS, 
                    I2C_WRITE_NACK_ON_DATA, I2C_WRITE_ERR_OTHER, I2C_WRITE_TIMEOUT, I2C_WRITE_INVALID_CODE,
                    I2C_READ_REQUEST_FAILED};
    enum GDKSensorState {UNAVAILABLE, IDLE, MEASUREMENT_WAIT, WAIT_NEXT_MEASUREMENT};
    
    /*! Hardware accuracy modes of GDK101, while the sensor can have different pressure- and temperature oversampling, we use same for both temp and press. */
    enum GDKSampleMode {
                        ULTRA_LOW_POWER=1,       ///< 1 samples, pressure resolution 16bit / 2.62 Pa, rec temperature oversampling: x1
                        LOW_POWER=2,             ///< 2 samples, pressure resolution 17bit / 1.31 Pa, rec temperature oversampling: x1
                        STANDARD=3,              ///< 4 samples, pressure resolution 18bit / 0.66 Pa, rec temperature oversampling: x1
                        HIGH_RESOLUTION=4,       ///< 8 samples, pressure resolution 19bit / 0.33 Pa, rec temperature oversampling: x1
                        ULTRA_HIGH_RESOLUTION=5  ///< 16 samples, pressure resolution 20bit / 0.16 Pa, rec temperature oversampling: x2
                        };
    GDKError lastError;
    GDKSensorState sensorState;
    bool disIrq=false;
    unsigned long errs=0;
    unsigned long oks=0;
    unsigned long pollRateUs = 2000000;
    enum FilterMode { FAST, MEDIUM, LONGTERM };
    String firmwareVersion;
    FilterMode filterMode;
    uint8_t i2c_address;
    ustd::sensorprocessor gamma10minavgSensor = ustd::sensorprocessor(4, 600, 0.005);
    ustd::sensorprocessor gamma1minavgSensor = ustd::sensorprocessor(4, 600, 0.005);
    bool bActive=false;

    GammaGDK101(String name, FilterMode filterMode = FilterMode::FAST, uint8_t i2c_address=0x18)
        : name(name), filterMode(filterMode), i2c_address(i2c_address) {
        /*! Instantiate an GDK sensor mupplet
        @param name Name used for pub/sub messages
        @param filterMode FAST, MEDIUM or LONGTERM filtering of sensor values
        @param i2c_address Should always be 0x76 or 0x77 for GDK101, depending address config.
        */
        lastError=GDKError::UNDEFINED;
        sensorState=GDKSensorState::UNAVAILABLE;
        setFilterMode(filterMode, true);
    }

    ~GammaGDK101() {
    }

    double getGamma10minavt() {
        /*! Get gamma radiation 10min avg
        @return gamma radiation (μSv/h)
        */
        return gamma10minavgValue;
    }

    double getGamma1minavt() {
        /*! Get gamma radiation 1min avg
        @return gamma radiation (μSv/h)
        */
        return gamma1minavgValue;
    }

    void begin(Scheduler *_pSched, TwoWire *_pWire=&Wire, bool forceSlowClock=false) {
        pSched = _pSched;
        pWire=_pWire;

        pWire->begin();  // required!
        if (forceSlowClock) {
            pWire->setClock(100000L);
            disIrq=true;
        }
        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 15000000);  // 500us

        auto fnall = [=](String topic, String msg, String originator) {
            this->subsMsg(topic, msg, originator);
        };
        pSched->subscribe(tID, name + "/sensor/#", fnall);

        // I2c_checkAddress kills the sensor!
        //lastError=i2c_checkAddress(i2c_address);

        bActive = resetGDKSensor();
        if (bActive) {
            firmwareVersion=getGDKFirmwareVersion();
        }
    }

    void setFilterMode(FilterMode mode, bool silent = false) {
        switch (mode) {
        case FAST:
            filterMode = FAST;
            gamma10minavgSensor.smoothInterval = 1;
            gamma10minavgSensor.pollTimeSec = 2;
            gamma10minavgSensor.eps = 0.05;
            gamma10minavgSensor.reset();
            gamma1minavgSensor.smoothInterval = 1;
            gamma1minavgSensor.pollTimeSec = 2;
            gamma1minavgSensor.eps = 0.1;
            gamma1minavgSensor.reset();
            break;
        case MEDIUM:
            filterMode = MEDIUM;
            gamma10minavgSensor.smoothInterval = 4;
            gamma10minavgSensor.pollTimeSec = 30;
            gamma10minavgSensor.eps = 0.1;
            gamma10minavgSensor.reset();
            gamma1minavgSensor.smoothInterval = 4;
            gamma1minavgSensor.pollTimeSec = 30;
            gamma1minavgSensor.eps = 0.5;
            gamma1minavgSensor.reset();
            break;
        case LONGTERM:
        default:
            filterMode = LONGTERM;
            gamma10minavgSensor.smoothInterval = 10;
            gamma10minavgSensor.pollTimeSec = 600;
            gamma10minavgSensor.eps = 0.1;
            gamma10minavgSensor.reset();
            gamma1minavgSensor.smoothInterval = 50;
            gamma1minavgSensor.pollTimeSec = 600;
            gamma1minavgSensor.eps = 0.5;
            gamma1minavgSensor.reset();
            break;
        }
        if (!silent)
            publishFilterMode();
    }

  private:

    GDKError i2c_checkAddress(uint8_t address) {
        pWire->beginTransmission(address);
        byte error = pWire->endTransmission();
        if (error == 0) {
            lastError=GDKError::OK;
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
                lastError=GDKError::OK;
                return true;
            case 1:
                lastError=GDKError::I2C_WRITE_DATA_TOO_LONG;
                return false;
            case 2:
                lastError=GDKError::I2C_WRITE_NACK_ON_ADDRESS;
                return false;
            case 3:
                lastError=GDKError::I2C_WRITE_NACK_ON_DATA;
                return false;
            case 4:
                lastError=GDKError::I2C_WRITE_ERR_OTHER;
                return false;
            case 5:
                lastError=GDKError::I2C_WRITE_TIMEOUT;
                return false;
            default:
                lastError=GDKError::I2C_WRITE_INVALID_CODE;
                return false;
        }
        return false;
    }

    bool i2c_readRegisterByte(uint8_t reg, uint8_t* pData) {
        *pData=(uint8_t)-1;
        pWire->beginTransmission(i2c_address);
        if (pWire->write(&reg,1)!=1) {
            lastError=GDKError::I2C_REGISTER_WRITE_ERROR;
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

    bool i2c_readRegisterWord(uint8_t reg, uint16_t* pData, bool stop=true, bool allow_irqs=true) {
        *pData=(uint16_t)-1;
        if (!allow_irqs) noInterrupts();
        pWire->beginTransmission(i2c_address);
        if (pWire->write(&reg,1)!=1) {
            if (!allow_irqs) interrupts();
            lastError=GDKError::I2C_REGISTER_WRITE_ERROR;
            return false;
        }
        if (i2c_endTransmission(stop)==false) return false;
        uint8_t read_cnt=pWire->requestFrom(i2c_address,(uint8_t)2,(uint8_t)true);
        if (read_cnt!=2) {
            if (!allow_irqs) interrupts();
            lastError=I2C_READ_REQUEST_FAILED;
            return false;
        }
        if (!allow_irqs) interrupts();
        uint8_t hb=pWire->read();
        uint8_t lb=pWire->read();
        uint16_t data=(hb<<8) | lb;
        *pData=data;
        return true;
    }
    
    bool i2c_readRegisterWordLE(uint8_t reg, uint16_t* pData, bool allow_irqs=true) {
        *pData=(uint16_t)-1;
        if (!allow_irqs) noInterrupts();
        pWire->beginTransmission(i2c_address);
        if (pWire->write(&reg,1)!=1) {
            if (!allow_irqs) interrupts();
            lastError=GDKError::I2C_REGISTER_WRITE_ERROR;
            return false;
        }
        if (i2c_endTransmission(true)==false) return false;
        uint8_t read_cnt=pWire->requestFrom(i2c_address,(uint8_t)2,(uint8_t)true);
        if (read_cnt!=2) {
            lastError=I2C_READ_REQUEST_FAILED;
            if (!allow_irqs) interrupts();
            return false;
        }
        if (!allow_irqs) interrupts();
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
            lastError=GDKError::I2C_REGISTER_WRITE_ERROR;
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
            lastError=GDKError::I2C_REGISTER_WRITE_ERROR;
            return false;
        }
        if (pWire->write(&val,1)!=1) {
            lastError=GDKError::I2C_VALUE_WRITE_ERROR;
            return false;
        }
        return i2c_endTransmission(releaseBus);
    }

    void publishGamma10minavg() {
        char buf[32];
        sprintf(buf, "%6.3f", gamma10minavgValue);
        pSched->publish(name + "/sensor/gamma10minavg", buf);
    }

    void publishGamma1minavg() {
        char buf[32];
        sprintf(buf, "%6.3f", gamma1minavgValue);
        pSched->publish(name + "/sensor/gamma1minavg", buf);
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

    String getGDKFirmwareVersion() {
        uint16_t data;
        if (!i2c_readRegisterWord(0xb4, &data, true, disIrq)) { // version
#ifdef USE_SERIAL_DBG
            Serial.print("Failed to read version GDK101 at address 0x");
            Serial.print(i2c_address, HEX);
            Serial.print(" data: ");
            Serial.print(data, HEX);
            Serial.print(" lasterr: ");
            Serial.println(lastError, HEX);
            return "unknown";
#endif
        } else {
            uint8_t h=data>>8;
            uint8_t l=data&0xff;
            char buf[64];
            sprintf(buf,"V%d.%d",h,l);
            Serial.print("Firmware version GTK ");
            Serial.println(buf);
            return String(buf);
        }
    }

    bool resetGDKSensor() {
        uint16_t data;
        if (!i2c_readRegisterWord(0xa0, &data, true, disIrq)) { // reset
#ifdef USE_SERIAL_DBG
            Serial.print("Failed to reset GDK101 at address 0x");
            Serial.print(i2c_address, HEX);
            Serial.print(" data: ");
            Serial.print(data, HEX);
            Serial.print(" lasterr: ");
            Serial.println(lastError, HEX);
#endif
            pSched->publish("dbg/1","RESFAIL");
            bActive=false;
            return false;
        } else {
            if ((data>>8)==1) {
                bActive=true;
#ifdef USE_SERIAL_DBG
                Serial.print("GDK101 sensor found at address 0x");
                Serial.print(i2c_address, HEX);
                Serial.print(". Reset returned: ");
                Serial.println(data, HEX);
#endif
            } else {
                Serial.print("Failed to reset GDK101 at address 0x");
                Serial.print(i2c_address, HEX);
                Serial.print(" data: ");
                Serial.print(data, HEX);
                Serial.print(" lasterr: ");
                Serial.println(lastError, HEX);
            }
        }
        return bActive;
    } 

    bool readGDKSensorMeasurement(uint8_t reg, double *pMeas) {
        uint16_t data;
        *pMeas=0.0;
        if (!i2c_readRegisterWord(reg, &data, true, disIrq)) { // reset
#ifdef USE_SERIAL_DBG
            Serial.print("Failed to read GDK101 at address 0x");
            Serial.print(i2c_address, HEX);
            Serial.print(" data: ");
            Serial.print(data, HEX);
            Serial.print(" lasterr: ");
            Serial.println(lastError, HEX);
#endif
            return false;
        } else {
            double ms=(double)(data>>8)+(double)(data&0xff)/100.0;
            *pMeas=ms;
            return true;
        }
    }

    bool readGDKSensor(double *pGamma10minavgVal, double *pGamma1minavgVal) {
        *pGamma10minavgVal=0.0;
        *pGamma1minavgVal=0.0;
        if (!readGDKSensorMeasurement(0xb2, pGamma10minavgVal)) return false;
        if (!readGDKSensorMeasurement(0xb3, pGamma1minavgVal)) return false;
        return true;
    } 

    void loop() {
        double gamma10minavgVal, gamma1minavgVal;
        if (bActive) {
            if (readGDKSensor(&gamma10minavgVal, &gamma1minavgVal)) {
                if (gamma10minavgSensor.filter(&gamma10minavgVal)) {
                    gamma10minavgValue = gamma10minavgVal;
                    publishGamma10minavg();
                }
                if (gamma1minavgSensor.filter(&gamma1minavgVal)) {
                    gamma1minavgValue = gamma1minavgVal;
                    publishGamma1minavg();
                }
            }
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/sensor/gamma10minavg/get") {
            publishGamma10minavg();
        }
        if (topic == name + "/sensor/gamma1minavg/get") {
            publishGamma1minavg();
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

};  // GammaGDK

}  // namespace ustd
