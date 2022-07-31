#pragma once

#include <Adafruit_GFX.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Adafruit_ST7735.h>
#include <Adafruit_SSD1306.h>

ustd::jsonfile jf;

namespace ustd {

class GfxDrivers {
  public:
    enum DisplayType {
        SSD1306, 
        ST7735
        };
    enum BusType {
        GPIBUS,
        I2CBUS,
        SPIBUS
    };

    String name;
    DisplayType displayType;
    uint16_t resX, resY;
    uint8_t i2cAddress;
    TwoWire *pWire;
    uint8_t csPin, dcPin, rstPin;
    BusType busType;
    bool validDisplay;
    Adafruit_ST7735 *pDisplayST;   // Subclasses of Adafruit_GFX, doesn't help much, an Interface would be really handy.
    Adafruit_SSD1306 *pDisplaySSD; // Inheritance is C++ language's design failure no.1
                                   // So un-inheritance.

    GfxDrivers(String name, DisplayType displayType, uint16_t resX, uint16_t resY, uint8_t i2cAddress, TwoWire *pWire=&Wire):
        name(name), displayType(displayType), resX(resX), resY(resY), i2cAddress(i2cAddress), pWire(pWire)
    {   
        if (displayType==DisplayType::SSD1306) {
            validDisplay=true;
            busType=BusType::I2CBUS;
        } else {
            validDisplay=false;
        }
    }

    GfxDrivers(String name, DisplayType displayType, uint16_t resX, uint16_t resY, uint8_t csPin, uint8_t dcPin, uint8_t rstPin=-1):
        name(name), displayType(displayType), resX(resX), resY(resY), csPin(csPin), dcPin(dcPin), rstPin(rstPin)
    {   
        if (displayType==DisplayType::ST7735) {
            validDisplay=true;
            busType=BusType::SPIBUS;
        } else {
            validDisplay=false;
        }
    }

    ~GfxDrivers() {
    }

    void begin() {
        if (validDisplay) {
            switch (displayType) {
                case DisplayType::SSD1306:
                    pDisplaySSD=new Adafruit_SSD1306(resX, resY, pWire); 
                    pDisplaySSD->begin(SSD1306_SWITCHCAPVCC, i2cAddress);
                    pDisplaySSD->clearDisplay();
                    pDisplaySSD->setTextWrap(false);
                    pDisplaySSD->setTextColor(SSD1306_WHITE); 
                    pDisplaySSD->cp437(true);
                    break;
                case DisplayType::ST7735:
                    pDisplayST=new Adafruit_ST7735(csPin, dcPin, rstPin); 
                    //pDisplayST->initR(INITR_BLACKTAB);
                    pDisplayST->initR(INITR_144GREENTAB);   // 1.4" thingy?
                    pDisplayST->setTextWrap(false);
                    pDisplayST->fillScreen(ST77XX_BLACK);
                    pDisplayST->cp437(true);
                    break;
            }
        }
    }

    static uint32_t RGB(uint8_t red, uint8_t green, uint8_t blue) {
        uint32_t rgb=(((uint32_t)red)<<16)+(((uint32_t)green)<<8)+((uint32_t)blue);
        return rgb;
    }

    static void splitRGB(uint32_t rgb, uint8_t *pRed, uint8_t *pGreen, uint8_t *pBlue) {
        *pRed=(uint8_t)((rgb>>16)&0xff);
        *pGreen=(uint8_t)((rgb>>8)&0xff);
        *pBlue=(uint8_t)(rgb&0xff);
    }

    uint16_t rgbColor(uint8_t red, uint8_t green, uint8_t blue) {
        switch (displayType) {
            case DisplayType::SSD1306: // Black or white
                if ((red!=0) || (green!=0) || (blue!=0)) return 1;
                else return 0;
            case DisplayType::ST7735: // RGB565 standard
                return (((red & 0xf8)<<8) + ((green & 0xfc)<<3)+(blue>>3));
            default:
                return 0;
        } 
    }

    uint16_t rgbColor(uint32_t rgb) {
        uint8_t red, green, blue;
        splitRGB(rgb, &red, &green, &blue);
        return rgbColor(red, green, blue);
    }

