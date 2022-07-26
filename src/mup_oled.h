#pragma once

#include <Adafruit_GFX.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Adafruit_SSD1306.h>

namespace ustd {
class SensorDisplay {
  public:
    uint8_t slots;
    double vals[4]={0,0,0,0};
    ustd::array<double> dirs={0,0,0,0};
    bool vals_init[4]={false,false,false,false};
    time_t lastUpdates[4]={0,0,0,0};
    uint16_t screen_x, screen_y;
    uint8_t i2c_address;
    TwoWire *pWire;
    ustd::array<String> topics;
    ustd::array<String> captions;
    
    #define HIST_CNT 30
    double hists[4][HIST_CNT];
   
    Adafruit_SSD1306 *pDisplay;

    SensorDisplay(uint16_t screen_x, uint16_t screen_y, uint8_t i2c_address=0x3c, TwoWire* pWire=&Wire): screen_x(screen_x), screen_y(screen_y), i2c_address(i2c_address), pWire(pWire) {
        slots=0;
    }
    ~SensorDisplay() {
    }
    void begin(uint8_t _slots, ustd::array<String> &_topics, ustd::array<String> &_captions) {
        slots=_slots;
        for (uint8_t i=0; i<slots; i++) {
            topics[i]=_topics[i];
            captions[i]=_captions[i];
        }
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

    void updateCell(uint8_t index, String msg, String caption, double arrowDir=0.0, bool large=false) {
        uint8_t x0=0, y0=0, x1=0, y1=0, xa=0, ya=0;
        String bold;
        switch (index) {
            case 0:
                x0=14; y0=3;
                x1=14; y1=29;
                xa=5; ya=14;
                break;
            case 1:
                x0=78; y0=3;
                x1=78; y1=29;
                xa=69; ya=14;
                break;
            case 2:
                x0=14; y0=36;
                x1=14; y1=61;
                xa=5; ya=45;
                break;
            case 3:
                x0=78; y0=36;
                x1=78; y1=61;
                xa=69; ya=45;
                break;
            default:
                break;
        }
        // caption
        pDisplay->setFont();
        pDisplay->setTextSize(1);
        pDisplay->setCursor(x0,y0);
        pDisplay->println(caption);
        int ind=caption.indexOf(' ');
        if (ind==-1) bold=caption;
        else bold=caption.substring(0,ind);
        pDisplay->setCursor(x0+1,y0);
        pDisplay->println(bold);
        // value
        pDisplay->setFont(&FreeSans12pt7b);
        pDisplay->setTextSize(1);
        pDisplay->setCursor(x1,y1);
        pDisplay->println(msg);
        pDisplay->setCursor(x1+1,y1);
        pDisplay->println(msg);
        // arrow
        if (arrowDir != 0.0) {
            drawArrow(xa,ya,(arrowDir>0.0),8,3,7);
        }
    }

    void updateDisplay(ustd::array<String> &msgs, ustd::array<double> &dirs) {
        String bold;
        pDisplay->clearDisplay();

        switch (slots) {
            case 2:
                pDisplay->drawLine(0,0,127,0, SSD1306_WHITE);
                updateCell(0, msgs[0], captions[0], dirs[0], true);
                pDisplay->drawLine(0,33,127,33, SSD1306_WHITE);
                updateCell(2, msgs[1], captions[1], dirs[1], true);
                pDisplay->drawLine(0,63,127,63, SSD1306_WHITE);
                break;
            case 3:
                pDisplay->drawLine(0,0,127,0, SSD1306_WHITE);
                updateCell(0, msgs[0], captions[0], dirs[0], false);
                updateCell(1, msgs[1], captions[1], dirs[1], false);
                pDisplay->drawLine(0,33,127,33, SSD1306_WHITE);
                updateCell(2, msgs[2], captions[2], dirs[2], true);
                pDisplay->drawLine(0,63,127,63, SSD1306_WHITE);
                break;
            case 4:
                pDisplay->drawLine(0,0,127,0, SSD1306_WHITE);
                updateCell(0, msgs[0], captions[0], dirs[0], false);
                updateCell(1, msgs[1], captions[1], dirs[1], false);
                pDisplay->drawLine(0,33,127,33, SSD1306_WHITE);
                updateCell(2, msgs[2], captions[2], dirs[2], false);
                updateCell(3, msgs[3], captions[3], dirs[3], false);
                pDisplay->drawLine(0,63,127,63, SSD1306_WHITE);
            default:
                break;
        }
       pDisplay->display();
    }

    // This is called on Sensor-update events
    void sensorUpdates(String topic, String msg, String originator) {
        ustd::array<String> msgs;
        char buf[64];
        bool disp_update=false;
#ifdef USE_SERIAL_DBG
        Serial.print("sensorUpdates ");
        Serial.print(topic);
        Serial.print(" ");
        Serial.println(msg);
#endif
        for (uint8_t i=0; i<slots; i++) {
            if (topic==topics[i]) {
#ifdef USE_SERIAL_DBG
        Serial.println("Updating: ");
        Serial.println(msg.toFloat());
#endif
                vals[i]=msg.toFloat();
                lastUpdates[i]=time(nullptr);
                if (vals_init[i]==false) {
                    for (uint8_t j=0; j<HIST_CNT; j++) hists[i][j]=vals[i];
                } else {
                    for (uint8_t j=0; j<HIST_CNT-1; j++) hists[i][j]=hists[i][j+1];
                    hists[i][HIST_CNT-1]=vals[i];
                }
                vals_init[i]=true;
                dirs[i]=vals[i]-hists[i][0];
                disp_update=true;
            }
        }
        if (disp_update==true) {
            for (uint8_t i=0; i<slots; i++) {
                if (vals_init[i]==true) {
                    sprintf(buf,"%.1f",vals[i]);
                    msgs[i]=String(buf);
                } else {
                    msgs[i]="NaN";
                }
            }
            updateDisplay(msgs, dirs);
        }
    }

}; // SensorDisplay
} // namespace ustd
