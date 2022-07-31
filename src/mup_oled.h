#pragma once

#include <Adafruit_GFX.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Adafruit_SSD1306.h>

ustd::jsonfile jf;

namespace ustd {
    // clang - format off
/*! \brief mupplet-sensor 128x64 oled display for sensor value monitoring

The mup_oled mupplet supports SSD1306 style 128x64 (or 128x32) OLED displays

<img src="https://github.com/muwerk/mupplet-sensor/blob/master/extras/oled.png" width="30%"
height="30%"> Hardware: SSD1306 I2C Oled display.

#### Sample code
```cpp
#include "ustd_platform.h"
#include "scheduler.h"
#include "net.h"
#include "mqtt.h"
#include "ota.h"
#include "jsonfile.h"

#include "mup_oled.h"

void appLoop();

ustd::Scheduler sched(10, 16, 32);
ustd::Net net(LED_BUILTIN);
ustd::Mqtt mqtt;
ustd::Ota ota;

ustd::SensorDisplay display("display", 128,64,0x3c);

void setup() {
    con.begin(&sched);
    net.begin(&sched);
    mqtt.begin(&sched);
    ota.begin(&sched);
    display.begin(&sched,&mqtt);
    sched.add(appLoop, "main", 1000000);
}

void appLoop() {
}

// Never add code to this loop, use appLoop() instead.
void loop() {
    sched.loop();
}
```

The data area of the ESP32 or ESP8266 must contain a json file with the name of SensorDisplay Object given as first
parameter to the object instantiation, "display" in the example above. The file `display.json` should contain:

```json
{
    "layout": "S|FF",
    "topics": ["clock/timeinfo", "!hastates/sensor/temperature/state", "!hastates/sensor/netatmo_temperature2/state"],
    "captions": ["Time", "Out C", "Studio C"],
}
```

`layout` contains a string defining up to two lines separated by |, marking display-slots by `S` for string (message as-is), 
or `I` (int),`P`(val * 100 as %),`F` (one decimal float),`D` (two decimals),`T` (three decimals) for numbers. 
Each line can have one (large display slot) or two (small slot) entries, 
e.g.: `S|FF`. A single line (without '|') and one or two slots is valid too, e.g. `S` or `FD`. 
`topics` gives a list of MQTT topics that are going to be displayed. A layout `S|FF` has three display slots 
(line 1: large string, line 2: two small numbers) and requires 3 topics and 3 captions. A topic starting with '!' creates an
external MQTT subscription (which allows displaying values from external devices), while topics without starting '!' subscribe
to device-local messages only via muwerk's scheduler.
A special topic `clock/timeinfo` is provided by this mupplet and displays day of week and time.
`captions` are the small-print titles for each display slot. Default is bold font. '_' switches between bold and normal 
font. 
*/
// clang-format on
class SensorDisplay {
  public:
    String name;
    ustd::Scheduler *pSched;
    ustd::Mqtt *pMqtt;
    uint8_t slots;
    String layout;
    String formats;
    double vals[4]={0,0,0,0};
    String svals[4]={"", "", "", ""};
    double dirs[4]={0,0,0,0};
    bool vals_init[4]={false,false,false,false};
    time_t lastUpdates[4]={0,0,0,0};
    uint16_t screen_x, screen_y;
    uint8_t i2c_address;
    TwoWire *pWire;
    String locale;
    ustd::array<String> topics;
    ustd::array<String> captions;
    
    #define HIST_CNT 30
    double hists[4][HIST_CNT];
    String valid_formats=" SIPFDT";
   
    Adafruit_SSD1306 *pDisplay;