    void clearDisplay() {
        if (validDisplay) {
            switch (displayType) {
                case DisplayType::SSD1306:
                    pDisplaySSD->clearDisplay();
                    break;
                case DisplayType::ST7735:
                    pDisplayST->fillScreen(ST77XX_BLACK);
                    break;
                default:
                    break;
            }
        }
    }

    void drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint32_t rgb) {
        if (validDisplay) {
            switch (displayType) {
                case DisplayType::SSD1306:
                    pDisplaySSD->drawLine(x0,y0,x1,y1,rgbColor(rgb));
                    break;
                case DisplayType::ST7735:
                    pDisplayST->drawLine(x0,y0,x1,y1,rgbColor(rgb));
                    break;
                default:
                    break;
            }
        }
    }

    void setFont(const GFXfont *gfxFont = NULL) {
        if (validDisplay) {
            switch (displayType) {
                case DisplayType::SSD1306:
                    pDisplaySSD->setFont(gfxFont);
                    break;
                case DisplayType::ST7735:
                    pDisplayST->setFont(gfxFont);
                    break;
                default:
                    break;
            }
        }
    }

    void setTextColor(uint32_t rgb) {
        if (validDisplay) {
            switch (displayType) {
                case DisplayType::SSD1306:
                    pDisplaySSD->setTextColor(rgbColor(rgb));
                    break;
                case DisplayType::ST7735:
                    pDisplayST->setTextColor(rgbColor(rgb));
                    break;
                default:
                    break;
            }
        }
    }

    void setTextSize(uint16_t textSize) {
        if (validDisplay) {
            switch (displayType) {
                case DisplayType::SSD1306:
                    pDisplaySSD->setTextSize(textSize);
                    break;
                case DisplayType::ST7735:
                    pDisplayST->setTextSize(textSize);
                    break;
                default:
                    break;
            }
        }
    }

    void setCursor(uint16_t x, uint16_t y) {
        if (validDisplay) {
            switch (displayType) {
                case DisplayType::SSD1306:
                    pDisplaySSD->setCursor(x,y);
                    break;
                case DisplayType::ST7735:
                    pDisplayST->setCursor(x,y);
                    break;
                default:
                    break;
            }
        }
    }
    
    void println(String &text) {
        if (validDisplay) {
            switch (displayType) {
                case DisplayType::SSD1306:
                    pDisplaySSD->println(text);
                    break;
                case DisplayType::ST7735:
                    pDisplayST->println(text);
                    break;
                default:
                    break;
            }
        }
    }

    void display() {
        if (validDisplay) {
            switch (displayType) {
                case DisplayType::SSD1306:
                    pDisplaySSD->display();
                    break;
                case DisplayType::ST7735:
                    break;
                default:
                    break;
            }
        }
    }
};

    // clang - format off
