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
    const char *topics[]={"sensor/data1", "sensor/data1", "sensor/data2"};
    const char *captions[]={"data 1 _N", "data 1 _N", "Sensor data 2 _N"};
    displayOled.begin(&sched,"dg|T",3,topics,captions); // "f: small slot .2 float data1, g: small plot data1, next line: large .3 float for data2.

#endif
    sched.add(appLoop, "main", 1000000);
}


void appLoop() {
    static double sensor_data1=0.0;
    static double sensor_data2=0.0;
    sched.publish("sensor/data1", String(sensor_data1,3));
    sched.publish("sensor/data2", String(sensor_data1,3));
    sensor_data1+=(random(10)-5)/70.0;
    if (sensor_data1<0.0) sensor_data1=0.0;
    if (sensor_data1>1.0) sensor_data1=1.0;
    sensor_data2+=(random(10)-5)/50.0;
    if (sensor_data2<0.0) sensor_data2=0.0;
    if (sensor_data2>1.0) sensor_data2=1.0;
}

// Never add code to this loop, use appLoop() instead.
void loop() {
    sched.loop();
}