    SensorDisplay(String name, uint16_t screen_x, uint16_t screen_y, uint8_t i2c_address=0x3c, TwoWire* pWire=&Wire, String locale="C"): 
                  name(name), screen_x(screen_x), screen_y(screen_y), i2c_address(i2c_address), pWire(pWire), locale(locale) {
        /*! Instantiate an sensor display mupplet
        @param name The display's `name`. A file `name`.json must exist in the format above to define the display slots and corresponding MQTT messages.
        @param screen_x Horizontal resolution, is currently always 128.
        @param screen_y Vertical resolution, is currently always 64, (maybe 32 works with format secifiers L or SS.)
        @param i2c_address i2c address of the SSD1306 display, usually 0x3c.
        @param pWire Pointer to a TwoWire i2c instance.
        @param locale Locale for time, current 'C' or 'DE'.
        */
        for (uint8_t i=0; i<4; i++) {
            captions[i]="room";
            topics[i]="some/topic";
            lastUpdates[i]=time(nullptr);
        }

        String combined_layout=jf.readString(name+"/layout","FF|FF");
        layout="";
        formats="";
        bool layout_valid=true;
        int ind = combined_layout.indexOf('|');
        if (ind==-1) {
            if (combined_layout.length()>2) {
                layout_valid=false;
            } else {
                if (combined_layout.length()==1) {
                    layout="L";
                } else {
                    layout="SS";
                }
                formats=layout;
            }
        } else {
            String line1=combined_layout.substring(0,ind);
            String line2=combined_layout.substring(ind+1);
            if (line1.length()>2 || line2.length()>2) {
                layout_valid=false;
            } else {
                if (line1.length()==1) {
                    layout="L";
                } else {
                    layout="SS";
                }
                formats=line1;
                layout+="|";
                if (line2.length()==1) {
                    layout+="L";
                } else {
                    layout+="SS";
                }
                formats+=line2;
            }
        }
        if (!layout_valid || (layout!="SS|SS" && layout!="L|SS" && layout!="L|L" && layout !="SS|L" && layout != "L" && layout != "SS")) {
            layout="SS|SS";
#ifdef USE_SERIAL_DBG
            Serial.print("Unsupported layout: ");
            Serial.print(layout);
            Serial.println(" please use (64x128 displays) 'SS|SS' or 'L|SS' or 'L|L' or 'SS|L' or (32x128 displays) 'L' or 'SS' ");
#endif  // USE_SERIAL_DBG
        }
        while (formats.length()<4) {
            formats += " ";
        }
        for (unsigned int i=0; i<formats.length(); i++) {
            if (valid_formats.indexOf(formats[i])==-1) {
                formats[i]='S';
#ifdef USE_SERIAL_DBG
                Serial.print("Unsupported formats string: ");
                Serial.print(formats);
                Serial.println(" should only contain 'I' for int, 'F' for single decimal, 'S' for string, 'D' for double decimals, 'T' for triple dec., or ' '.");
#endif  // USE_SERIAL_DBG
            }
        }

        jf.readStringArray(name+"/topics", topics);
        jf.readStringArray(name+"/captions", captions);
        slots=topics.length();
        if (captions.length()<slots) {
            slots=captions.length();
#ifdef USE_SERIAL_DBG
                Serial.print("Error: fewer captions than topics, reducing!");
#endif  // USE_SERIAL_DBG
        }
    }