/*! \brief gfx_panel mupplet for oled and tft panels to display sensor values

The mup_gfx_panel mupplet supports SSD1306 oled and ST7735 tft style displays.

<img src="https://github.com/muwerk/mupplet-sensor/blob/master/extras/tft.png" width="30%"
height="30%"> Hardware: ST7735 tft display.

<img src="https://github.com/muwerk/mupplet-sensor/blob/master/extras/oled.png" width="30%"
height="30%"> Hardware: SSD1306 oled display.

#### Sample code
```cpp
#include "ustd_platform.h"
#include "scheduler.h"
#include "net.h"
#include "mqtt.h"
#include "ota.h"
#include "jsonfile.h"

#include "mup_gfx_panelxx.h"

void appLoop();

ustd::Scheduler sched(10, 16, 32);
ustd::Net net(LED_BUILTIN);
ustd::Mqtt mqtt;
ustd::Ota ota;

// requires a display config file "display1.json"
ustd::GfxPanel displayTft("display1", ustd::GfxDrivers::DisplayType::ST7735, 128, 128, D4, D3, (uint8_t)-1, "DE");
// requires a display config file "display2.json"
ustd::GfxPanel displayOled("display2", ustd::GfxDrivers::DisplayType::SSD1306, 128,64, 0x3c, &Wire, "DE");

void setup() {
    con.begin(&sched);
    net.begin(&sched);
    mqtt.begin(&sched);
    ota.begin(&sched);
    displayTft.begin(&sched,&mqtt);
    displayOled.begin(&sched,&mqtt);
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
parameter to the object instantiation, "display1" or "display2" in the example above. The file `display1,2.json` should contain:

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
class GfxPanel {
  public:
    String name;
    GfxDrivers::DisplayType displayType;
    uint16_t resX, resY;
    uint8_t i2cAddress;
    TwoWire *pWire;
    uint8_t csPin, dcPin, rstPin;
    String locale;

    GfxDrivers *pDisplay;
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
    ustd::array<String> topics;
    ustd::array<String> captions;
    
    #define HIST_CNT 30
    double hists[4][HIST_CNT];
    String valid_formats=" SIPFDT";
    char oldbuf[64]="";
        
    GfxPanel(String name, GfxDrivers::DisplayType displayType, uint16_t resX, uint16_t resY, uint8_t i2cAddress, TwoWire *pWire=&Wire, String locale="C"): 
        name(name), displayType(displayType), resX(resX), resY(resY), i2cAddress(i2cAddress), pWire(pWire), locale(locale)
        /*!
        @param name The display's `name`. A file `name`.json must exist in the format above to define the display slots and corresponding MQTT messages.
        @param displayType A GfxDrivers::DisplayType, e.g. GfxDrivers::DisplayType::SSD1306, GfxDrivers::DisplayType::ST7735.
        @param resX Horizontal resolution.
        @param resY Vertical resolution.
        @param i2cAddress I2C address of display
        @param pWire Pointer to a TwoWire I2C structure, default is &Wire.
        @param locale Locale for date strings, current 'C' (default) or 'DE'.
        */
    {   
        switch (displayType) {
            case GfxDrivers::DisplayType::SSD1306:
                pDisplay=new GfxDrivers(name, displayType, resX, resY, i2cAddress, pWire);
                break;
            default:
                pDisplay=nullptr;
        }
        _init_theme();
    }

    GfxPanel(String name, GfxDrivers::DisplayType displayType, uint16_t resX, uint16_t resY, uint8_t csPin, uint8_t dcPin, uint8_t rstPin=-1, String locale="C"):
        name(name), displayType(displayType), resX(resX), resY(resY), csPin(csPin), dcPin(dcPin), rstPin(rstPin), locale(locale)
        /*!
        @param name The display's `name`. A file `name`.json must exist in the format above to define the display slots and corresponding MQTT messages.
        @param displayType A DisplayDriver DisplayType, e.g. GfxDrivers::DisplayType::SSD1306, GfxDrivers::DisplayType::ST7735.
        @param resX Horizontal resolution.
        @param resY Vertical resolution.
        @param csPin CS Pin for SPI.
        @param dcPin DC Pin for SPI.
        @param rstPin RST Pin for SPI. (default unused, -1)
        @param locale Locale for date strings, current 'C' (default) or 'DE'.
        */
    {   
        switch (displayType) {
            case GfxDrivers::DisplayType::ST7735:
                pDisplay=new GfxDrivers(name, displayType, resX, resY, csPin, dcPin, rstPin);
                break;
            default:
                pDisplay=nullptr;
        }
        _init_theme();
    }

    ~GfxPanel() {
    }

  private:
    void _init_theme() {
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
  
    void _sensorLoop() {
        const char *weekDays[]={"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
        const char *wochenTage[]={"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
        const char *pDay;
        struct tm *plt;
        time_t t;
        char buf[64];
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

        pDisplay->begin();

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

    uint16_t Rgb565(uint8_t red, uint8_t green, uint8_t blue) {
        return (((red & 0xf8)<<8) + ((green & 0xfc)<<3)+(blue>>3));
    } 

  private:
    void drawArrow(uint16_t x, uint16_t y, bool up=true, uint16_t len=8, uint16_t wid=3, int16_t delta_down=0) {
        uint32_t red=pDisplay->RGB(0xff,0,0);
        uint32_t blue=pDisplay->RGB(0,0,0xff);
        if (up) {
            pDisplay->drawLine(x,y+len, x, y, red);
            pDisplay->drawLine(x+1,y+len, x+1, y, red);
            pDisplay->drawLine(x, y, x-wid, y+wid, red);
            pDisplay->drawLine(x, y, x+wid, y+wid, red);
            pDisplay->drawLine(x+1, y, x-wid+1, y+wid, red);
            pDisplay->drawLine(x+1, y, x+wid+1, y+wid, red);
        } else {
            pDisplay->drawLine(x,y+len+delta_down, x, y+delta_down, blue);
            pDisplay->drawLine(x+1,y+len+delta_down, x+1, y+delta_down, blue);
            pDisplay->drawLine(x, y+len+delta_down, x-wid, y+len-wid+delta_down, blue);
            pDisplay->drawLine(x, y+len+delta_down, x+wid, y+len-wid+delta_down, blue);
            pDisplay->drawLine(x+1, y+len+delta_down, x-wid+1, y+len-wid+delta_down, blue);
            pDisplay->drawLine(x+1, y+len+delta_down, x+wid+1, y+len-wid+delta_down, blue);
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
        pDisplay->setTextColor(pDisplay->RGB(0xbb,0xbb,0xbb));
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
        pDisplay->setTextColor(pDisplay->RGB(0xff,0xff,0xff));
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
        uint32_t lineColor = pDisplay->RGB(0x80,0x80,0x80);
        if (layout=="L|L") {
            pDisplay->drawLine(0,0,127,0, lineColor);
            updateCell(0, msgs[0], captions[0], dirs[0], true);
            pDisplay->drawLine(0,33,127,33, lineColor);
            updateCell(2, msgs[1], captions[1], dirs[1], true);
            pDisplay->drawLine(0,63,127,63, lineColor);
            updated=true;
        }
        if (layout=="SS|L") {
            pDisplay->drawLine(0,0,127,0, lineColor);
            updateCell(0, msgs[0], captions[0], dirs[0], false);
            updateCell(1, msgs[1], captions[1], dirs[1], false);
            pDisplay->drawLine(0,33,127,33, lineColor);
            updateCell(2, msgs[2], captions[2], dirs[2], true);
            pDisplay->drawLine(0,63,127,63, lineColor);
            updated=true;
        }
        if (layout=="L|SS") {
            pDisplay->drawLine(0,0,127,0, lineColor);
            updateCell(0, msgs[0], captions[0], dirs[0], true);
            pDisplay->drawLine(0,33,127,33, lineColor);
            updateCell(2, msgs[1], captions[1], dirs[1], false);
            updateCell(3, msgs[2], captions[2], dirs[2], false);
            pDisplay->drawLine(0,63,127,63, lineColor);
            updated=true;
        }
        if (layout=="SS|SS") {
            pDisplay->drawLine(0,0,127,0, lineColor);
            updateCell(0, msgs[0], captions[0], dirs[0], false);
            updateCell(1, msgs[1], captions[1], dirs[1], false);
            pDisplay->drawLine(0,33,127,33, lineColor);
            updateCell(2, msgs[2], captions[2], dirs[2], false);
            updateCell(3, msgs[3], captions[3], dirs[3], false);
            pDisplay->drawLine(0,63,127,63, lineColor);
            updated=true;
        }
        if (layout=="SS") {
            // pDisplay->drawLine(0,0,127,0, lineColor);
            updateCell(0, msgs[0], captions[0], dirs[0], false);
            updateCell(1, msgs[1], captions[1], dirs[1], false);
            // pDisplay->drawLine(0,33,127,33, lineColor);
            // updateCell(2, msgs[2], captions[2], dirs[2], true);
            // pDisplay->drawLine(0,63,127,63, lineColor);
            updated=true;
        }
        if (layout=="L") {
            // pDisplay->drawLine(0,0,127,0, lineColor);
            updateCell(0, msgs[0], captions[0], dirs[0], true);
            // updateCell(1, msgs[1], captions[1], dirs[1], false);
            // pDisplay->drawLine(0,33,127,33, lineColor);
            // updateCell(2, msgs[2], captions[2], dirs[2], true);
            // pDisplay->drawLine(0,63,127,63, lineColor);
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
