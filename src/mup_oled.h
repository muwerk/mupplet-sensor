#pragma once

#include <Adafruit_GFX.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Adafruit_SSD1306.h>

namespace ustd {
class SensorDisplay {
  public:
    double t1,t2,t3, d1, d2, d3;
    int t1_val=0;
    int t2_val=0;
    int t3_val=0;
    time_t lastUpdate_t1=0;
    time_t lastUpdate_t2=0;
    time_t lastUpdate_t3=0;
    uint16_t screen_x, screen_y;
    uint8_t i2c_address;
    TwoWire *pWire;
    String topic1,topic2, topic3, caption1, caption2, caption3, caption11, caption21, caption31;

    #define HIST_CNT 30
    double hist_t1[HIST_CNT];
    double hist_t2[HIST_CNT];
    double hist_t3[HIST_CNT];
   
    Adafruit_SSD1306 *pDisplay;

    SensorDisplay(uint16_t screen_x, uint16_t screen_y, uint8_t i2c_address=0x3c, TwoWire* pWire=&Wire): screen_x(screen_x), screen_y(screen_y), i2c_address(i2c_address), pWire(pWire) {
    }
    ~SensorDisplay() {
    }
    void begin(String _topic1, String _topic2, String _topic3, String _caption1, String _caption2, String _caption3) {
        topic1=_topic1;
        topic2=_topic2;
        topic3=_topic3;

        caption1=_caption1;
        caption2=_caption2;
        caption3=_caption3;
        
        pDisplay=new Adafruit_SSD1306(screen_x, screen_y, pWire); 
        pDisplay->begin(SSD1306_SWITCHCAPVCC, i2c_address);
        pDisplay->clearDisplay();
        pDisplay->setTextColor(SSD1306_WHITE); // Draw white text
        pDisplay->cp437(true);         // Use full 256 char 'Code Page 437' font
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

    void updateDisplay(String msg1, String msg2, String msg3, double d1, double d2, double d3) {
        int16_t ind;
        String bold;
        pDisplay->clearDisplay();

        pDisplay->drawLine(0,0,127,0, SSD1306_WHITE);
        pDisplay->setFont();
        pDisplay->setTextSize(1);
        pDisplay->setCursor(14,3);
        pDisplay->println(caption1);
        ind=caption1.indexOf(' ');
        if (ind==-1) bold=caption1;
        else bold=caption1.substring(0,ind);
        pDisplay->setCursor(15,3);
        pDisplay->println(bold);

        pDisplay->setCursor(78,3);
        pDisplay->println(caption2);
        pDisplay->setCursor(79,3);
        ind=caption2.indexOf(' ');
        if (ind==-1) bold=caption2;
        else bold=caption2.substring(0,ind);
        pDisplay->println(bold);

        pDisplay->setFont(&FreeSans12pt7b);
        pDisplay->setTextSize(1);      // Normal 1:1 pixel scale
        pDisplay->setCursor(14,29);
        pDisplay->println(msg1);
        pDisplay->setCursor(15,29);
        pDisplay->println(msg1);
        pDisplay->setCursor(78,29);
        pDisplay->println(msg2);
        pDisplay->setCursor(79,29);
        pDisplay->println(msg2);

        pDisplay->drawLine(0,33,127,33, SSD1306_WHITE);
        pDisplay->setFont();
        pDisplay->setTextSize(1);
        pDisplay->setCursor(14,36);
        pDisplay->println(caption3);
        pDisplay->setCursor(15,36);
        ind=caption3.indexOf(' ');
        if (ind==-1) bold=caption3;
        else bold=caption3.substring(0,ind);
        pDisplay->println(bold);

        pDisplay->setFont(&FreeSans12pt7b);
        pDisplay->setTextSize(1);      // Normal 1:1 pixel scale
        pDisplay->setCursor(14,61);
        pDisplay->println(msg3);
        pDisplay->setCursor(15,61);
        pDisplay->println(msg3);
        pDisplay->drawLine(0,63,127,63, SSD1306_WHITE);

        if (d1!=0.0) {
            drawArrow(5,14,(d1>0.0),8,3,7);
        }
        if (d2!=0.0) {
            drawArrow(69,14,(d1>0.0),8,3,7);
        }
        if (d3!=0.0) {
            drawArrow(5,45,(d2>0.0),8,3,7);
        }
        pDisplay->display();

    }

    // This is called on Sensor-update events
    void sensorUpdates(String topic, String msg, String originator) {
        int disp_update=0;
        char buf1[64],buf2[64],buf3[64];
        if (topic == topic1) {
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
        if (topic == topic2) {
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
        if (topic == topic3) {
            t3 = msg.toFloat();
            lastUpdate_t3=time(nullptr);
            if (!t3_val) {
                for (int i=0; i<HIST_CNT; i++) hist_t3[i]=t3;
            } else {
                for (int i=0; i<HIST_CNT-1; i++) hist_t3[i]=hist_t3[i+1];
                hist_t3[HIST_CNT-1]=t3;
            }
            d3=t3-hist_t3[0];
            t3_val=1;
            disp_update=1;
        }
        if (disp_update) {
            if (t1_val) {
                sprintf(buf1,"%.1f",t1);
            } else {
                sprintf(buf1,"%s", "NaN");
            }
            if (t2_val) {
                sprintf(buf2,"%.1f",t2);
            } else {
                sprintf(buf2,"%s", "NaN");
            }
            if (t3_val) {
                sprintf(buf3,"%.3f",t3);
            } else {
                sprintf(buf3,"%s", "NaN");
            }
            updateDisplay(buf1,buf2,buf3,d1,d2,d3);
        }
    }

}; // SensorDisplay
} // namespace ustd
