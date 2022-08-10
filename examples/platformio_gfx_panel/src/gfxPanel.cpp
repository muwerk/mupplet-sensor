#include "ustd_platform.h"
#include "scheduler.h"
#include "jsonfile.h"

#include "../../../src/mup_gfx_panel.h"

void appLoop();

ustd::Scheduler sched(10, 16, 32);

// make sure that two json files exist: display1.json and display2.json, corresponding to the names of the GfxPanel s.
#ifdef __ESP32__
//                                                                                     CS  DC  RST
ustd::GfxPanel displayTft("display1", ustd::GfxDrivers::DisplayType::ST7735, 160, 128,  5, 16, 17, "DE");
#else
//                                                                                     CS  DC  RST
//ustd::GfxPanel displayTft("display1", ustd::GfxDrivers::DisplayType::ST7735, 128, 128, D4, D3, (uint8_t)-1, "DE");
ustd::GfxPanel displayOled("display2", ustd::GfxDrivers::DisplayType::SSD1306, 128,64, 0x3c, &Wire, "DE");
#endif

void setup() {
#ifdef USE_SERIAL_DBG
    Serial.begin(115200);
    Serial.println("Starting up...");
#endif  // USE_SERIAL_DBG


#ifdef __ESP32__
Serial.println("Starting display");
    displayTft.begin(&sched);
Serial.println("Display started");
#else
    const char *topics[]={"clock/timeinfo", "random/var"};
    const char *captions[]={"Time", "Random"};
    displayOled.begin(&sched,"S|T",2,topics,captions);
#endif
    sched.add(appLoop, "main", 1000000);
}


void appLoop() {
    static double sensor_data=0.0;
    sched.publish("random/var", String(sensor_data));
#ifdef USE_SERIAL_DBG
    Serial.println(sensor_data);
#endif
    sensor_data+=0.001;
}

// Never add code to this loop, use appLoop() instead.
void loop() {
    sched.loop();
}
