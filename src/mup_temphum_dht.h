// mup_temphum_dht.h
#pragma once

#include "scheduler.h"
#include "sensors.h"

namespace ustd {

#if defined(__ESP32__) || defined (__ESP__)
#define G_INT_ATTR IRAM_ATTR
#else
#define G_INT_ATTR
#endif

#define USTD_DHT_MAX_PIRQS (10)

/*! States of the interrupt handler protocol automat */
enum DhtProtState {NONE,                           /*!< idle, no operation */ 
                   START_PULSE_START,              /*!< start of (1) [see state diagram below], mcu starts write +-20ms low-pulse */
                   START_PULSE_END,                /*!< end of (1), a short high pulse is sent by MCU before switching to input and activating IRQ handler */
                   REPL_PULSE_START,               /*!< start of (3.1), preamble, dht writes 80us low pulse */
                   REPL_PULSE_START_H,             /*!< start of (3.2), preamble, dht writes 80us high pulse */
                   DATA_ACQUISITION_INTRO_START,   /*!< start of (4), lead-in for data-bit */
                   DATA_ACQUISITION_INTRO_END,     /*!< end of (4), starting to receive data bit */
                   DATA_ACQUISITION,               /*!< end of (5), either 0bit(27us) or 1bit(70us) received */
                   DATA_ABORT,                     /*!< error condition, timeout, or illegal state, state machine aborted, no valid result */
                   DATA_OK                         /*!< five data bits have been received and can be decoded */
                   };

enum DhtFailureCode {OK=0, BAD_START_PULSE_LEVEL=1, BAD_REPLY_PULSE_LENGTH=2, BAD_START_PULSE_END_LEVEL=3, BAD_REPLY_PULSE_LENGTH2=4, BAD_START_PULSE_END_LEVEL2=5, 
                     BAD_DATA_INTRO_PULSE_LENGTH=6, BAD_DATA_BIT_LENGTH=7};

volatile DhtProtState pDhtState[USTD_DHT_MAX_PIRQS] = {DhtProtState::NONE,DhtProtState::NONE,DhtProtState::NONE,DhtProtState::NONE,
    DhtProtState::NONE,DhtProtState::NONE,DhtProtState::NONE,DhtProtState::NONE,DhtProtState::NONE,DhtProtState::NONE};
volatile unsigned long pDhtBeginIrqTimer[USTD_DHT_MAX_PIRQS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
volatile uint8_t pDhtPortIrq[USTD_DHT_MAX_PIRQS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
volatile uint8_t pDhtBitCounter[USTD_DHT_MAX_PIRQS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
volatile DhtFailureCode pDhtFailureCode[USTD_DHT_MAX_PIRQS] = {DhtFailureCode::OK,DhtFailureCode::OK,DhtFailureCode::OK,
    DhtFailureCode::OK,DhtFailureCode::OK,DhtFailureCode::OK,DhtFailureCode::OK,DhtFailureCode::OK,DhtFailureCode::OK,
    DhtFailureCode::OK};
volatile int pDhtFailureData[USTD_DHT_MAX_PIRQS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
volatile uint8_t sensorDataBytes[USTD_DHT_MAX_PIRQS*5];


// clang - format off
/*! DHT protocol state diagram
..........MCU awakens DHT...............||.........DHT preamble..........|......data bit 1...........|......data bit 2...........| -> 40 data bis.
...........MCU writes...................||..............MCU reads, DHT writes.......................................................
 - - - -+                      +--------||- - -+            +--- 80us ---+         +- 27us or 70 us -+         +- 27us or 70 us -+
        |                      |               |            |            |         |  0bit    1bit   |         |  0bit    1bit   |
        |                      |               |            |            |         |                 |         |                 |
        |                      |               |            |            |         |                 |         |                 |
        |                      |               |            |            |         |                 |         |                 |
        +--------// 22ms // ---+               +--- 80us ---+            +--50 us -+                 +--50 us -+                 + . . . 38 more bits
                 (1)              (2)    |          (3.1)        (3.2)       (4)           (5)        
*/
// clang - format on

int dhtWakeUpPulse=22000;   // (1) The intial low-pulse of 22ms that awakens the DHT. Note: manufacturer doc is wrong! Says 2ms.  
int dhtInitialDelay=20;     // (2) after about 20ms(!) low by mcpu, at least dhtInitialDelay uS high is set by mcu before switching to input.
int dhtSignalInitDelta=25;  // (3.1, 3.2) deviation for initial 80us low + high by dht, initialDeals uS get substracted from this.
int dhtSignalIntroDelta=25; // (4) deviation for initial 50us low by dht
int dhtSignalDelta=15;      // (5) uS count for max signal length deviation, 27+-dhtSignalDelta is low, 70+-dhtSignalDelta is high-bit

void G_INT_ATTR ustd_dht_pirq_master(uint8_t irqno) {
    long dt;
    switch (pDhtState[irqno]) {
        case DhtProtState::NONE:
            return;
        case DhtProtState::START_PULSE_START:
            return;
        case DhtProtState::START_PULSE_END:  // start of (3.1)
            if (digitalRead(pDhtPortIrq[irqno])==LOW) {
                pDhtBeginIrqTimer[irqno]=micros();
                pDhtState[irqno]=DhtProtState::REPL_PULSE_START;
                pDhtBitCounter[irqno]=0;
                for (int i=0; i<5; i++) sensorDataBytes[irqno*5+i]=0;
                return;
            } else {
                pDhtFailureCode[irqno] = DhtFailureCode::BAD_START_PULSE_LEVEL;
                pDhtState[irqno] = DhtProtState::DATA_ABORT;
            }
            break;
        case DhtProtState::REPL_PULSE_START: // start of (3.2)
            if (digitalRead(pDhtPortIrq[irqno])==HIGH) {
                dt=timeDiff(pDhtBeginIrqTimer[irqno], micros());
                if (dt>80-dhtInitialDelay-dhtSignalInitDelta && dt < 80+dhtSignalInitDelta) { // First alive signal of 80us LOW +- dhtSignalInitDelta uS.
                    pDhtBeginIrqTimer[irqno]=micros();
                    pDhtState[irqno]=DhtProtState::REPL_PULSE_START_H;
                    return;
                } else {
                    pDhtFailureCode[irqno] = DhtFailureCode::BAD_REPLY_PULSE_LENGTH;
                    pDhtFailureData[irqno] = dt;
                    pDhtState[irqno] = DhtProtState::DATA_ABORT;
                }
            } else {
                pDhtFailureCode[irqno] = DhtFailureCode::BAD_START_PULSE_END_LEVEL;
                pDhtState[irqno] = DhtProtState::DATA_ABORT;
            }
            break;
        case DhtProtState::REPL_PULSE_START_H: // end of (3.2), start of data transmission (4)
            if (digitalRead(pDhtPortIrq[irqno])==LOW) {
                dt=timeDiff(pDhtBeginIrqTimer[irqno], micros());
                if (dt>80-dhtSignalInitDelta && dt < 80+dhtSignalInitDelta) { // Sec alive signal of 80us HIGH +- dhtSignalInitDelta uS.
                    pDhtBeginIrqTimer[irqno]=micros();
                    pDhtState[irqno]=DhtProtState::DATA_ACQUISITION_INTRO_END;
                    return;
                } else {
                    pDhtFailureCode[irqno] = DhtFailureCode::BAD_REPLY_PULSE_LENGTH2;
                    pDhtFailureData[irqno] = dt;
                    pDhtState[irqno] = DhtProtState::DATA_ABORT;
                }
            } else {
                pDhtFailureCode[irqno] = DhtFailureCode::BAD_START_PULSE_END_LEVEL2;
                pDhtState[irqno] = DhtProtState::DATA_ABORT;
            }
            break;

        case DhtProtState::DATA_ACQUISITION_INTRO_START: // start of (4)
            if (digitalRead(pDhtPortIrq[irqno])==LOW) {
                pDhtBeginIrqTimer[irqno]=micros();
                pDhtState[irqno]=DhtProtState::DATA_ACQUISITION_INTRO_END;
            } 
        case DhtProtState::DATA_ACQUISITION_INTRO_END: // end of (4)
            if (digitalRead(pDhtPortIrq[irqno])==HIGH) {
                dt=timeDiff(pDhtBeginIrqTimer[irqno], micros());
                if (dt>50-dhtSignalIntroDelta && dt < 50+dhtSignalIntroDelta) { // Intro-marker 50us LOW +- dhtSignalIntroDelta uS.
                    pDhtBeginIrqTimer[irqno]=micros();
                    pDhtState[irqno]=DhtProtState::DATA_ACQUISITION;
                    return;
                } else {
                    pDhtFailureCode[irqno] = DhtFailureCode::BAD_DATA_INTRO_PULSE_LENGTH;
                    pDhtFailureData[irqno] = dt;
                    pDhtState[irqno] = DhtProtState::DATA_ABORT;
                }
            }
            break;
        case DhtProtState::DATA_ACQUISITION: // end of (5), data bit of either 27us (0bit) or 70us (1bit) received.
            if (digitalRead(pDhtPortIrq[irqno])==LOW) {
                dt=timeDiff(pDhtBeginIrqTimer[irqno], micros());
                if (dt>27-dhtSignalDelta && dt < 27+dhtSignalDelta) { // Zero-bit 26-28uS 
                   // nothing
                } else {
                    if (dt>70-dhtSignalDelta && dt < 70+dhtSignalDelta) { // One-bit 70uS
                        uint8_t byte=pDhtBitCounter[irqno]/8;
                        uint8_t bit=pDhtBitCounter[irqno]%8;
                        sensorDataBytes[irqno*5+byte] |= 1<<(7-bit);
                    } else {
                        pDhtFailureCode[irqno] = DhtFailureCode::BAD_DATA_BIT_LENGTH;
                        pDhtFailureData[irqno] = dt;
                        pDhtState[irqno] = DhtProtState::DATA_ABORT;
                        return;
                    }
                }
                ++pDhtBitCounter[irqno];
                if (pDhtBitCounter[irqno]==40) {  // after 40 bit received, data (2byte humidity, 2byte temperature, 1byte crc) complete.
                    pDhtState[irqno]=DhtProtState::DATA_OK;
                } else {
                    pDhtBeginIrqTimer[irqno]=micros();
                    pDhtState[irqno]=DhtProtState::DATA_ACQUISITION_INTRO_END;
                }
            } 
            break;
        default:
            return;
    }
}

void G_INT_ATTR ustd_dht_pirq0() {
    ustd_dht_pirq_master(0);
}
void G_INT_ATTR ustd_dht_pirq1() {
    ustd_dht_pirq_master(1);
}
void G_INT_ATTR ustd_dht_pirq2() {
    ustd_dht_pirq_master(2);
}
void G_INT_ATTR ustd_dht_pirq3() {
    ustd_dht_pirq_master(3);
}
void G_INT_ATTR ustd_dht_pirq4() {
    ustd_dht_pirq_master(4);
}
void G_INT_ATTR ustd_dht_pirq5() {
    ustd_dht_pirq_master(5);
}
void G_INT_ATTR ustd_dht_pirq6() {
    ustd_dht_pirq_master(6);
}
void G_INT_ATTR ustd_dht_pirq7() {
    ustd_dht_pirq_master(7);
}
void G_INT_ATTR ustd_dht_pirq8() {
    ustd_dht_pirq_master(8);
}
void G_INT_ATTR ustd_dht_pirq9() {
    ustd_dht_pirq_master(9);
}

void (*ustd_dht_irq_table[USTD_DHT_MAX_PIRQS])() = {ustd_dht_pirq0, ustd_dht_pirq1, ustd_dht_pirq2, ustd_dht_pirq3,
                                             ustd_dht_pirq4, ustd_dht_pirq5, ustd_dht_pirq6, ustd_dht_pirq7,
                                             ustd_dht_pirq8, ustd_dht_pirq9};



// clang - format off
/*! \brief mupplet-sensor temperature and humidity with DHT11/22

The temphum_dht mupplet measures temperature and humidity using a DHT11 or DHT22 sensor

#### Messages sent by temphum_dht mupplet:

| topic | message body | comment |
| ----- | ------------ | ------- |
| `<mupplet-name>/sensor/temperature` | temperature in degree celsius | Float value encoded as string |
| `<mupplet-name>/sensor/humidity` | humidity in percent | Float value encoded as string |
| `<mupplet-name>/sensor/mode` | `FAST`, `MEDIUM`, or `LONGTERM` | Integration time for sensor values |

#### Messages received by temphum_dht mupplet:

| topic | message body | comment |
| ----- | ------------ | ------- |
| `<mupplet-name>/sensor/temperature/get` | - | Causes current value to be sent. |
| `<mupplet-name>/sensor/humidity/get` | - | Causes current value to be sent. |
| `<mupplet-name>/sensor/mode/get` | - | Returns filterMode: `FAST`, `MEDIUM`, or `LONGTERM` |
| `<mupplet-name>/sensor/mode/set` | `FAST`, `MEDIUM`, or `LONGTERM` | Set integration time for illuminance values |

#### Sample code

For a complete examples see the `muwerk/examples` project.

```cpp
#define __ESP__ 1
#include "scheduler.h"
#include "mup_temphum_dht.h"

ustd::Scheduler sched;
ustd::TempHumDHT dht("myDHT",D4);

void task0(String topic, String msg, String originator) {
    if (topic == "myDHT/sensor/temperature") {
        Serial.print("Temperature: ");
        Serial.prinln(msg);  // String float
    }
}

void setup() {
    dht.begin(&sched);
    String name="myDHT";
    auto fnall = [=](String topic, String msg, String originator) {
        this->subsMsg(topic, msg, originator);
    };
    pSched->subscribe(tID, name + "/sensor/#", fnall);
}
```
*/

// clang-format on
class TempHumDHT {
  private:
    String LDR_VERSION = "0.1.0";
    Scheduler *pSched;
    int tID;
    String name;
    uint8_t port;
    uint8_t interruptIndex;
    double temperatureValue, humidityValue;
    bool bActive=false;
    unsigned long stateMachineTimeout=0;
    unsigned long lastPoll = 0;
    unsigned long startPulseStartUs = 0;
    unsigned long errs = 0;
    unsigned long oks = 0;
    unsigned long sensorPollRate = 3;
    uint8_t iPin=255;

  public:
    enum DHTType { DHT11, DHT22 };
    DHTType dhtType;
    enum FilterMode { FAST, MEDIUM, LONGTERM };
    FilterMode filterMode;
    ustd::sensorprocessor temperatureSensor = ustd::sensorprocessor(4, 600, 0.005);
    ustd::sensorprocessor humiditySensor = ustd::sensorprocessor(4, 600, 0.005);

    TempHumDHT(String name, uint8_t port, uint8_t interruptIndex, DHTType dhtType=DHTType::DHT22, FilterMode filterMode = FilterMode::MEDIUM)
        : name(name), port(port), interruptIndex(interruptIndex), dhtType(dhtType), filterMode(filterMode) {
        /*! Instantiate an DHT sensor mupplet
        @param name Name used for pub/sub messages
        @param port GPIO port with A/D converter capabilities.
        @param dhtType DHT11, DHT22
        @param filterMode FAST, MEDIUM or LONGTERM filtering of sensor values
        */

        if (interruptIndex >= 0 && interruptIndex < USTD_DHT_MAX_PIRQS) {
            iPin = digitalPinToInterrupt(port);
            attachInterrupt(iPin, ustd_dht_irq_table[interruptIndex], CHANGE);
            pDhtPortIrq[interruptIndex] = port;
            pDhtState[interruptIndex] = DhtProtState::NONE;
            setFilterMode(filterMode, true);
        }
    }

    ~TempHumDHT() {
        if (iPin!=255)
            detachInterrupt(iPin);
    }

    double getTemperature() {
        /*! Get current temperature
        @return Temperature (in degrees celsius)
        */
        return temperatureValue;
    }

    double getHumidity() {
        /*! Get current humidity
        @return Humidity (in percent)
        */
        return humidityValue;
    }

    void begin(Scheduler *_pSched) {
        /*! Start processing of A/D input from LDR */
        pSched = _pSched;

        lastPoll = 0;
        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 500);  // 500us

        auto fnall = [=](String topic, String msg, String originator) {
            this->subsMsg(topic, msg, originator);
        };
        pSched->subscribe(tID, name + "/sensor/#", fnall);
        if (iPin!=255) {
            pDhtState[interruptIndex] = DhtProtState::NONE;
            bActive = true;
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
            humiditySensor.smoothInterval = 1;
            humiditySensor.pollTimeSec = 2;
            humiditySensor.eps = 0.1;
            humiditySensor.reset();
            break;
        case MEDIUM:
            filterMode = MEDIUM;
            temperatureSensor.smoothInterval = 4;
            temperatureSensor.pollTimeSec = 30;
            temperatureSensor.eps = 0.1;
            temperatureSensor.reset();
            humiditySensor.smoothInterval = 4;
            humiditySensor.pollTimeSec = 30;
            humiditySensor.eps = 0.5;
            humiditySensor.reset();
            break;
        case LONGTERM:
        default:
            filterMode = LONGTERM;
            temperatureSensor.smoothInterval = 10;
            temperatureSensor.pollTimeSec = 600;
            temperatureSensor.eps = 0.1;
            temperatureSensor.reset();
            humiditySensor.smoothInterval = 50;
            humiditySensor.pollTimeSec = 600;
            humiditySensor.eps = 0.5;
            humiditySensor.reset();
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

    void publishHumidity() {
        char buf[32];
        sprintf(buf, "%6.2f", humidityValue);
        pSched->publish(name + "/sensor/humidity", buf);
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

    void generateStartMeasurementPulse() {
        // generate a 80us low-pulse to initiate DHT sensor measurement
        noInterrupts();
        switch (pDhtState[interruptIndex]) {
            case DhtProtState::NONE:
                pinMode(port, OUTPUT);
                digitalWrite(port,LOW);
                startPulseStartUs=micros();
                pDhtState[interruptIndex]=DhtProtState::START_PULSE_START;
                pDhtFailureCode[interruptIndex]=DhtFailureCode::OK;
                pDhtFailureData[interruptIndex]=0;
                break;
            case DhtProtState::START_PULSE_START:
                if (timeDiff(startPulseStartUs, micros()) > dhtWakeUpPulse) {  // should be >18ms (Note: original manufacturer doc is WRONG, says 2ms)
                    digitalWrite(port,HIGH);
                    delayMicroseconds(dhtInitialDelay);
                    startPulseStartUs=0;
                    pDhtState[interruptIndex]=DhtProtState::START_PULSE_END;
                    lastPoll=time(nullptr);
                    pinMode(port, INPUT_PULLUP);
                }
                break;
            default:
                break;
        }
        interrupts();
    }

    bool measurementChanged(double *tempVal, double *humiVal) {
        int crc=0;
        char msg[128];
        for (unsigned int i=0; i<4; i++) crc+=sensorDataBytes[interruptIndex*5+i];
        if (crc%256!=sensorDataBytes[interruptIndex*5+4]) {
            ++errs;
            sprintf(msg,"CRC_ERROR! (%d) Errs: %ld, Code: %d, ErrData %d, bytes:[%d,%d,%d,%d,%d]",(crc%256), errs, int(pDhtFailureCode[interruptIndex]), pDhtFailureData[interruptIndex],
                        sensorDataBytes[interruptIndex*5+0],sensorDataBytes[interruptIndex*5+1],sensorDataBytes[interruptIndex*5+2],sensorDataBytes[interruptIndex*5+3],sensorDataBytes[interruptIndex*5+4]);
            publishError(msg);
            noInterrupts();
            pDhtState[interruptIndex]=DhtProtState::NONE;
            interrupts();
            return false;

        } else {
                ++oks;
                sprintf(msg,"OK! Oks: %ld Errs: %ld, Code: %d, ErrData %d, bytes:[%d,%d,%d,%d,%d]",oks, errs, int(pDhtFailureCode[interruptIndex]), pDhtFailureData[interruptIndex],
                            sensorDataBytes[interruptIndex*5+0],sensorDataBytes[interruptIndex*5+1],sensorDataBytes[interruptIndex*5+2],sensorDataBytes[interruptIndex*5+3],sensorDataBytes[interruptIndex*5+4]);
                //publishError(msg);
                int t=((sensorDataBytes[interruptIndex*5+2]&0x7f)<<8) | sensorDataBytes[interruptIndex*5+3];
                if (sensorDataBytes[interruptIndex*5+2] & 0x80) t = t * (-1);
                *tempVal=(double)t/10.0;
                int h=((sensorDataBytes[interruptIndex*5+0]) << 8) | sensorDataBytes[interruptIndex*5+1];
                *humiVal=(double)h/10.0;
                return true;
        }
    }

    void loop() {
        double tempVal, humiVal;
        double errratio;
        DhtProtState curState;
        if (bActive) {
            noInterrupts();
            curState=pDhtState[interruptIndex];
            interrupts();
            switch (curState) {
                case DhtProtState::NONE:
                    if (time(nullptr)-lastPoll>sensorPollRate) {
                        generateStartMeasurementPulse();
                        lastPoll=time(nullptr);
                    }
                    break;
                case DhtProtState::START_PULSE_START:
                    generateStartMeasurementPulse();
                    break;
                case DhtProtState::DATA_OK:
                    if (measurementChanged(&tempVal, &humiVal)) {
                        if (temperatureSensor.filter(&tempVal)) {
                            temperatureValue = tempVal;
                            publishTemperature();
                        }
                        if (humiditySensor.filter(&humiVal)) {
                            humidityValue = humiVal;
                            publishHumidity();
                        }
                    }
                    ++oks;
                    noInterrupts();
                    pDhtState[interruptIndex]=DhtProtState::NONE;
                    interrupts();
                    break;
                case DhtProtState::DATA_ABORT:
                    // State machine error!
                    char msg[128];
                    errratio=(double)errs/(errs+oks)*100.0;
                    ++errs;
                    sprintf(msg,"Errs: %ld, err-rate: %6.2f Code: %d, Data %d",errs, errratio, pDhtFailureCode[interruptIndex], pDhtFailureData[interruptIndex]);
                    publishError(msg);
                    noInterrupts();
                    pDhtState[interruptIndex]=DhtProtState::NONE;
                    interrupts();
                    break;
                default:
                   return;            
            }
        }
    }



    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/sensor/temperature/get") {
            publishTemperature();
        }
        if (topic == name + "/sensor/humidity/get") {
            publishHumidity();
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
    };
};  // TempHumDHT

}  // namespace ustd
