#pragma once

#include <Adafruit_GFX.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Adafruit_SSD1306.h>

namespace ustd {
class SensorDisplay {
  public:
    double t1,t2, d1, d2;
    int t1_val=0;
    int t2_val=0;
    time_t lastUpdate_t1=0;
    time_t lastUpdate_t2=0;
    uint16_t screen_x, screen_y;
    uint8_t i2c_address;
    TwoWire *pWire;

    #define HIST_CNT 4
    double hist_t1[HIST_CNT];
    double hist_t2[HIST_CNT];
   
    Adafruit_SSD1306 *pDisplay;

    SensorDisplay(uint16_t screen_x, uint16_t screen_y, uint8_t i2c_address=0x3c, TwoWire* pWire=&Wire): screen_x(screen_x), screen_y(screen_y), i2c_address(i2c_address), pWire(pWire) {
    }
    ~SensorDisplay() {
    }
    void begin() {
        pDisplay=new Adafruit_SSD1306(screen_x, screen_y, pWire); 
        pDisplay->begin(SSD1306_SWITCHCAPVCC, i2c_address);
        pDisplay->clearDisplay();
        pDisplay->setTextColor(SSD1306_WHITE); // Draw white text
        pDisplay->cp437(true);         // Use full 256 char 'Code Page 437' font

        updateDisplay("init","init",0.0,0.0);
    }

    void drawArrow(uint16_t x, uint16_t y, bool up=true, uint16_t len=8, uint16_t wid=3, int16_t delta_down=0) {
        if (up) {
            pDisplay->drawLine(x,y+len, x, y, SSD1306_WHITE);
            pDisplay->drawLine(x+1,y+len, x+1, y, SSD1306_WHITE);
            pDisplay->drawLine(x, y, x-wid, y+wid, SSD1306_WHITE);
            pDisplay->drawLine(x, y, x+wid, y+wid, SSD1306_WHITE);
            pDisplay->drawLine(x+1, y, x-wid+1, y+wid, SSD1306_WHITE);
            pDisplay->drawLine(x+1, y, x+wid+1, y+wid, SSD1306_WHITE);
        } else {
            pDisplay->drawLine(x,y+len+delta_down, x, y+delta_down, SSD1306_WHITE);
            pDisplay->drawLine(x+1,y+len+delta_down, x+1, y+delta_down, SSD1306_WHITE);
            pDisplay->drawLine(x, y+len+delta_down, x-wid, y+len-wid+delta_down, SSD1306_WHITE);
            pDisplay->drawLine(x, y+len+delta_down, x+wid, y+len-wid+delta_down, SSD1306_WHITE);
            pDisplay->drawLine(x+1, y+len+delta_down, x-wid+1, y+len-wid+delta_down, SSD1306_WHITE);
            pDisplay->drawLine(x+1, y+len+delta_down, x+wid+1, y+len-wid+delta_down, SSD1306_WHITE);
        }
    }

    void updateDisplay(String msg1, String msg2, double d1, double d2) {
        pDisplay->clearDisplay();

        pDisplay->drawLine(0,0,127,0, SSD1306_WHITE);
        pDisplay->setFont();
        pDisplay->setTextSize(1);
        pDisplay->setCursor(14,3);
        pDisplay->println("In  DHT-1 14:07");
        pDisplay->setCursor(15,3);
        pDisplay->println("In        14:07");

        pDisplay->setFont(&FreeSans12pt7b);
        pDisplay->setTextSize(1);      // Normal 1:1 pixel scale
        pDisplay->setCursor(14,29);
        pDisplay->println(msg1);
        pDisplay->setCursor(15,29);
        pDisplay->println(msg1);

        pDisplay->drawLine(0,33,127,33, SSD1306_WHITE);
        pDisplay->setFont();
        pDisplay->setTextSize(1);
        pDisplay->setCursor(14,36);
        pDisplay->println("Pressure");
        pDisplay->setCursor(15,36);
        pDisplay->println("Pressure (hPA)");

        pDisplay->setFont(&FreeSans12pt7b);
        pDisplay->setTextSize(1);      // Normal 1:1 pixel scale
        pDisplay->setCursor(14,61);
        pDisplay->println(msg2);
        pDisplay->setCursor(15,61);
        pDisplay->println(msg2);
        pDisplay->drawLine(0,63,127,63, SSD1306_WHITE);

        if (d1!=0.0) {
            drawArrow(5,14,(d1>0.0),8,3,7);
        }
        if (d2!=0.0) {
            drawArrow(5,45,(d2>0.0),8,3,7);
        }
        pDisplay->display();

    }

    // This is called on Sensor-update events
    void sensorUpdates(String topic, String msg, String originator) {
        int disp_update=0;
        char buf1[64],buf2[64];
        if (topic == "myBMP180/sensor/temperature") {
            t1 = msg.toFloat();
            lastUpdate_t1=time(nullptr);
            if (!t1_val) {
                for (int i=0; i<HIST_CNT; i++) hist_t1[i]=t1;
            } else {
                for (int i=0; i<HIST_CNT-1; i++) hist_t1[i]=hist_t1[i+1];
                hist_t1[HIST_CNT-1]=t1;
            }
            d1=t1-hist_t1[0];
            t1_val=1;
            disp_update=1;
        }
        if (topic == "myBMP180/sensor/deltaaltitude") {
            t2 = msg.toFloat();
            lastUpdate_t2=time(nullptr);
            if (!t2_val) {
                for (int i=0; i<HIST_CNT; i++) hist_t2[i]=t2;
            } else {
                for (int i=0; i<HIST_CNT-1; i++) hist_t2[i]=hist_t2[i+1];
                hist_t2[HIST_CNT-1]=t2;
            }
            d2=t2-hist_t2[0];
            t2_val=1;
            disp_update=1;
        }
        if (disp_update) {
            if (t1_val) {
                sprintf(buf1,"%.1f C",t1);
            } else {
                sprintf(buf1,"%s", "NaN");
            }
            if (t2_val) {
                sprintf(buf2,"%.3f",t2);
            } else {
                sprintf(buf2,"%s", "NaN");
            }
            updateDisplay(buf1,buf2,d1,d2);
        }
    }

}; // SensorDisplay
} // namespace ustd
