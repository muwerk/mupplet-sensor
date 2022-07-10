#define __ESP__  // or other ustd library platform define
#include "scheduler.h"
#include "mup_illuminance_ldr.h"

ustd::Scheduler sched;

ustd::IlluminanceLdr ldr("myldr", A0);

void appLoop() {
    // your code here...
}

void ldrMsg(String topic, String message, String originator) {
    if (topic=="myldr/sensor/unitilluminance") {
        // message contains the unit-illumance value as string: 0.0=dark, 1.0 max light
    }
}

void setup() {
    ldr.begin(&sched);
    int tid = sched.add(appLoop, "main",
                        1000000);  // call appLoop every 1sec (1000000us, change as you wish.)
    // Subscribe to ldr sensor messages:
    sched.subscribe(tid, "myldr/sensor/unitilluminance", ldrMsg);
}

// Never add code to this loop, use appLoop() instead.
void loop() {
    sched.loop();
}
