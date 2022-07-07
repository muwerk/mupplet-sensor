// mup_temphum_dht.h
#pragma once

#include "scheduler.h"
#include "sensors.h"

namespace ustd {

#ifdef __ESP32__
#define G_INT_ATTR IRAM_ATTR
#else
#ifdef __ESP__
#define G_INT_ATTR ICACHE_RAM_ATTR
#else
#define G_INT_ATTR
#endif
#endif

#define USTD_DHT_MAX_PIRQS (10)

enum DhtProtState {NONE, START_PULSE_START, START_PULSE_END, REPL_PULSE_START, REPL_PULSE_END, DATA_ACQUISITION, DATA_ABORT, DATA_OK };

volatile DhtProtState pDhtState[USTD_DHT_MAX_PIRQS] = {DhtProtState::NONE,DhtProtState::NONE,DhtProtState::NONE,DhtProtState::NONE,
    DhtProtState::NONE,DhtProtState::NONE,DhtProtState::NONE,DhtProtState::NONE,DhtProtState::NONE,DhtProtState::NONE};

//volatile unsigned long pDhtIrqCounter[USTD_DHT_MAX_PIRQS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
//volatile unsigned long pDhtLastIrqTimer[USTD_DHT_MAX_PIRQS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
volatile unsigned long pDhtBeginIrqTimer[USTD_DHT_MAX_PIRQS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
volatile uint8_t pDhtPortIrq[USTD_DHT_MAX_PIRQS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
volatile uint8_t pDhtBitCounter[USTD_DHT_MAX_PIRQS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
volatile uint8_t sensorDataBytes[USTD_DHT_MAX_PIRQS*5];

int signalDelta=30;  // uS count for max signal length deviation

void G_INT_ATTR ustd_dht_pirq_master(uint8_t irqno) {
    unsigned long dt;
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
                for (int i=0; i<5; i++) sensorDataBytes[USTD_DHT_MAX_PIRQS*5+i]=0;
                return;
            }
            // XXX This should never occure.
            break;
        case DhtProtState::REPL_PULSE_START:
            if (digitalRead(pDhtPortIrq[irqno])==HIGH) {
                dt=timeDiff(pDhtBeginIrqTimer[irqno], micros());
                if (dt>80-signalDelta && dt < 80+signalDelta) { // First alive signal of 80us LOW +- signalDelta uS.
                    pDhtState[irqno]=DhtProtState::REPL_PULSE_END;
                    return;
                }
                // XXX bad signal pulse
            }
            // bad port state
            break;
            

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

void (*ustd_dht_pirq_table[USTD_DHT_MAX_PIRQS])() = {ustd_dht_pirq0, ustd_dht_pirq1, ustd_dht_pirq2, ustd_dht_pirq3,
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

  public:
    enum FilterMode { FAST, MEDIUM, LONGTERM };
    enum DHTType { DHT11, DHT22 };
    FilterMode filterMode;
    DHTType dhtType;
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

        if (interruptIndex >= 0 && interruptIndex < USTD_SW_MAX_IRQS) {
            ipin = digitalPinToInterrupt(port);
            attachInterrupt(ipin, ustd_sw_irq_table[interruptIndex], CHANGE);
            pDhtPortIrq[interruptIndex] = port;
            pDhtState[interruptIndex] = DhtProtState::NONE;
            setFilterMode(filterMode, true);
        }
    }

    ~TempHumDHT() {
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
        pDhtState[interruptIndex] = DhtProtState::NONE;
        bActive = true;
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
        NoInterrupts();
        switch (pDhtState[interruptIndex]) {
            case DhtProtState::NONE:
                pinMode(port, OUTPUT);
                digitalWrite(port,LOW);
                startPulseStartUs=micros();
                pDhtState[interruptIndex]=DhtProtState::START_PULSE_START;
                break;
            case DhtProtState::START_PULSE_START:
                if timeDiff(startPulseStartUs, micros()) > 80 {
                    digitalWrite(port,HIGH);
                    startPulseStartUs=0;
                    pDhtState[interruptIndex]=DhtProtState::START_PULSE_END;
                    lastPoll=time();
                    pinMode(port, INPUT_PULLUP);
                }
                break;
            default:
                break;
        }
        Interrupts():
    }

    bool measurementChange(&tempVal, &humiVal) {
        // XXX
        return False;
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
                    if (time()-lastPoll>sensorPollRate) {
                        generateStartMeasurementPulse();
                        lastPoll=time();
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
                    Interrupts();
                    break;
                case DhtProtState::DATA_ABORT:
                    // State machine did not terminate!
                    ++errs;
                    noInterrupts();
                    pDhtState[interruptIndex]=DhtProtState::NONE;
                    Interrupts();
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
