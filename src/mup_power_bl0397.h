// power_bl0937.h
#pragma once

#include "scheduler.h"

namespace ustd {

#if defined(__ESP32__) || defined(__ESP__)
#define G_INT_ATTR IRAM_ATTR
#else
#define G_INT_ATTR
#endif

#define USTD_MAX_BLP_PIRQS (10)

volatile unsigned long blp_pirpcounter[USTD_MAX_BLP_PIRQS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
volatile unsigned long blp_plastIrqTimer[USTD_MAX_BLP_PIRQS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
volatile unsigned long blp_pbeginIrqTimer[USTD_MAX_BLP_PIRQS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

void G_INT_ATTR ustd_blp_pirp_master(uint8_t irqno) {
    unsigned long curr = micros();
    if (blp_pbeginIrqTimer[irqno] == 0)
        blp_pbeginIrqTimer[irqno] = curr;
    else
        ++blp_pirpcounter[irqno];
    blp_plastIrqTimer[irqno] = curr;
}

void G_INT_ATTR ustd_blp_pirp0() {
    ustd_blp_pirp_master(0);
}
void G_INT_ATTR ustd_blp_pirp1() {
    ustd_blp_pirp_master(1);
}
void G_INT_ATTR ustd_blp_pirp2() {
    ustd_blp_pirp_master(2);
}
void G_INT_ATTR ustd_blp_pirp3() {
    ustd_blp_pirp_master(3);
}
void G_INT_ATTR ustd_blp_pirp4() {
    ustd_blp_pirp_master(4);
}
void G_INT_ATTR ustd_blp_pirp5() {
    ustd_blp_pirp_master(5);
}
void G_INT_ATTR ustd_blp_pirp6() {
    ustd_blp_pirp_master(6);
}
void G_INT_ATTR ustd_blp_pirp7() {
    ustd_blp_pirp_master(7);
}
void G_INT_ATTR ustd_blp_pirp8() {
    ustd_blp_pirp_master(8);
}
void G_INT_ATTR ustd_blp_pirp9() {
    ustd_blp_pirp_master(9);
}

void (*ustd_blp_pirp_table[USTD_MAX_BLP_PIRQS])() = {ustd_blp_pirp0, ustd_blp_pirp1, ustd_blp_pirp2, ustd_blp_pirp3,
                                                     ustd_blp_pirp4, ustd_blp_pirp5, ustd_blp_pirp6, ustd_blp_pirp7,
                                                     ustd_blp_pirp8, ustd_blp_pirp9};

unsigned long getBlpResetpIrqCount(uint8_t irqno) {
    unsigned long count = (unsigned long)-1;
    noInterrupts();
    if (irqno < USTD_MAX_BLP_PIRQS) {
        count = blp_pirpcounter[irqno];
        blp_pirpcounter[irqno] = 0;
    }
    interrupts();
    return count;
}

double getBlpResetpIrqFrequency(uint8_t irqno, unsigned long minDtUs = 50) {
    double frequency = 0.0;
    noInterrupts();
    if (irqno < USTD_MAX_BLP_PIRQS) {
        unsigned long count = blp_pirpcounter[irqno];
        unsigned long dt = timeDiff(blp_pbeginIrqTimer[irqno], blp_plastIrqTimer[irqno]);
        if (dt > minDtUs) {                       // Ignore small Irq flukes
            frequency = (count * 500000.0) / dt;  // = count/2.0*1000.0000 uS / dt; no.
                                                  // of waves (count/2) / dt.
        }
        blp_pbeginIrqTimer[irqno] = 0;
        blp_pirpcounter[irqno] = 0;
        blp_plastIrqTimer[irqno] = 0;
    }
    interrupts();
    return frequency;
}

bool changeBlpSELi(bool bsel, uint8_t pin_sel, uint8_t irqno) {
    digitalWrite(pin_sel, bsel);
    getBlpResetpIrqFrequency(irqno);
    return bsel;
}

/*! \brief Power, Voltage and Current sensor BL0397

The power_bl0397 mupplet measures current voltage (V), current (A) and power
consumption (W) using a chinese BL0397 sensor as used in Gosund SP-1 switches.

#### Messages sent by illuminance_ldr mupplet:

| topic | message body | comment |
| ----- | ------------ | ------- |
| `<mupplet-name>/sensor/voltage` | <current-voltage> | Current voltage (V) as string. |
|  `<mupplet-name>/sensor/current` | <current-current> | Current current (A) as string. |
|  `<mupplet-name>/sensor/power` | <current-power> | Current power usage (W) as string. |

#### Messages received by illuminance_ldr mupplet:

| topic | message body | comment |
| ----- | ------------ | ------- |
| `<mupplet-name>/sensor/state/get` | - | Causes current voltage, current, and power messages to be sent. |
| `<mupplet-name>/sensor/voltage/get` | - | Causes current voltage message to be sent. |
| `<mupplet-name>/sensor/current/get` | - | Causes current current message to be sent. |
| `<mupplet-name>/sensor/power/get` | - | Causes current power message to be sent. |
*/
class PowerBl0937 {
  public:
    String POWER_BL0937_VERSION = "0.1.0";
    Scheduler *pSched;
    int tID;

    String name;
    bool irqsAttached = false;
    uint8_t pin_CF, pin_CF1, pin_SELi;
    uint8_t irqno_CF, irqno_CF1, irqno_SELi;
    int8_t interruptIndex_CF, interruptIndex_CF1;
    bool bSELi = false;
    ustd::sensorprocessor frequencyCF = ustd::sensorprocessor(8, 600, 0.1);
    ustd::sensorprocessor frequencyCF1_I = ustd::sensorprocessor(8, 600, 0.01);
    ustd::sensorprocessor frequencyCF1_V = ustd::sensorprocessor(8, 600, 0.1);
    double CFfrequencyVal = 0.0;
    double CF1_IfrequencyVal = 0.0;
    double CF1_VfrequencyVal = 0.0;

    double voltageRenormalisation = 6.221651690201113;  // Empirical factors measured on Gosund-SP1
                                                        // to convert frequency CF to power in W.
    double currentRenormalisation =
        84.4444444444444411;                          // frequency CF1 (SELi high) to voltage (V)
    double powerRenormalization = 0.575713594581519;  // frequency CF1 (SELi low) to current (I)

    double userCalibrationPowerFactor = 1.0;
    double userCalibrationVoltageFactor = 1.0;
    double userCalibrationCurrentFactor = 1.0;

    uint8_t ipin = 255;

    PowerBl0937(String name, uint8_t pin_CF, uint8_t pin_CF1, uint8_t pin_SELi,
                int8_t interruptIndex_CF, uint8_t interruptIndex_CF1)
        : name(name), pin_CF(pin_CF), pin_CF1(pin_CF1), pin_SELi(pin_SELi),
          interruptIndex_CF(interruptIndex_CF), interruptIndex_CF1(interruptIndex_CF1) {
        /*! Creates a new instance of BL0937 based power meter
        @param name Friendly name of the meter.
        @param pin_CF BL0937 pin CF. BL0937 outputs a 50% duty PWM signal with
        frequency proportional to power usage
        @param pin_CF1 BL0937 pin CF1. BL0937 outputs a 50% duty PWM signal with
        frequency eithe proportical to voltage (SELi high) or current (SELi
        low).
        @param pin_SELi BL0937 pin SELi. If set to high, BL0937 outputs voltage
        proportional frequncy on CF1, low: current-proportional.
        @param interruptIndex_CF Should be unique interrupt index
        0..USTD_MAX_BLP_PIRQS. Used to assign a unique interrupt service routine.
        @param interruptIndex_CF1 IRQ service index for CF1, Should be unique
        interrupt index 0..USTD_MAX_BLP_PIRQS. Used to assign a unique interrupt
        service routine.
        */
    }

    ~PowerBl0937() {
        if (irqsAttached) {
            detachInterrupt(irqno_CF);
            detachInterrupt(irqno_CF1);
        }
    }

    bool begin(Scheduler *_pSched) {
        pSched = _pSched;

        pinMode(pin_CF, INPUT_PULLUP);
        pinMode(pin_CF1, INPUT_PULLUP);
        pinMode(pin_SELi, OUTPUT);
        digitalWrite(pin_SELi, bSELi);

        if (interruptIndex_CF >= 0 && interruptIndex_CF < USTD_MAX_BLP_PIRQS &&
            interruptIndex_CF1 >= 0 && interruptIndex_CF1 < USTD_MAX_BLP_PIRQS &&
            interruptIndex_CF != interruptIndex_CF1) {
#ifdef __ESP32__
            irqno_CF = digitalPinToInterrupt(pin_CF);
            irqno_CF1 = digitalPinToInterrupt(pin_CF1);
#else
            irqno_CF = digitalPinToInterrupt(pin_CF);
            irqno_CF1 = digitalPinToInterrupt(pin_CF1);
#endif
            attachInterrupt(irqno_CF, ustd_blp_pirp_table[interruptIndex_CF], CHANGE);
            attachInterrupt(irqno_CF1, ustd_blp_pirp_table[interruptIndex_CF1], CHANGE);
            irqsAttached = true;
        } else {
            return false;
        }

        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 2000000);  // uS schedule

        auto fnall = [=](String topic, String msg, String originator) {
            this->subsMsg(topic, msg, originator);
        };
        pSched->subscribe(tID, name + "/power_bl0937/#", fnall);
        return true;
    }

    void setUserCalibrationFactors(double powerFactor = 1.0, double voltageFactor = 1.0,
                                   double currentFactor = 1.0) {
        userCalibrationPowerFactor = powerFactor;
        userCalibrationVoltageFactor = voltageFactor;
        userCalibrationCurrentFactor = currentFactor;
        frequencyCF.reset();
        frequencyCF1_V.reset();
        frequencyCF1_I.reset();
    }

    void publish_CF() {
        char buf[32];
        sprintf(buf, "%6.1f", CFfrequencyVal);
        char *p1 = buf;
        while (*p1 == ' ')
            ++p1;
        pSched->publish(name + "/sensor/power", p1);
    }
    void publish_CF1_V() {
        char buf[32];
        sprintf(buf, "%5.1f", CF1_VfrequencyVal);
        char *p1 = buf;
        while (*p1 == ' ')
            ++p1;
        pSched->publish(name + "/sensor/voltage", p1);
    }
    void publish_CF1_I() {
        char buf[32];
        sprintf(buf, "%5.2f", CF1_IfrequencyVal);
        char *p1 = buf;
        while (*p1 == ' ')
            ++p1;
        pSched->publish(name + "/sensor/current", p1);
    }

    void publish() {
        publish_CF();
        publish_CF1_V();
        publish_CF1_I();
    }

    void loop() {
        double watts = getBlpResetpIrqFrequency(interruptIndex_CF) / powerRenormalization *
                       userCalibrationPowerFactor;
        if ((frequencyCF.lastVal == 0.0 && watts > 0.0) ||
            (frequencyCF.lastVal > 0.0 && watts == 0.0))
            frequencyCF.reset();
        if (watts >= 0.0 && watts < 3800) {
            if (frequencyCF.filter(&watts)) {
                CFfrequencyVal = watts;
                publish_CF();
            }
        }
        double mfreq = getBlpResetpIrqFrequency(interruptIndex_CF1);
        if (bSELi) {
            double volts = mfreq / voltageRenormalisation * userCalibrationVoltageFactor;
            if (volts < 5.0 || (volts >= 100.0 && volts < 260)) {
                if ((frequencyCF1_V.lastVal == 0.0 && volts > 0.0) ||
                    (frequencyCF1_V.lastVal > 0.0 && volts == 0.0))
                    frequencyCF1_V.reset();
                if (frequencyCF1_V.filter(&volts)) {
                    CF1_VfrequencyVal = volts;
                    publish_CF1_V();
                }
            }
        } else {
            double currents = mfreq / currentRenormalisation * userCalibrationCurrentFactor;
            if (currents >= 0.0 && currents < 16.0) {
                if ((frequencyCF1_I.lastVal == 0.0 && currents > 0.0) ||
                    (frequencyCF1_I.lastVal > 0.0 && currents == 0.0))
                    frequencyCF1_I.reset();
                if (frequencyCF1_I.filter(&currents)) {
                    CF1_IfrequencyVal = currents;
                    publish_CF1_I();
                }
            }
        }
        bSELi = changeBlpSELi(!bSELi, pin_SELi, interruptIndex_CF1);
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/sensor/state/get") {
            publish();
        } else if (topic == name + "/sensor/power/get") {
            publish_CF();
        } else if (topic == name + "/sensor/voltage/get") {
            publish_CF1_V();
        } else if (topic == name + "/sensor/current/get") {
            publish_CF1_I();
        }
    };
};  // PowerBl0937

}  // namespace ustd