    ~SensorDisplay() {
    }
  private:
    void _sensorLoop() {
        const char *weekDays[]={"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
        const char *wochenTage[]={"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
        const char *pDay;
        struct tm *plt;
        time_t t;
        char buf[64];
        static char oldbuf[64]="";
        // scruffy old c time functions 
        t=time(nullptr);
        plt=localtime(&t);
        if (locale=="DE") pDay=wochenTage[plt->tm_wday];
        else pDay=weekDays[plt->tm_wday];
        // sprintf(buf,"%s %02d. %02d:%02d",pDay,plt->tm_mday, plt->tm_hour, plt->tm_min);
        sprintf(buf,"%s %02d:%02d",pDay, plt->tm_hour, plt->tm_min);
        if (strcmp(buf,oldbuf)) {
            strcpy(oldbuf,buf);
            sensorUpdates("clock/timeinfo", String(buf), "self.local");
        }
        // If a sensors doesn't update values for 1 hr (3600sec), declare invalid.
        for (uint8_t i=0; i<slots; i++) {
            if (time(nullptr)-lastUpdates[i] > 3600) {
                vals_init[i]=false;
            }
        }
    }
  public:
    void begin(ustd::Scheduler *_pSched, ustd::Mqtt *_pMqtt) {
        /*! Activate display and begin receiving MQTT updates for the display slots

        @param _pSched Pointer to the muwerk scheduler
        @param _pMqtt Pointer to munet mqtt object, used to subscribe to mqtt topics defined in `'display-name'.json` file.
        */
        pSched = _pSched;
        pMqtt = _pMqtt;
        pDisplay=new Adafruit_SSD1306(screen_x, screen_y, pWire); 
        pDisplay->begin(SSD1306_SWITCHCAPVCC, i2c_address);
        pDisplay->clearDisplay();
        pDisplay->setTextColor(SSD1306_WHITE); // Draw white text
        pDisplay->cp437(true);         // Use full 256 char 'Code Page 437' font

        auto fntsk = [=]() {
            _sensorLoop();
        };
        int tID = pSched->add(fntsk, "oled", 1000000);

        auto fnall = [=](String topic, String msg, String originator) {
            sensorUpdates(topic, msg, originator);
        };
        for (uint8_t i=0; i<slots; i++) {
            if (topics[i][0]=='!') {
                topics[i]=topics[i].substring(1);
                pMqtt->addSubscription(tID, topics[i], fnall);
#ifdef USE_SERIAL_DBG
                Serial.print("Subscribing via MQTT: ");
                Serial.println(topics[i]);
#endif
            } else {
                if (topics[i]!="clock/timeinfo") {  // local shortcut msg.
                    pSched->subscribe(tID, topics[i], fnall);
#ifdef USE_SERIAL_DBG
                    Serial.print("Subscribing internally: ");
                    Serial.println(topics[i]);
#endif
                } else {
#ifdef USE_SERIAL_DBG
                    Serial.print("Internal topic: ");
                    Serial.println(topics[i]);
#endif

                }
            }
        }
    }

  private:
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
        bool isBold=true;
        String first="";
        String second="";
        for (unsigned int i=0; i<caption.length(); i++) {
            if (caption[i]=='_') {
                isBold=!isBold;
                continue;
            }
            first+=caption[i];
            if (isBold) {
                second+=caption[i];
            } else {
                second+=' ';
            }
        }
        pDisplay->setCursor(x0,y0);
        pDisplay->println(first);
        pDisplay->setCursor(x0+1,y0);
        pDisplay->println(second);
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

    void updateDisplay(ustd::array<String> &msgs) {
        String bold;
        bool updated=false;
        pDisplay->clearDisplay();
        if (layout=="L|L") {
            pDisplay->drawLine(0,0,127,0, SSD1306_WHITE);
            updateCell(0, msgs[0], captions[0], dirs[0], true);
            pDisplay->drawLine(0,33,127,33, SSD1306_WHITE);
            updateCell(2, msgs[1], captions[1], dirs[1], true);
            pDisplay->drawLine(0,63,127,63, SSD1306_WHITE);
            updated=true;
        }
        if (layout=="SS|L") {
            pDisplay->drawLine(0,0,127,0, SSD1306_WHITE);
            updateCell(0, msgs[0], captions[0], dirs[0], false);
            updateCell(1, msgs[1], captions[1], dirs[1], false);
            pDisplay->drawLine(0,33,127,33, SSD1306_WHITE);
            updateCell(2, msgs[2], captions[2], dirs[2], true);
            pDisplay->drawLine(0,63,127,63, SSD1306_WHITE);
            updated=true;
        }
        if (layout=="L|SS") {
            pDisplay->drawLine(0,0,127,0, SSD1306_WHITE);
            updateCell(0, msgs[0], captions[0], dirs[0], true);
            pDisplay->drawLine(0,33,127,33, SSD1306_WHITE);
            updateCell(2, msgs[1], captions[1], dirs[1], false);
            updateCell(3, msgs[2], captions[2], dirs[2], false);
            pDisplay->drawLine(0,63,127,63, SSD1306_WHITE);
            updated=true;
        }
        if (layout=="SS|SS") {
            pDisplay->drawLine(0,0,127,0, SSD1306_WHITE);
            updateCell(0, msgs[0], captions[0], dirs[0], false);
            updateCell(1, msgs[1], captions[1], dirs[1], false);
            pDisplay->drawLine(0,33,127,33, SSD1306_WHITE);
            updateCell(2, msgs[2], captions[2], dirs[2], false);
            updateCell(3, msgs[3], captions[3], dirs[3], false);
            pDisplay->drawLine(0,63,127,63, SSD1306_WHITE);
            updated=true;
        }
        if (layout=="SS") {
            // pDisplay->drawLine(0,0,127,0, SSD1306_WHITE);
            updateCell(0, msgs[0], captions[0], dirs[0], false);
            updateCell(1, msgs[1], captions[1], dirs[1], false);
            // pDisplay->drawLine(0,33,127,33, SSD1306_WHITE);
            // updateCell(2, msgs[2], captions[2], dirs[2], true);
            // pDisplay->drawLine(0,63,127,63, SSD1306_WHITE);
            updated=true;
        }
        if (layout=="L") {
            // pDisplay->drawLine(0,0,127,0, SSD1306_WHITE);
            updateCell(0, msgs[0], captions[0], dirs[0], true);
            // updateCell(1, msgs[1], captions[1], dirs[1], false);
            // pDisplay->drawLine(0,33,127,33, SSD1306_WHITE);
            // updateCell(2, msgs[2], captions[2], dirs[2], true);
            // pDisplay->drawLine(0,63,127,63, SSD1306_WHITE);
            updated=true;
        }
        if (updated) {
            pDisplay->display();
        } else {
#ifdef USE_SERIAL_DBG
            Serial.print("Can't draw unsupported layout: ");
            Serial.println(layout);
#endif            
        }
    }

    // This is called on Sensor-update events
    void sensorUpdates(String topic, String msg, String originator) {
        ustd::array<String> msgs;
        char buf[64];
#ifdef USE_SERIAL_DBG
        Serial.print("sensorUpdates ");
        Serial.println(msg);
#endif
        for (uint8_t i=0; i<slots; i++) {
            if (topic==topics[i]) {
                String valid_numbers="IPFDT";
                if (valid_numbers.indexOf(formats[i])!= -1) {
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
                }
                if (formats[i]=='S') {
                    lastUpdates[i]=time(nullptr);
                    vals_init[i]=true;
                    svals[i]=msg;
                }
            }
        }
        for (uint8_t i=0; i<slots; i++) {
            if (vals_init[i]==true) {
                msgs[i]="?Format";
                if (formats[i]=='S') {
                    msgs[i]=svals[i];
                }
                if (formats[i]=='F') {
                    sprintf(buf,"%.1f",vals[i]);
                    msgs[i]=String(buf);
                }
                if (formats[i]=='D') {
                    sprintf(buf,"%.2f",vals[i]);
                    msgs[i]=String(buf);
                }
                if (formats[i]=='T') {
                    sprintf(buf,"%.3f",vals[i]);
                    msgs[i]=String(buf);
                }
                if (formats[i]=='I') {
                    sprintf(buf,"%d",(int)vals[i]);
                    msgs[i]=String(buf);                        
                }
                if (formats[i]=='P') {
                    sprintf(buf,"%d%%",(int)(vals[i]*100));
                    msgs[i]=String(buf);                        
                }
            } else {
                msgs[i]="NaN";
            }
        }
        updateDisplay(msgs);
    }

}; // SensorDisplay
} // namespace ustd
