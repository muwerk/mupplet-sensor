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

enum DhtProtState {NONE, START_PULSE_START, START_PULSE_END, REPL_PULSE_START, DATA_ACQUISITION_INTRO_START, DATA_ACQUISITION_INTRO_END, DATA_ACQUISITION, DATA_ABORT, DATA_OK };
enum DhtFailureCode {OK, BAD_START_PULSE_LEVEL, BAD_REPLY_PULSE_LENGTH, BAD_START_PULSE_END_LEVEL, BAD_DATA_INTRO_PULSE_LENGTH, BAD_DATA_BIT_LENGTH};

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

int signalDelta=10;  // uS count for max signal length deviation

void G_INT_ATTR ustd_dht_pirq_master(uint8_t irqno) {
    long dt;
    switch (pDhtState[irqno]) {
        case DhtProtState::NONE:
            return;
        case DhtProtState::START_PULSE_START:
            return;
        case DhtProtState::START_PULSE_END:
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
        case DhtProtState::REPL_PULSE_START:
            if (digitalRead(pDhtPortIrq[irqno])==HIGH) {
                dt=timeDiff(pDhtBeginIrqTimer[irqno], micros());
                if (dt>80-signalDelta && dt < 80+signalDelta) { // First alive signal of 80us LOW +- signalDelta uS.
                    pDhtState[irqno]=DhtProtState::DATA_ACQUISITION_INTRO_START;
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
        case DhtProtState::DATA_ACQUISITION_INTRO_START:
            if (digitalRead(pDhtPortIrq[irqno])==LOW) {
                pDhtBeginIrqTimer[irqno]=micros();
                pDhtState[irqno]=DhtProtState::DATA_ACQUISITION_INTRO_END;
            } 
        case DhtProtState::DATA_ACQUISITION_INTRO_END:
            if (digitalRead(pDhtPortIrq[irqno])==HIGH) {
                dt=timeDiff(pDhtBeginIrqTimer[irqno], micros());
                if (dt>50-signalDelta && dt < 50+signalDelta) { // Intro-marker 50us LOW +- signalDelta uS.
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
        case DhtProtState::DATA_ACQUISITION:
            if (digitalRead(pDhtPortIrq[irqno])==LOW) {
                dt=timeDiff(pDhtBeginIrqTimer[irqno], micros());
                if (dt>27-signalDelta && dt < 27+signalDelta) { // Zero-bit 26-28uS 
                   // nothing
                } else {
                    if (dt>70-signalDelta && dt < 70+signalDelta) { // One-bit 70uS
                        uint8_t byte=pDhtBitCounter[irqno]/8;
                        uint8_t bit=pDhtBitCounter[irqno]%8;
                        sensorDataBytes[irqno*5+byte] |= 1<<bit;
                    } else {
                        pDhtFailureCode[irqno] = DhtFailureCode::BAD_DATA_BIT_LENGTH;
                        pDhtFailureData[irqno] = dt;
                        pDhtState[irqno] = DhtProtState::DATA_ABORT;
                        return;
                    }
                }
                ++pDhtBitCounter[irqno];
                if (pDhtBitCounter[irqno]==40) {
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

/*
unsigned long getDhtResetpIrqCount(uint8_t irqno) {
    unsigned long count = (unsigned long)-1;
    noInterrupts();
    if (irqno < USTD_DHT_MAX_PIRQS) {
        count = pDhtIrqCounter[irqno];
        pDhtIrqCounter[irqno] = 0;
    }
    interrupts();
    return count;
}

double frequencyMultiplicator = 1000000.0;
double getDhtResetpIrqFrequency(uint8_t irqno, unsigned long minDtUs = 50) {
    double frequency = 0.0;
    noInterrupts();
    if (irqno < USTD_DHT_MAX_PIRQS) {
        unsigned long count = pDhtIrqCounter[irqno];
        unsigned long dt = timeDiff(pDhtBeginIrqTimer[irqno], pDhtLastIrqTimer[irqno]);
        if (dt > minDtUs) {                                     // Ignore small Irq flukes
            frequency = (count * frequencyMultiplicator) / dt;  // = count/2.0*1000.0000 uS / dt;
                                                                // no. of waves (count/2) / dt.
        }
        pDhtBeginIrqTimer[irqno] = 0;
        pDhtIrqCounter[irqno] = 0;
        pDhtLastIrqTimer[irqno] = 0;
    }
    interrupts();
    return frequency;
}
*/

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
            temperatureSensor.pollTimeSec = 300;
            temperatureSensor.eps = 0.1;
            temperatureSensor.reset();
            humiditySensor.smoothInterval = 4;
            humiditySensor.pollTimeSec = 300;
            humiditySensor.eps = 0.5;
            humiditySensor.reset();
            break;
        case LONGTERM:
        default:
            filterMode = LONGTERM;
            temperatureSensor.smoothInterval = 50;
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
                if (timeDiff(startPulseStartUs, micros()) > 80) {
                    digitalWrite(port,HIGH);
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
        if (crc!=sensorDataBytes[interruptIndex*5+4]) {
            ++errs;
            sprintf(msg,"CRC_ERROR! Errs: %ld, Code: %d, ErrData %d, bytes:[%d,%d,%d,%d,%d]",errs, int(pDhtFailureCode[interruptIndex]), pDhtFailureData[interruptIndex],
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
                return true;
        }
    }

    void loop() {
        double tempVal, humiVal;
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
                    ++errs;
                    sprintf(msg,"Errs: %ld, Code: %d, Data %d",errs, pDhtFailureCode[interruptIndex], pDhtFailureData[interruptIndex]);
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
