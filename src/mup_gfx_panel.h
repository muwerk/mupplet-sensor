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
        GPIOBUS,
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
                    if (resX==128 && resY==128) {
                        pDisplayST->initR(INITR_144GREENTAB);   // 1.4" thingy?
                    }
                    if (resX==160 && resY==128) {
                        pDisplayST->initR(INITR_BLACKTAB);   // 1.8" thingy?
                    }
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

#### Messages sent by gamma_gdk101 mupplet:

messages are prefixed by `omu/<hostname>`:

| topic | message body | comment |
| ----- | ------------ | ------- |
| `<mupplet-name>/display/slot/<slot_index>`/caption | caption text | caption text for slot_index |
| `<mupplet-name>/display/slot/<slot_index>`/topic | topic | subscription topic for slot_index |
| `<mupplet-name>/display/slot/<slot_index>`/text | main slot text | text for slot_index |
| `<mupplet-name>/display/slot/<slot_index>`/format | format specifier | slot format specifier for slot_index |

#### Messages received by gfx_panel mupplet:

Need to be prefixed by `<hostname>/`:

| topic | message body | comment |
| ----- | ------------ | ------- |
| `<mupplet-name>/display/slot/<slot_index>/caption/set` | <new-caption-text> | Causes caption text of slot 0..<slots be set to <new-caption-text>. |
| `<mupplet-name>/display/slot/<slot_index>/caption/get` | | Returns the caption text of slot 0..<slots. |
| `<mupplet-name>/display/slot/<slot_index>/text/set` | <new-slot-text> | String. Causes slot text of slot 0..<slots be set to <new-slot-text>. |
| `<mupplet-name>/display/slot/<slot_index>/text/get` | | Returns the slot text of slot 0..<slots. |
| `<mupplet-name>/display/slot/<slot_index>/topic/set` | <new-subs-topic> | String. Causes new subscription of <new-subs-topic> for slot 0..<slots. |
| `<mupplet-name>/display/slot/<slot_index>/topic/get` | | Returns the subscription topic of slot 0..<slots. |
| `<mupplet-name>/display/slot/<slot_index>/format/set` | <format-spec> | String. Sets <format-spec> for slot 0..<slots. |
| `<mupplet-name>/display/slot/<slot_index>/format/get` | | Returns the format-spec of slot 0..<slots. |

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
    "layout": "S|ff",
    "topics": ["clock/timeinfo", "!hastates/sensor/temperature/state", "!hastates/sensor/netatmo_temperature2/state"],
    "captions": ["Time", "Out C", "Studio C"],
}
```

`layout` contains a string defining 1-n lines separated by |, marking display-slots by `S` for string (message as-is), 
or `I` (int),`P`(val * 100 as %),`F` (one decimal float),`D` (two decimals),`T` (three decimals) for numbers, `G` for graphical plot.
Uppercase letters generate a 64x32 slot, lowercase letters generate a 32x32 slot.
`topics` gives a list of MQTT topics that are going to be displayed. A layout `S|ff|G` has four display slots 
(line 1: large string, line 2: two small numbers), line 3: large plot and requires 4 topics and 4 captions. A topic starting with '!' creates an
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
#if defined(USTD_FEATURE_NET) && !defined(OPTION_NO_MQTT)
    ustd::Mqtt *pMqtt;
#endif

    enum SlotType {INT, FLOAT, DOUBLE, TRIPLE, PERCENT, STRING, GRAPH};
    uint32_t defaultColor = GfxDrivers::RGB(0xff,0xff,0xff);
    uint32_t defaultBgColor = GfxDrivers::RGB(0x00,0x00,0x00);
    uint16_t defaultHistLen=64;
    uint32_t defaultHistDeltaMs=24*3600*1000/64;  // 24 hours in ms for entire history
    typedef struct t_slot {
        bool isInit;
        SlotType slotType;
        uint8_t slotX;
        uint8_t slotY;
        uint8_t slotLenX;
        uint8_t slotLenY;
        uint32_t color;
        uint32_t bgColor;
        String topic;
        String caption;
        uint16_t histLen;
        uint32_t lastHistUpdate;
        uint32_t histDeltaMs;
        float *pHist;
        time_t lastUpdate;
        bool isValid;
        float val;
        String formatter;
        String textRepr;
        float deltaDir;
    } T_SLOT;
    uint16_t slots;
    T_SLOT *pSlots;

    #define GFX_MAX_SLOTS 16
    #define GFX_MAX_HIST 48
    double vals[GFX_MAX_SLOTS];
    String svals[GFX_MAX_SLOTS];
    double dirs[GFX_MAX_SLOTS];
    bool vals_init[GFX_MAX_SLOTS];
    time_t lastUpdates[GFX_MAX_SLOTS];
    double hists[GFX_MAX_SLOTS][GFX_MAX_HIST];
    
    String layout;
    String formats;
    ustd::array<String> topics;
    ustd::array<String> captions;
    ustd::array<String> layouts;
    ustd::array<String> msgs;
//    uint16_t slotsX, slotsY;
    uint32_t lastRefresh;
    bool delayedUpdate;
    
    String valid_formats=" SIPFDTG";
    
    String valid_formats_long=" SIPFDTG";
    String valid_formats_small=" sipfdtg";
    char oldTimeString[64]="";
        
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
        //_init_theme();
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
        //_init_theme();
    }

    ~GfxPanel() {
    }

  private:
    bool splitCombinedLayout(String combined_layout) {
        bool layout_valid=true;
        bool parsing=true;

        formats="";
        layout="";
        slots=0;
        String line="";

        while (parsing) {
            int ind = combined_layout.indexOf('|');
            if (ind==-1) {
                line=combined_layout;
                combined_layout="";
            } else {
                line=combined_layout.substring(0,ind);
                combined_layout=combined_layout.substring(ind+1);
            }
            for (char c : line) {
                if (valid_formats_small.indexOf(c)==-1 && valid_formats_long.indexOf(c)==-1) {
                    layout_valid=false;
                    parsing=false;
                    break;
                } else {
                    ind=valid_formats_small.indexOf(c);
                    if (ind!=-1) {
                        c=valid_formats_long[ind];
                        layout+="S";
                    } else {
                        layout+="L";
                    }
                    formats+=c;
                    ++slots;
                }
            }
            if (!layout_valid) break;
            if (combined_layout!="") {
                layout+="|";
            } else {
                parsing=false;
            }
        }

        lastRefresh=0;
        delayedUpdate=false;
        for (uint8_t i=0; i<slots; i++) {
            lastUpdates[i]=time(nullptr);
        }

        return layout_valid;
    }

    bool getConfigFromFS(String name) {
        /*! Read config from FS.
        @param name: The display's `name` and name of config file `name`.json.
        @return: True if config file was found and read, false otherwise.
        */
        String combined_layout=jf.readString(name+"/layout","ff|ff");
        if (!splitCombinedLayout(combined_layout)) {
            return false;
        }
        for (uint8_t i=0; i<slots; i++) {
            captions[i]="room";
            topics[i]="some/topic";
        }
        jf.readStringArray(name+"/topics", topics);
        jf.readStringArray(name+"/captions", captions);
        if (topics.length() != captions.length() || topics.length() != slots) {
#ifdef USE_SERIAL_DBG
            Serial.println("Error: topics, captions and layout do not match");
#endif
            return false;
        }
        return true;
    }

    bool getConfigFromLayout(String name, String combined_layout) {
        /*! Read config from FS.
        @param name: The display's `name` and name of config file `name`.json.
        @param combined_layout: The layout string, e.g. "ff|ff".
        @return: True if config file was found and read, false otherwise.
        */
        if (!splitCombinedLayout(combined_layout)) {
            return false;
        }
        if (topics.length() != captions.length() || topics.length() != slots) {
#ifdef USE_SERIAL_DBG
            Serial.println("Error: topics, captions and layout do not match");
#endif
            return false;
        }
        return true;
    }

    bool config2slot(uint16_t slot) {
        /*! Convert config to slot.
        @param slot: The slot to convert.
        @return: True if config was converted, false otherwise.
        */
        if (slot>=slots) {
            return false;
        }
        pSlots[slot].slotX=0;
        pSlots[slot].slotY=0;
        bool ok=false;
        uint16_t ind=0;
        for (auto c : layouts[slot]) {
            if (ind==slot) {
                if (c=='S') {
                    pSlots[slot].slotLenX=1;
                    pSlots[slot].slotLenY=1;
                } else if (c=='L') {
                    pSlots[slot].slotLenX=2;
                    pSlots[slot].slotLenY=1;
                } else {
                    return false;
                }
                ok=true;
                break;
            }
            if (c=='S') {
                pSlots[slot].slotX++;
                ++ind;
            } else if (c=='L') {
                pSlots[slot].slotX+=2;
                ++ind;
            } else if (c=='|') {
                pSlots[slot].slotY++;
                pSlots[slot].slotX=0;
            } else {
                return false;
            }
        }
        pSlots[slot].histLen=defaultHistLen;
        switch (formats[slot]) {
            case 'I':
                pSlots[slot].slotType=SlotType::INT;
                pSlots[slot].formatter="%d";
                break;
            case 'F':
                pSlots[slot].slotType=SlotType::FLOAT;
                pSlots[slot].formatter="%.1f";
                break;
            case 'D':
                pSlots[slot].slotType=SlotType::DOUBLE;
                pSlots[slot].formatter="%.2f";
                break;
            case 'T':
                pSlots[slot].slotType=SlotType::TRIPLE;
                pSlots[slot].formatter="%.3f";
                break;
            case 'S':
                pSlots[slot].slotType=SlotType::STRING;
                pSlots[slot].formatter="%s";
                pSlots[slot].histLen=0;
                break;
            case 'P':
                pSlots[slot].slotType=SlotType::PERCENT;
                pSlots[slot].formatter="%.1f%%";
                break;
            case 'G':
                pSlots[slot].slotType=SlotType::GRAPH;
                pSlots[slot].formatter="%s";
                break;
            default:
                return false;
        }
        if (pSlots[slot].histLen) {
            pSlots[slot].pHist=new float[pSlots[slot].histLen];
            if (pSlots[slot].pHist) {
                for (uint16_t j=0; j<pSlots[slot].histLen; j++) {
                    pSlots[slot].pHist[j]=0;
                }
            } else {
                pSlots[slot].histLen=0;
                return false;
            }
        } else {
            pSlots[slot].pHist=nullptr;
            pSlots[slot].histLen=0;
        }
        pSlots[slot].topic=topics[slot];
        pSlots[slot].caption=captions[slot];
        pSlots[slot].lastUpdate=0;
        pSlots[slot].lastHistUpdate=0;
        pSlots[slot].histDeltaMs=defaultHistDeltaMs;
        pSlots[slot].val=0.0;
        pSlots[slot].textRepr="";
        pSlots[slot].deltaDir=0.0;
        pSlots[slot].isValid=false;
        pSlots[slot].color=defaultColor;
        pSlots[slot].bgColor=defaultBgColor;
        return true;
    }

    bool shortConfig2Slots() {
        /*! Convert short config to slots.
         *  @return: True if config was valid, false otherwise.
         */
        if (formats.length() != slots) {
#ifdef USE_SERIAL_DBG
            Serial.println("Error: formats and slots number do not match");
#endif
            return false;
        }
        pSlots=new T_SLOT[slots];
        for (uint16_t i=0; i<slots; i++) {
            if (!config2slot(i)) {
                return false;
            }
        }
        return true;
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
        if (strcmp(buf,oldTimeString)) {
            strcpy(oldTimeString,buf);
            sensorUpdates("clock/timeinfo", String(buf), "self.local");
        }
        // If a sensors doesn't update values for 1 hr (3600sec), declare invalid.
        for (uint8_t i=0; i<slots; i++) {
            if (time(nullptr)-lastUpdates[i] > 3600) {
                vals_init[i]=false;
            }
        }
        if (delayedUpdate && timeDiff(lastRefresh, micros())>800000L) {
            updateDisplay(true);
        }
    }

    void commonBegin() {
        pDisplay->begin();

        auto fntsk = [=]() {
            _sensorLoop();
        };
        int tID = pSched->add(fntsk, name, 1000000);
        auto fnsub = [=](String topic, String msg, String originator) {
            this->subsMsg(topic, msg, originator);
        };
        pSched->subscribe(tID, name + "/display/#", fnsub);
        auto fnall = [=](String topic, String msg, String originator) {
            sensorUpdates(topic, msg, originator);
        };
        for (uint8_t i=0; i<slots; i++) {
            if (topics[i]!="") {
#if defined(USTD_FEATURE_NET) && !defined(OPTION_NO_MQTT)
                if (topics[i][0]=='!') {
                    topics[i]=topics[i].substring(1);
                    pMqtt->addSubscription(tID, topics[i], fnall);
#ifdef USE_SERIAL_DBG
                    Serial.print("Subscribing via MQTT: ");
                    Serial.println(topics[i]);
#endif
                } 
#endif
                if (topics[i][0]!='!') {
                    if (topics[i]!="clock/timeinfo") {  // local shortcut msg. Questionable?
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
            } else {
                // No sub for empty topics.
            }
        }
#ifdef USE_SERIAL_DBG
        Serial.print("Layout: ");
        Serial.print(layout);
        Serial.print(" formats: ");
        Serial.println(formats);
#endif  // USE_SERIAL_DBG
    }

  public:
    void setSlotCaption(uint8_t slot, String caption) {
        /*! Set the caption for a slot.
        @param slot: The slot number.
        @param caption: The caption.
        */
        captions[slot]=caption;
        if (pSlots) {
            if (pSlots[slot].isInit) {
                pSlots[slot].caption=caption;
            }
        }
        updateDisplay(true);
    }

    void publishSlotCaption(uint16_t slot) {
        /*! Publish the caption for a slot.
        @param slot: The slot number, 0..<slots.
        */
        String cap="";
        bool done=false;
        if (slot<slots) {
            if (pSlots) {
                if (pSlots[slot].isInit) {
                    cap=pSlots[slot].caption;
                    done=true;
                }
            }
            if (!done) {
                cap=captions[slot];
            }
            pSched->publish(name+"/display/slot/"+String(slot)+"/caption", cap);
        }
    }

    void setSlotText(uint16_t slot, String text) {
        /*! Set the text for a slot.
        @param slot: The slot number.
        @param text: The text.
        */
        if (slot<slots) {
            // XXX
        }
    }
    void publishSlotText(uint16_t slot) {
        /*! Publish the text for a slot.
        @param slot: The slot number, 0..<slots.
        */
        if (slot<slots) {
            // pSched->publish(name+"/display/slot/"+String(slot), xxx[slot]);
        }
    }

    void setSlotTopic(uint16_t slot, String topic) {
        /*! Set the topic for a slot.
        @param slot: The slot number.
        @param topic: The topic.
        */
    }
    void publishSlotTopic(uint16_t slot) {
        /*! Publish the topic for a slot.
        @param slot: The slot number, 0..<slots.
        */
    }
    void setSlotFormat(uint16_t slot, String format) {
        /*! Set the format for a slot.
        @param slot: The slot number.
        @param format: The format.
        */
        if (slot<slots && format.length()==1) {
            formats[slot]=format[0];
            updateDisplay(true);
        }
    }
    void publishSlotFormat(uint16_t slot) {
        /*! Publish the format for a slot.
        @param slot: The slot number, 0..<slots.
        */
    }

#if defined(USTD_FEATURE_NET) && !defined(OPTION_NO_MQTT)
    void begin(ustd::Scheduler *_pSched, ustd::Mqtt *_pMqtt) {
        /*! Activate display and begin receiving MQTT updates for the display slots

        @param _pSched Pointer to the muwerk scheduler
        @param _pMqtt Pointer to munet mqtt object, used to subscribe to mqtt topics defined in `'display-name'.json` file.
        */
        pSched = _pSched;
        pMqtt = _pMqtt;
#else
    void begin(ustd::Scheduler *_pSched) {
        /*! Activate display and begin receiving updates for the display slots

        @param _pSched Pointer to the muwerk scheduler
        */
        pSched = _pSched;
#endif
        getConfigFromFS(name);
        commonBegin();
        updateDisplay();
    }

#if defined(USTD_FEATURE_NET) && !defined(OPTION_NO_MQTT)
    void begin(ustd::Scheduler *_pSched, ustd::Mqtt *_pMqtt, String combined_layout, ustd::array<String> _topics, ustd::array<String> _captions) {
        /*! Activate display and begin receiving MQTT updates for the display slots

        @param _pSched Pointer to the muwerk scheduler
        @param _pMqtt Pointer to munet mqtt object, used to subscribe to mqtt topics defined in `'display-name'.json` file.
        @param combined_layout The layout string, e.g. "ff|ff" (two lines, two short floats each).
        @param _topics std::array<String> of topics to subscribe to. The number of topics must match the number of captions and the number of slot-qualifiers in the combined_layout string.
        @param _captions std::array<String> of captions to display for the topics. The number of captions must match the number of topics and the number of slot-qualifiers in the combined_layout string.
        */
        pSched = _pSched;
        pMqtt = _pMqtt;
#else
    void begin(ustd::Scheduler *_pSched, String combined_layout, ustd::array<String> _topics, ustd::array<String> _captions) {
        /*! Activate display and begin receiving updates for the display slots

        @param _pSched Pointer to the muwerk scheduler
        @param combined_layout The layout string, e.g. "ff|ff" (two lines, two short floats each).
        @param _topics std::array<String> of topics to subscribe to. The number of topics must match the number of captions and the number of slot-qualifiers in the combined_layout string.
        @param _captions std::array<String> of captions to display for the topics. The number of captions must match the number of topics and the number of slot-qualifiers in the combined_layout string.
        */
        pSched = _pSched;
#endif
        for (auto t : _topics) {
            topics.add(t);
        }
        for (auto c : _captions) {
            captions.add(c);
        }

        getConfigFromLayout(name, combined_layout);
        commonBegin();
        updateDisplay(true);
    }

#if defined(USTD_FEATURE_NET) && !defined(OPTION_NO_MQTT)
    void begin(ustd::Scheduler *_pSched, ustd::Mqtt *_pMqtt, String combined_layout, uint16_t _slots, const char *_topics[], const char *_captions[]) {
        /*! Activate display and begin receiving MQTT updates for the display slots

        @param _pSched Pointer to the muwerk scheduler
        @param _pMqtt Pointer to munet mqtt object, used to subscribe to mqtt topics defined in `'display-name'.json` file.
        @param combined_layout The layout string, e.g. "ff|ff" (two lines, two short floats each).
        @param _slots Number of slots to use, must be array dimension of both _captions and topics.
        @param _topics const char *[] of topics to subscribe to. The number of topics must match the number of captions and the number of slot-qualifiers in the combined_layout string.
        @param _captions const char *[] of captions to display for the topics. The number of captions must match the number of topics and the number of slot-qualifiers in the combined_layout string.
        */
        pSched = _pSched;
        pMqtt = _pMqtt;
#else
    void begin(ustd::Scheduler *_pSched, String combined_layout, uint16_t _slots, const char *_topics[], const char *_captions[]) {
        /*! Activate display and begin receiving updates for the display slots

        @param _pSched Pointer to the muwerk scheduler
        @param combined_layout The layout string, e.g. "ff|ff" (two lines, two short floats each).
        @param _slots Number of slots to use, must be array dimension of both _captions and topics.
        @param _topics const char *[] of topics to subscribe to. The number of topics must match the number of captions and the number of slot-qualifiers in the combined_layout string.
        @param _captions const char *[] of captions to display for the topics. The number of captions must match the number of topics and the number of slot-qualifiers in the combined_layout string.
        */
        pSched = _pSched;
#endif
        for (uint16_t i=0; i<_slots; i++) {
            String s=_topics[i];
            topics.add(s);
            String c=_captions[i];
            captions.add(c);
        }
        getConfigFromLayout(name, combined_layout);
        commonBegin();
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

    void updateCell(uint8_t slotX, uint8_t slotY, String msg, double hist[], String caption, double arrowDir=0.0, bool large=false) {
        uint8_t x0=0, y0=0, x1=0, y1=0, xa=0, ya=0;
        uint8_t xm0, ym0, xm1,ym1;
        String bold;
        x0=slotX*64+14;
        y0=slotY*32+3;
        x1=slotX*64+14;
        y1=slotY*32+29;
        xa=slotX*64+5;
        ya=slotY*32+14;
        xm0=slotX*64+1;
        ym0=slotY*32+1;
        xm1=(slotX+1)*64-2; if (large) xm1+=64;
        ym1=(slotY+1)*32-2;
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
        if (msg!="graph") {
            pDisplay->setFont(&FreeSans12pt7b);
            pDisplay->setTextColor(pDisplay->RGB(0xff,0xff,0xff));
            pDisplay->setTextSize(1);
            pDisplay->setCursor(x1,y1);
            pDisplay->println(msg);
            pDisplay->setCursor(x1+1,y1);
            pDisplay->println(msg);
            // arrow
            if (arrowDir != 0.0) {
                drawArrow(xa,ya,(arrowDir>0.0),8,3,7);
            }
        } else {
            double min=100000, max=-100000;
            for (uint16_t x=0; x<GFX_MAX_HIST; x++) {
                if (hist[x]>max) max=hist[x];
                if (hist[x]<min) min=hist[x];
            }
            double deltaY=max-min;
            if (deltaY<0.0001) deltaY=1;
            double deltaX=(double)(xm1-xm0)/(double)(GFX_MAX_HIST);
            int lx0,ly0,lx1,ly1;
            int gHeight=(ym1-ym0)-11; // font size of caption.
            for (uint16_t i=1; i<GFX_MAX_HIST; i++) {
                lx0=xm0+(int)((double)(i-1)*deltaX); lx1=xm0+(int)((double)i*deltaX);
                ly0=ym1-(int)((hist[i-1]-min)/deltaY*(double)(gHeight));
                ly1=ym1-(int)((hist[i]-min)/deltaY*(double)(gHeight));
                uint32_t col;
                if (ly1<ly0) col=pDisplay->RGB(0xff,0x80,0x80);
                else {
                    if (ly1==ly0) col=pDisplay->RGB(0xc0,0xc0,0xc0);
                    else col=pDisplay->RGB(0x80,0x80,0xff);
                }
                pDisplay->drawLine(lx0, ly0, lx1, ly1, col);
            }
        }
    }

    void updateDisplay(bool forceUpdate=false) {
        String bold;
        if (!forceUpdate && (delayedUpdate || timeDiff(lastRefresh, micros()) < 1000000L)) {
            delayedUpdate=true;
            return;
        }
        lastRefresh=micros();
        delayedUpdate=false;
        bool updated=false;
        pDisplay->clearDisplay();
        uint32_t lineColor = pDisplay->RGB(0x80,0x80,0x80);
        uint8_t sx, sy, ind, ly;
        sx=0, sy=0; ind=0, ly=0;
        pDisplay->drawLine(0,ly,resX-1,ly, lineColor);
        for (char c : layout) {
            switch (c) {
                case 'L':
                    updateCell(sx,sy, msgs[ind], hists[ind], captions[ind], dirs[ind], true);
                    sx+=2;
                    ++ind;
                    updated=true;
                    break;
                case 'S':
                    updateCell(sx,sy, msgs[ind], hists[ind], captions[ind], dirs[ind], false);
                    updated=true;
                    sx+=1;
                    ++ind;
                    break;
                case '|':
                    sx=0;
                    sy+=1;
                    ly+=32;
                    pDisplay->drawLine(0,ly,resX-1,ly, lineColor);
                    break;
                default:
                    break;
            }
        }
        ly+=32;
        if (ly==resY) --ly;
        pDisplay->drawLine(0,ly,resX-1,ly, lineColor);

        if (updated) {
            pDisplay->display();
        }
    }

    // This is called on Sensor-update events
    void sensorUpdates(String topic, String msg, String originator) {
        char buf[64];
        bool changed;
#ifdef USE_SERIAL_DBG
        Serial.print("sensorUpdates ");
        Serial.print(topic);
        Serial.print(" -> ");
        Serial.println(msg);
#endif
        for (uint8_t i=0; i<slots; i++) {
            if (topic==topics[i]) {
                String valid_numbers="IPFDTG";
                if (valid_numbers.indexOf(formats[i])!= -1) {
                    vals[i]=msg.toFloat();
                    lastUpdates[i]=time(nullptr);
                    if (vals_init[i]==false) {
                        for (uint8_t j=0; j<GFX_MAX_HIST; j++) hists[i][j]=vals[i];
                    } else {
                        for (uint8_t j=0; j<GFX_MAX_HIST-1; j++) hists[i][j]=hists[i][j+1];
                        hists[i][GFX_MAX_HIST-1]=vals[i];
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
        changed=false;
        for (uint8_t i=0; i<slots; i++) {
            if (vals_init[i]==true) {
                msgs[i]="?Format";
                if (formats[i]=='S') {
                    if (msgs[i]!=svals[i]) changed=true;
                    msgs[i]=svals[i];
                }
                if (formats[i]=='F') {
                    sprintf(buf,"%.1f",vals[i]);
                    if (String(buf)!=msgs[i]) changed=true;
                    msgs[i]=String(buf);
                }
                if (formats[i]=='D') {
                    sprintf(buf,"%.2f",vals[i]);
                    if (String(buf)!=msgs[i]) changed=true;
                    msgs[i]=String(buf);
                }
                if (formats[i]=='T') {
                    sprintf(buf,"%.3f",vals[i]);
                    if (String(buf)!=msgs[i]) changed=true;
                    msgs[i]=String(buf);
                }
                if (formats[i]=='I') {
                    sprintf(buf,"%d",(int)vals[i]);
                    if (String(buf)!=msgs[i]) changed=true;
                    msgs[i]=String(buf);                        
                }
                if (formats[i]=='P') {
                    sprintf(buf,"%d%%",(int)(vals[i]*100));
                    if (String(buf)!=msgs[i]) changed=true;
                    msgs[i]=String(buf);                        
                }
                if (formats[i]=='G') {
                    msgs[i]=String("graph");
                    changed=true;
                }
            } else {
                msgs[i]="NaN";
                changed=true;
            }
        }
        if (changed) updateDisplay();
    }

    void subsMsg(String topic, String msg, String originator) {
        String toc=name+"/display/slot/";
        if (topic.startsWith(toc)) {
            String sub=topic.substring(toc.length());
            int16_t ind=sub.indexOf("/");
            if (ind!=-1) {
                uint16_t slot=sub.substring(0,ind).toInt();
                if (slot < slots) {
                    String action=sub.substring(ind+1);
                    if (action=="caption/get") {
                        publishSlotCaption(slot);
                    }   
                    if (action=="caption/set") {
                        setSlotCaption(slot,msg);
                    }
                    if (action=="format/get") {
                        publishSlotFormat(slot);
                    }
                    if (action=="format/set") {
                        setSlotFormat(slot,msg);
                    }
                    if (action=="topic/get") {
                        publishSlotTopic(slot);
                    }
                    if (action=="topic/set") {
                        setSlotTopic(slot,msg);
                    }
                    if (action=="text/get") {
                        publishSlotText(slot);
                    }
                    if (action=="text/set") {
                        setSlotText(slot,msg);
                    }
                }
            }
        }
    }

}; // SensorDisplay
} // namespace ustd
