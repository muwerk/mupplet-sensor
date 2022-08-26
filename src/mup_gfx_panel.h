#pragma once

#include <Adafruit_GFX.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Adafruit_ST7735.h>
#include <Adafruit_SSD1306.h>
#include "jsonfile.h"

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
    uint32_t bgColor;
    bool isLightTheme;
    uint8_t i2cAddress;
    TwoWire *pWire;
    uint8_t csPin, dcPin, rstPin;
    BusType busType;
    bool validDisplay;
    bool useCanvas;
    bool hasBegun;
    Adafruit_ST7735 *pDisplayST;
    Adafruit_SSD1306 *pDisplaySSD;
    GFXcanvas16 *pCanvas;

    GfxDrivers(String name, DisplayType displayType, uint16_t resX, uint16_t resY, uint8_t i2cAddress, TwoWire *pWire = &Wire)
        : name(name), displayType(displayType), resX(resX), resY(resY), i2cAddress(i2cAddress), pWire(pWire) {
        pDisplayST = nullptr;
        pDisplaySSD = nullptr;
        pCanvas = nullptr;
        hasBegun = false;
        if (displayType == DisplayType::SSD1306) {
            validDisplay = true;
            busType = BusType::I2CBUS;
            bgColor = SSD1306_BLACK;
        } else {
            validDisplay = false;
        }
    }

    GfxDrivers(String name, DisplayType displayType, uint16_t resX, uint16_t resY, uint8_t csPin, uint8_t dcPin, uint8_t rstPin = -1)
        : name(name), displayType(displayType), resX(resX), resY(resY), csPin(csPin), dcPin(dcPin), rstPin(rstPin) {
        pDisplayST = nullptr;
        pDisplaySSD = nullptr;
        pCanvas = nullptr;
        hasBegun = false;
        if (displayType == DisplayType::ST7735) {
            validDisplay = true;
            busType = BusType::SPIBUS;
            bgColor = RGB(0, 0, 0);
        } else {
            validDisplay = false;
        }
    }

    ~GfxDrivers() {  // undefined behavior for delete display objects.
    }

    void begin(bool _useCanvas = false) {
        /*! Initialize the display.
         * Note: Using canvas uses a lot of memory! Use with ESP32 or better kind of chips.
         *  @param _useCanvas If true, use a canvas for drawing. If false, use the display directly.
         */
        if (hasBegun) {
#ifdef USE_SERIAL_DBG
            Serial.println("ERROR GfxDrivers::begin() - already begun");
#endif
            return;
        }
        hasBegun = true;
        useCanvas = _useCanvas;
        if (validDisplay) {
            switch (displayType) {
            case DisplayType::SSD1306:
                pDisplaySSD = new Adafruit_SSD1306(resX, resY, pWire);
                pDisplaySSD->begin(SSD1306_SWITCHCAPVCC, i2cAddress);
                pDisplaySSD->clearDisplay();
                pDisplaySSD->setTextWrap(false);
                pDisplaySSD->setTextColor(SSD1306_WHITE);
                pDisplaySSD->cp437(true);
                break;
            case DisplayType::ST7735:
                // pDisplayST->initR(INITR_BLACKTAB);
                if (resX == 128 && resY == 128) {
                    pDisplayST = new Adafruit_ST7735(csPin, dcPin, rstPin);
                    pDisplayST->initR(INITR_144GREENTAB);  // 1.4" thingy?
                } else if (resX == 128 && resY == 160) {
                    pDisplayST = new Adafruit_ST7735(csPin, dcPin, rstPin);
                    pDisplayST->initR(INITR_BLACKTAB);  // 1.8" thingy?
                } else {
#ifdef USE_SERIAL_DBG
                    Serial.println("ERROR GfxDrivers::begin() - unknown/invalid display resolution");
#endif
                    hasBegun = false;
                    return;
                }
                if (useCanvas) {
                    pCanvas = new GFXcanvas16(resX, resY);
                    pCanvas->setTextWrap(false);
                    pCanvas->fillScreen(ST77XX_BLACK);
                    pCanvas->cp437(true);
                } else {
                    pDisplayST->setTextWrap(false);
                    pDisplayST->fillScreen(ST77XX_BLACK);
                    pDisplayST->cp437(true);
                }
                break;
            }
        }
    }

    void setBGColor(uint32_t _bgColor) {
        uint8_t r, g, b;
        splitRGB(_bgColor, &r, &g, &b);
        if (r + b + g > 256 + 128) {
            isLightTheme = true;
        } else {
            isLightTheme = false;
        }
        bgColor = _bgColor;
    }

    static uint32_t RGB(uint8_t red, uint8_t green, uint8_t blue) {
        uint32_t rgb = (((uint32_t)red) << 16) + (((uint32_t)green) << 8) + ((uint32_t)blue);
        return rgb;
    }

    static void splitRGB(uint32_t rgb, uint8_t *pRed, uint8_t *pGreen, uint8_t *pBlue) {
        *pRed = (uint8_t)((rgb >> 16) & 0xff);
        *pGreen = (uint8_t)((rgb >> 8) & 0xff);
        *pBlue = (uint8_t)(rgb & 0xff);
    }

    uint16_t rgbColor(uint8_t red, uint8_t green, uint8_t blue) {
        switch (displayType) {
        case DisplayType::SSD1306:  // Black or white
            if (RGB(red, green, blue) == bgColor) {
                if (isLightTheme)
                    return SSD1306_WHITE;
                else
                    return SSD1306_BLACK;
            } else {
                if (isLightTheme)
                    return SSD1306_BLACK;
                else
                    return SSD1306_WHITE;
            }
        case DisplayType::ST7735:  // RGB565 standard
            return (((red & 0xf8) << 8) + ((green & 0xfc) << 3) + (blue >> 3));
        default:
            return 0;
        }
    }

    uint16_t rgbColor(uint32_t rgb) {
        uint8_t red, green, blue;
        splitRGB(rgb, &red, &green, &blue);
        return rgbColor(red, green, blue);
    }

    void clearDisplay(uint32_t bgColor) {
        if (validDisplay) {
            switch (displayType) {
            case DisplayType::SSD1306:
                pDisplaySSD->clearDisplay();
                pDisplaySSD->fillRect(0, 0, resX, resY, bgColor);
                break;
            case DisplayType::ST7735:
                if (useCanvas) {
                    pCanvas->fillScreen(bgColor);
                    // pCanvas->fillRect(0, 0, resX, resY, bgColor);
                } else {
                    pDisplayST->fillScreen(bgColor);
                    // pDisplayST->fillRect(0, 0, resX, resY, bgColor);
                }
                break;
            default:
                break;
            }
        }
    }

    void drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint32_t rgb) {
        if (validDisplay) {
            if (!hasBegun) {
#ifdef USE_SERIAL_DBG
                Serial.println("ERROR GfxDrivers::drawLine() - not begun");
#endif
                return;
            }
            switch (displayType) {
            case DisplayType::SSD1306:
                pDisplaySSD->drawLine(x0, y0, x1, y1, rgbColor(rgb));
                break;
            case DisplayType::ST7735:
                if (useCanvas && pCanvas) {
                    if (y0 == y1) {
                        pCanvas->drawFastHLine(x0, y0, x1 - x0, rgbColor(rgb));
                    } else {
                        pCanvas->drawLine(x0, y0, x1, y1, rgbColor(rgb));
                    }
                } else {
                    if (pDisplayST) {
                        pDisplayST->drawLine(x0, y0, x1, y1, rgbColor(rgb));
                    }
                }
                break;
            default:
                break;
            }
        }
    }

    void fillRect(uint16_t x0, uint16_t y0, uint16_t lx, uint16_t ly, uint32_t rgb) {
        if (validDisplay) {
            switch (displayType) {
            case DisplayType::SSD1306:
                pDisplaySSD->fillRect(x0, y0, lx, ly, rgbColor(rgb));
                break;
            case DisplayType::ST7735:
                if (useCanvas) {
                    pCanvas->fillRect(x0, y0, lx, ly, rgbColor(rgb));
                } else {
                    pDisplayST->fillRect(x0, y0, lx, ly, rgbColor(rgb));
                }
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
                if (useCanvas) {
                    pCanvas->setFont(gfxFont);
                } else {
                    pDisplayST->setFont(gfxFont);
                }
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
                if (useCanvas) {
                    pCanvas->setTextColor(rgbColor(rgb));
                } else {
                    pDisplayST->setTextColor(rgbColor(rgb));
                }
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
                if (useCanvas) {
                    pCanvas->setTextSize(textSize);
                } else {
                    pDisplayST->setTextSize(textSize);
                }
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
                pDisplaySSD->setCursor(x, y);
                break;
            case DisplayType::ST7735:
                if (useCanvas) {
                    pCanvas->setCursor(x, y);
                } else {
                    pDisplayST->setCursor(x, y);
                }
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
                if (useCanvas) {
                    pCanvas->println(text);
                } else {
                    pDisplayST->println(text);
                }
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
                if (useCanvas) {
                    pDisplayST->drawRGBBitmap(0, 0, pCanvas->getBuffer(), resX, resY);
                }
                break;
            default:
                break;
            }
        }
    }
};

// clang-format off
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
    uint16_t slotResX, slotResY;
    uint8_t i2cAddress;
    TwoWire *pWire;
    uint8_t csPin, dcPin, rstPin;
    String locale;
    bool active;

    GfxDrivers *pDisplay;
    ustd::Scheduler *pSched;
#if defined(USTD_FEATURE_NETWORK) && !defined(OPTION_NO_MQTT)
    ustd::Mqtt *pMqtt;
#endif

    enum SlotType { NUMBER,
                    TEXT,
                    GRAPH };
    uint32_t defaultColor;
    uint32_t defaultBgColor;
    uint32_t defaultSeparatorColor;
    uint32_t defaultAccentColor;
    uint32_t defaultIncreaseColor;
    uint32_t defaultConstColor;
    uint32_t defaultDecreaseColor;
    uint16_t defaultHistLen;
    uint32_t defaultHistSampleRateMs;  // 24 hours in ms for entire history
    typedef struct t_slot {
        bool isInit;
        bool isValid;
        bool hasChanged;
        time_t lastUpdate;
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
        uint32_t histSampleRateMs;
        float *pHist;
        bool histInit;

        float currentValue;
        String currentText;
        uint8_t digits;
        float scalingFactor;
        float offset;
        float deltaDir;
        uint32_t lastFrame;
        uint32_t frameRate;
    } T_SLOT;
    uint16_t slots;
    T_SLOT *pSlots;

    String layout;
    String formats;
    ustd::array<String> topics;
    ustd::array<String> captions;
    ustd::array<String> msgs;

    uint32_t displayFrameRateMs;
    uint32_t lastRefresh;
    uint32_t minUpdateIntervalMs;

    String valid_formats = " SIPFDTG";
    String valid_formats_long = " SIPFDTG";
    String valid_formats_small = " sipfdtg";
    char oldTimeString[64] = "";

    GfxPanel(String name, GfxDrivers::DisplayType displayType, uint16_t resX, uint16_t resY, uint8_t i2cAddress, TwoWire *pWire = &Wire, String locale = "C")
        : name(name), displayType(displayType), resX(resX), resY(resY), i2cAddress(i2cAddress), pWire(pWire), locale(locale)
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
            pDisplay = new GfxDrivers(name, displayType, resX, resY, i2cAddress, pWire);
            break;
        default:
            pDisplay = nullptr;
        }
        _common_init();
    }

    GfxPanel(String name, GfxDrivers::DisplayType displayType, uint16_t resX, uint16_t resY, uint8_t csPin, uint8_t dcPin, uint8_t rstPin = -1, String locale = "C")
        : name(name), displayType(displayType), resX(resX), resY(resY), csPin(csPin), dcPin(dcPin), rstPin(rstPin), locale(locale)
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
            pDisplay = new GfxDrivers(name, displayType, resX, resY, csPin, dcPin, rstPin);
            break;
        default:
            pDisplay = nullptr;
        }
        _common_init();
    }

    ~GfxPanel() {
        if (pDisplay) {
            delete pDisplay;
        }
        if (pSlots) {
            for (uint16_t i = 0; i < slots; i++) {
                if (pSlots[i].pHist) {
                    delete pSlots[i].pHist;
                }
            }
        }
        if (pSlots) {
            delete[] pSlots;
        }
    }

  private:
    uint32_t relRGB(uint8_t r, uint8_t g, uint8_t b, float brightness, float contrast) {
        int16_t rt, gt, bt;
        rt = (int16_t)((((float)r - 128.0f) * contrast * 2.0f + 128.0f) * brightness * 2.0f);
        if (rt > (int16_t)0xff) rt = 0xff;
        if (rt < (int16_t)0) rt = 0;
        gt = (int16_t)((((float)g - 128.0f) * contrast * 2.0f + 128.0f) * brightness * 2.0f);
        if (gt > (int16_t)0xff) gt = 0xff;
        if (gt < (int16_t)0) gt = 0;
        bt = (int16_t)((((float)b - 128.0f) * contrast * 2.0f + 128.0f) * brightness * 2.0f);
        if (bt > (int16_t)0xff) bt = 0xff;
        if (bt < (int16_t)0) bt = 0;
        return GfxDrivers::RGB((uint8_t)rt, (uint8_t)gt, (uint8_t)bt);
    }

    String themeName;
    float brightness, contrast;
    enum Theme { ThemeDark,
                 ThemeLight,
                 ThemeGruvbox,
                 ThemeSolarizedDark,
                 ThemeSolarizedLight };
    Theme themeType;
    void _setTheme(Theme theme) {
        themeType = Theme::ThemeDark;
        switch (theme) {
        case ThemeLight:
            themeName = "light";
            themeType = theme;
            defaultColor = relRGB(0x00, 0x00, 0x00, brightness, contrast);
            defaultBgColor = relRGB(0xff, 0xff, 0xff, brightness, contrast);
            defaultSeparatorColor = relRGB(0x80, 0x80, 0x80, brightness, contrast);
            defaultAccentColor = relRGB(0x40, 0x40, 0x40, brightness, contrast);
            defaultIncreaseColor = relRGB(0xff, 0xa0, 0xa0, brightness, contrast);
            defaultConstColor = relRGB(0x30, 0x30, 0x30, brightness, contrast);
            defaultDecreaseColor = relRGB(0xa0, 0xa0, 0xff, brightness, contrast);
            break;
        case ThemeSolarizedLight:
            themeName = "solarizedlight";
            themeType = theme;
            defaultColor = relRGB(0x00, 0x2b, 0x36, brightness, contrast);
            defaultBgColor = relRGB(0xee, 0xe8, 0x95, brightness, contrast);
            defaultSeparatorColor = relRGB(0x58, 0x6e, 0x05, brightness, contrast);
            defaultAccentColor = relRGB(0x67, 0x76, 0x02, brightness, contrast);
            defaultIncreaseColor = relRGB(0xeb, 0x4b, 0x16, brightness, contrast);
            defaultConstColor = relRGB(0x50, 0x50, 0x30, brightness, contrast);
            defaultDecreaseColor = relRGB(0x43, 0x64, 0xe6, brightness, contrast);
            break;
        case ThemeDark:
        default:
            themeName = "dark";
            themeType = Theme::ThemeDark;
            defaultColor = relRGB(0xff, 0xff, 0xff, brightness, contrast);
            defaultBgColor = relRGB(0x00, 0x00, 0x00, brightness, contrast);
            defaultSeparatorColor = relRGB(0x80, 0x80, 0x80, brightness, contrast);
            defaultAccentColor = relRGB(0xb0, 0xb0, 0xb0, brightness, contrast);
            defaultIncreaseColor = relRGB(0xff, 0x80, 0x80, brightness, contrast);
            defaultConstColor = relRGB(0xc0, 0xc0, 0xc0, brightness, contrast);
            defaultDecreaseColor = relRGB(0x80, 0x80, 0xff, brightness, contrast);
            break;
        }
        for (uint16_t slot = 0; slot < slots; slot++) {
            pSlots[slot].color = defaultColor;
            pSlots[slot].bgColor = defaultBgColor;
        }
        pDisplay->setBGColor(defaultBgColor);
#ifdef USE_SERIAL_DBG
        Serial.println("setTheme: " + themeName);
#endif
    }

    void _common_init() {
        active = false;
        displayFrameRateMs = 1000;
        slotResX = 64;
        slotResY = 32;
        brightness = 0.5;
        contrast = 0.5;
#if USTD_FEATURE_MEMORY >= USTD_FEATURE_MEM_128K
        defaultHistLen = 128;
#elif USTD_FEATURE_MEMORY >= USTD_FEATURE_MEM_32K
        defaultHistLen = 64;
#else
        defaultHistLen = 16;
#endif
        defaultHistSampleRateMs = 3600 * 1000 / 64;  // 1 hr in ms for entire history
    }

    bool splitCombinedLayout(String combined_layout) {
        bool layout_valid = true;
        bool parsing = true;

        formats = "";
        layout = "";
        slots = 0;
        String line = "";

        while (parsing) {
            int ind = combined_layout.indexOf('|');
            if (ind == -1) {
                line = combined_layout;
                combined_layout = "";
            } else {
                line = combined_layout.substring(0, ind);
                combined_layout = combined_layout.substring(ind + 1);
            }
            for (char c : line) {
                if (valid_formats_small.indexOf(c) == -1 && valid_formats_long.indexOf(c) == -1) {
                    layout_valid = false;
                    parsing = false;
                    break;
                } else {
                    ind = valid_formats_small.indexOf(c);
                    if (ind != -1) {
                        c = valid_formats_long[ind];
                        layout += "S";
                    } else {
                        layout += "L";
                    }
                    formats += c;
                    ++slots;
                }
            }
            if (!layout_valid) break;
            if (combined_layout != "") {
                layout += "|";
            } else {
                parsing = false;
            }
        }

        lastRefresh = 0;
        return layout_valid;
    }

    bool getConfigFromFS(String name) {
        /*! Read config from FS.
        @param name: The display's `name` and name of config file `name`.json.
        @return: True if config file was found and read, false otherwise.
        */
        String combined_layout = jf.readString(name + "/layout", "ff|ff");
        if (!splitCombinedLayout(combined_layout)) {
            return false;
        }
        for (uint8_t i = 0; i < slots; i++) {
            captions[i] = "room";
            topics[i] = "some/topic";
        }
        jf.readStringArray(name + "/topics", topics);
        jf.readStringArray(name + "/captions", captions);
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
        if (slot >= slots) {
            return false;
        }
        pSlots[slot].slotX = 0;
        pSlots[slot].slotY = 0;
        uint16_t ind = 0;
        for (auto c : layout) {
            if (c == 'S') {
                if (ind == slot) {
                    pSlots[slot].slotLenX = 1;
                    pSlots[slot].slotLenY = 1;
                    break;
                } else {
                    pSlots[slot].slotX++;
                }
                ++ind;
            } else if (c == 'L') {
                if (ind == slot) {
                    pSlots[slot].slotLenX = 2;
                    pSlots[slot].slotLenY = 1;
                    break;
                } else {
                    pSlots[slot].slotX += 2;
                }
                ++ind;
            } else if (c == '|') {
                pSlots[slot].slotY++;
                pSlots[slot].slotX = 0;
            } else {
                return false;
            }
        }
        pSlots[slot].histLen = defaultHistLen;
        pSlots[slot].offset = 0.0;
        pSlots[slot].scalingFactor = 1.0;
        switch (formats[slot]) {
        case 'I':
            pSlots[slot].slotType = SlotType::NUMBER;
            pSlots[slot].digits = 0;
            break;
        case 'F':
            pSlots[slot].slotType = SlotType::NUMBER;
            pSlots[slot].digits = 1;
            break;
        case 'D':
            pSlots[slot].slotType = SlotType::NUMBER;
            pSlots[slot].digits = 2;
            break;
        case 'T':
            pSlots[slot].slotType = SlotType::NUMBER;
            pSlots[slot].digits = 3;
            break;
        case 'S':
            pSlots[slot].slotType = SlotType::TEXT;
            pSlots[slot].digits = 3;
            pSlots[slot].histLen = 0;
            break;
        case 'P':
            pSlots[slot].slotType = SlotType::NUMBER;
            pSlots[slot].scalingFactor = 100.0;
            pSlots[slot].digits = 1;
            break;
        case 'G':
            pSlots[slot].slotType = SlotType::GRAPH;
            pSlots[slot].digits = 3;
            break;
        default:
            return false;
        }
        if (pSlots[slot].histLen) {
            pSlots[slot].pHist = (float *)malloc(sizeof(float) * pSlots[slot].histLen);
            pSlots[slot].histInit = false;
            if (pSlots[slot].pHist) {
                for (uint16_t j = 0; j < pSlots[slot].histLen; j++) {
                    pSlots[slot].pHist[j] = 0;
                }
            } else {
                pSlots[slot].histLen = 0;
                return false;
            }
        } else {
            pSlots[slot].pHist = nullptr;
            pSlots[slot].histLen = 0;
        }
        pSlots[slot].topic = topics[slot];
        pSlots[slot].caption = captions[slot];
        pSlots[slot].lastUpdate = time(nullptr);
        pSlots[slot].lastHistUpdate = 0;
        pSlots[slot].histSampleRateMs = defaultHistSampleRateMs;
        pSlots[slot].currentValue = 0.0;
        pSlots[slot].currentText = "";
        pSlots[slot].deltaDir = 0.0;
        pSlots[slot].isInit = true;
        pSlots[slot].isValid = false;
        pSlots[slot].color = defaultColor;
        pSlots[slot].bgColor = defaultBgColor;
        pSlots[slot].lastFrame = 0;
        pSlots[slot].frameRate = 1000;
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
        slots = formats.length();
        pSlots = new T_SLOT[slots];
        for (uint16_t i = 0; i < slots; i++) {
            if (!config2slot(i)) {
                return false;
            }
        }
        return true;
    }

    void _sensorLoop() {
        const char *weekDays[] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
        const char *wochenTage[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
        const char *pDay;
        struct tm *plt;
        time_t t;
        char buf[64];
        if (!active) return;
        // scruffy old c time functions
        t = time(nullptr);
        plt = localtime(&t);
        if (locale == "DE")
            pDay = wochenTage[plt->tm_wday];
        else
            pDay = weekDays[plt->tm_wday];
        // sprintf(buf,"%s %02d. %02d:%02d",pDay,plt->tm_mday, plt->tm_hour, plt->tm_min);
        sprintf(buf, "%s %02d:%02d", pDay, plt->tm_hour, plt->tm_min);
        if (strcmp(buf, oldTimeString)) {
            strcpy(oldTimeString, buf);
            sensorUpdates("clock/timeinfo", String(buf), "self.local");
        }
        // If a sensors doesn't update values for 1 hr (3600sec), declare invalid.
        for (uint8_t i = 0; i < slots; i++) {
            if (time(nullptr) - pSlots[i].lastUpdate > 3600) {
                pSlots[i].isValid = false;
            }
            if (pSlots[i].slotType == SlotType::GRAPH) {
                if (!pSlots[i].hasChanged) {
                    updateSlot(i, pSlots[i].currentText);  // force update with same value for scrolling graph
                }
            }
        }
        updateDisplay(false, false);
    }

    void commonBegin(bool useCanvas = false) {
        pDisplay->begin(useCanvas);

        auto fntsk = [=]() {
            _sensorLoop();
        };
        minUpdateIntervalMs = 50;
        int tID = pSched->add(fntsk, name, minUpdateIntervalMs * 1000L);
        auto fnsub = [=](String topic, String msg, String originator) {
            this->subsMsg(topic, msg, originator);
        };
        pSched->subscribe(tID, name + "/display/#", fnsub);
        auto fnall = [=](String topic, String msg, String originator) {
            sensorUpdates(topic, msg, originator);
        };
        for (uint8_t i = 0; i < slots; i++) {
            if (topics[i] != "") {
#if defined(USTD_FEATURE_NETWORK) && !defined(OPTION_NO_MQTT)
                if (topics[i][0] == '!') {
                    topics[i] = topics[i].substring(1);
                    pMqtt->addSubscription(tID, topics[i], fnall);
#ifdef USE_SERIAL_DBG
                    Serial.print("Subscribing via MQTT: ");
                    Serial.println(topics[i]);
#endif
                }
#endif
                if (topics[i][0] != '!') {
                    if (topics[i] != "clock/timeinfo") {  // local shortcut msg. Questionable?
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
        Serial.print(formats);
        Serial.print(" histLen: ");
        Serial.println(defaultHistLen);
#endif  // USE_SERIAL_DBG
        shortConfig2Slots();
        active = true;
        _setTheme(Theme::ThemeDark);
        updateDisplay(true, true);
    }

  public:
    void setBrightness(float _brightness = 0.5) {
        if (!active) return;
        if (_brightness < 0.0) _brightness = 0.0;
        if (_brightness > 1.0) _brightness = 1.0;
        brightness = _brightness;
        _setTheme(themeType);
        updateDisplay(true, true);
    }

    void publishBrightness() {
        if (!active) return;
        pSched->publish(name + "/display/brightness", String(brightness, 3));
    }

    void setContrast(float _contrast = 0.5) {
        if (!active) return;
        if (_contrast < 0.0) _contrast = 0.0;
        if (_contrast > 1.0) _contrast = 1.0;
        contrast = _contrast;
        _setTheme(themeType);
        updateDisplay(true, true);
    }

    void publishContrast() {
        if (!active) return;
        pSched->publish(name + "/display/contrast", String(contrast, 3));
    }

    void setTheme(String _theme) {
        if (!active) return;
        bool upd = false;
        contrast = 0.5;
        brightness = 0.5;
        if (_theme == "light") {
            _setTheme(Theme::ThemeLight);
            upd = true;
        }
        if (_theme == "solarizedlight") {
            _setTheme(Theme::ThemeSolarizedLight);
            upd = true;
        }
        if (!upd) {
            _setTheme(Theme::ThemeDark);
        }
        updateDisplay(true, true);
    }

    void publishTheme() {
        if (!active) return;
        pSched->publish(name + "/display/theme", themeName);
    }

    void setSlotCaption(uint16_t slot, String caption) {
        /*! Set the caption for a slot.
        @param slot: The slot number.
        @param caption: The caption.
        */
        if (!active) return;
        captions[slot] = caption;
        if (pSlots && slot < slots) {
            if (pSlots[slot].isInit) {
                pSlots[slot].caption = caption;
                pSlots[slot].hasChanged = true;
            }
        }
        updateDisplay(false, false);
    }

    void publishSlotCaption(uint16_t slot) {
        /*! Publish the caption for a slot.
        @param slot: The slot number, 0..<slots.
        */
        if (!active) return;
        String cap = "";
        bool done = false;
        if (slot < slots) {
            if (pSlots) {
                if (pSlots[slot].isInit) {
                    cap = pSlots[slot].caption;
                    done = true;
                }
            }
            if (!done) {
                cap = captions[slot];
            }
            pSched->publish(name + "/display/slot/" + String(slot) + "/caption", cap);
        }
    }

    void setSlotText(uint16_t slot, String text) {
        /*! Set the text for a slot.
        @param slot: The slot number.
        @param text: The text.
        */
        if (!active) return;
        if (slot < slots) {
            // XXX
        }
    }
    void publishSlotText(uint16_t slot) {
        /*! Publish the text for a slot.
        @param slot: The slot number, 0..<slots.
        */
        if (!active) return;
        if (slot < slots) {
            // pSched->publish(name+"/display/slot/"+String(slot), xxx[slot]);
        }
    }

    void setSlotTopic(uint16_t slot, String topic) {
        /*! Set the topic for a slot.
        @param slot: The slot number.
        @param topic: The topic.
        */
        if (!active) return;
    }
    void publishSlotTopic(uint16_t slot) {
        /*! Publish the topic for a slot.
        @param slot: The slot number, 0..<slots.
        */
        if (!active) return;
    }
    void setSlotFormat(uint16_t slot, String format) {
        /*! Set the format for a slot.
        @param slot: The slot number.
        @param format: The format.
        */
        if (!active) return;
        if (slot < slots && format.length() == 1) {
            formats[slot] = format[0];
            pSlots[slot].hasChanged = true;
            updateDisplay(true, true);
        }
    }
    void publishSlotFormat(uint16_t slot) {
        /*! Publish the format for a slot.
        @param slot: The slot number, 0..<slots.
        */
        if (!active) return;
    }

    void setSlotHistorySampleRateMs(uint16_t slot, uint32_t rate) {
        /*! Set the history sample rate for a slot.
        @param slot: The slot number.
        @param rate: The sample rate in milliseconds.
        */
        if (!active) return;
        if (slot < slots) {
            pSlots[slot].histSampleRateMs = rate;
            pSlots[slot].frameRate = rate;
            if (rate < displayFrameRateMs) {
                displayFrameRateMs = rate;
            }
        }
    }

    void publishSlotHistorySampleRateMs(uint16_t slot) {
        /*! Publish the history sample rate for a slot.
        @param slot: The slot number, 0..<slots.
        */
        if (!active) return;
        if (slot < slots) {
            pSched->publish(name + "/display/slot/" + String(slot) + "/histosrysampleratems", String(pSlots[slot].histSampleRateMs));
        }
    }
#if defined(USTD_FEATURE_NETWORK) && !defined(OPTION_NO_MQTT)
    void begin(ustd::Scheduler *_pSched, ustd::Mqtt *_pMqtt, bool _useCanvas = false) {
        /*! Activate display and begin receiving MQTT updates for the display slots

        @param _pSched Pointer to the muwerk scheduler
        @param _pMqtt Pointer to munet mqtt object, used to subscribe to mqtt topics defined in `'display-name'.json` file.
        @param _useCanvas Use the canvas for the display. Note: requires considerable memory for color displays!
        */
        pSched = _pSched;
        pMqtt = _pMqtt;
#else
    void begin(ustd::Scheduler *_pSched, bool _useCanvas = false) {
        /*! Activate display and begin receiving updates for the display slots

        @param _pSched Pointer to the muwerk scheduler
        @param _useCanvas Use the canvas for the display. Note: requires considerable memory for color displays!
        */
        pSched = _pSched;
#endif
        getConfigFromFS(name);
        commonBegin(_useCanvas);
    }

#if defined(USTD_FEATURE_NETWORK) && !defined(OPTION_NO_MQTT)
    void begin(ustd::Scheduler *_pSched, ustd::Mqtt *_pMqtt, String combined_layout, ustd::array<String> _topics, ustd::array<String> _captions, bool _useCanvas = false) {
        /*! Activate display and begin receiving MQTT updates for the display slots

        @param _pSched Pointer to the muwerk scheduler
        @param _pMqtt Pointer to munet mqtt object, used to subscribe to mqtt topics defined in `'display-name'.json` file.
        @param combined_layout The layout string, e.g. "ff|ff" (two lines, two short floats each).
        @param _topics std::array<String> of topics to subscribe to. The number of topics must match the number of captions and the number of slot-qualifiers in the combined_layout string.
        @param _captions std::array<String> of captions to display for the topics. The number of captions must match the number of topics and the number of slot-qualifiers in the combined_layout string.
        @param _useCanvas Use the canvas for the display. Note: requires considerable memory for color displays!
        */
        pSched = _pSched;
        pMqtt = _pMqtt;
#else
    void begin(ustd::Scheduler *_pSched, String combined_layout, ustd::array<String> _topics, ustd::array<String> _captions, bool _useCanvas = false) {
        /*! Activate display and begin receiving updates for the display slots

        @param _pSched Pointer to the muwerk scheduler
        @param combined_layout The layout string, e.g. "ff|ff" (two lines, two short floats each).
        @param _topics std::array<String> of topics to subscribe to. The number of topics must match the number of captions and the number of slot-qualifiers in the combined_layout string.
        @param _captions std::array<String> of captions to display for the topics. The number of captions must match the number of topics and the number of slot-qualifiers in the combined_layout string.
        @param _useCanvas Use the canvas for the display. Note: requires considerable memory for color displays!
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
        commonBegin(_useCanvas);
    }

#if defined(USTD_FEATURE_NETWORK) && !defined(OPTION_NO_MQTT)
    void begin(ustd::Scheduler *_pSched, ustd::Mqtt *_pMqtt, String combined_layout, uint16_t _slots, const char *_topics[], const char *_captions[], bool _useCanvas = false) {
        /*! Activate display and begin receiving MQTT updates for the display slots

        @param _pSched Pointer to the muwerk scheduler
        @param _pMqtt Pointer to munet mqtt object, used to subscribe to mqtt topics defined in `'display-name'.json` file.
        @param combined_layout The layout string, e.g. "ff|ff" (two lines, two short floats each).
        @param _slots Number of slots to use, must be array dimension of both _captions and topics.
        @param _topics const char *[] of topics to subscribe to. The number of topics must match the number of captions and the number of slot-qualifiers in the combined_layout string.
        @param _captions const char *[] of captions to display for the topics. The number of captions must match the number of topics and the number of slot-qualifiers in the combined_layout string.
        @param _useCanvas Use the canvas for the display. Note: requires considerable memory for color displays!
        */
        pSched = _pSched;
        pMqtt = _pMqtt;
#else
    void begin(ustd::Scheduler *_pSched, String combined_layout, uint16_t _slots, const char *_topics[], const char *_captions[], bool _useCanvas = false) {
        /*! Activate display and begin receiving updates for the display slots

        @param _pSched Pointer to the muwerk scheduler
        @param combined_layout The layout string, e.g. "ff|ff" (two lines, two short floats each).
        @param _slots Number of slots to use, must be array dimension of both _captions and topics.
        @param _topics const char *[] of topics to subscribe to. The number of topics must match the number of captions and the number of slot-qualifiers in the combined_layout string.
        @param _captions const char *[] of captions to display for the topics. The number of captions must match the number of topics and the number of slot-qualifiers in the combined_layout string.
        @param _useCanvas Use the canvas for the display. Note: requires considerable memory for color displays!
        */
        pSched = _pSched;
#endif
        for (uint16_t i = 0; i < _slots; i++) {
            String s = _topics[i];
            topics.add(s);
            String c = _captions[i];
            captions.add(c);
        }
        getConfigFromLayout(name, combined_layout);
        commonBegin(_useCanvas);
    }

  private:
    void drawArrow(uint16_t x, uint16_t y, bool up = true, uint16_t len = 8, uint16_t wid = 3, int16_t delta_down = 0) {
        uint32_t red = defaultIncreaseColor;
        uint32_t blue = defaultDecreaseColor;
        if (up) {
            pDisplay->drawLine(x, y + len, x, y, red);
            pDisplay->drawLine(x + 1, y + len, x + 1, y, red);
            pDisplay->drawLine(x, y, x - wid, y + wid, red);
            pDisplay->drawLine(x, y, x + wid, y + wid, red);
            pDisplay->drawLine(x + 1, y, x - wid + 1, y + wid, red);
            pDisplay->drawLine(x + 1, y, x + wid + 1, y + wid, red);
        } else {
            pDisplay->drawLine(x, y + len + delta_down, x, y + delta_down, blue);
            pDisplay->drawLine(x + 1, y + len + delta_down, x + 1, y + delta_down, blue);
            pDisplay->drawLine(x, y + len + delta_down, x - wid, y + len - wid + delta_down, blue);
            pDisplay->drawLine(x, y + len + delta_down, x + wid, y + len - wid + delta_down, blue);
            pDisplay->drawLine(x + 1, y + len + delta_down, x - wid + 1, y + len - wid + delta_down, blue);
            pDisplay->drawLine(x + 1, y + len + delta_down, x + wid + 1, y + len - wid + delta_down, blue);
        }
    }

    void boldParser(String msg, String &first, String &sec) {
        bool isBold = true;
        for (unsigned int i = 0; i < msg.length(); i++) {
            if (msg[i] == '_') {
                isBold = !isBold;
                continue;
            }
            if (isBold) {
                first += msg[i];
                sec += msg[i];
            } else {
                first += msg[i];
                sec += ' ';
            }
        }
    }

    bool displaySlot(uint16_t slot) {
        if (slot >= slots) return false;

        uint8_t x0 = 0, y0 = 0, x1 = 0, y1 = 0, xa = 0, ya = 0;
        uint8_t xm0, ym0, xm1, ym1;

        // Blank
        uint16_t xf0, yf0, xl, yl;
        xf0 = pSlots[slot].slotX * slotResX;
        xl = slotResX * pSlots[slot].slotLenX;
        yf0 = pSlots[slot].slotY * slotResY + 1;
        yl = slotResY * pSlots[slot].slotLenY - 1;
        pDisplay->fillRect(xf0, yf0, xl, yl, pSlots[slot].bgColor);
        // Caption font start x0,y0
        x0 = pSlots[slot].slotX * slotResX + 14;
        y0 = pSlots[slot].slotY * slotResY + 3;
        x1 = pSlots[slot].slotX * slotResX + 14;
        y1 = pSlots[slot].slotY * slotResY + slotResY - 3;
        xa = pSlots[slot].slotX * slotResX + 5;
        ya = pSlots[slot].slotY * slotResY + 14;
        xm0 = pSlots[slot].slotX * slotResX + 1;
        ym0 = pSlots[slot].slotY * slotResY + 1;
        xm1 = (pSlots[slot].slotX + 1) * slotResX - 2 + (pSlots[slot].slotLenX - 1) * slotResX;
        ym1 = (pSlots[slot].slotY + 1) * slotResY - 2 + (pSlots[slot].slotLenY - 1) * slotResY;

        // caption
        pDisplay->setFont();
        pDisplay->setTextColor(defaultAccentColor);
        pDisplay->setTextSize(1);
        String first = "", second = "";
        boldParser(pSlots[slot].caption, first, second);
        pDisplay->setCursor(x0, y0);
        pDisplay->println(first);
        pDisplay->setCursor(x0 + 1, y0);
        pDisplay->println(second);

        float gmin, gmax, dmin, dmax;
        float avg, navg;
        dmin = 1000000000.0;
        dmax = -1000000000.0;
        gmax = -1000000000.0;
        gmin = 1000000000.0;
        avg = 0.0;
        navg = 0.0;
        if (pSlots[slot].slotType != SlotType::TEXT) {
            if (pSlots[slot].pHist && pSlots[slot].histLen) {
                for (uint16_t x = 0; x < pSlots[slot].histLen; x++) {
                    if (pSlots[slot].pHist[x] > gmax) gmax = pSlots[slot].pHist[x];
                    if (pSlots[slot].pHist[x] < gmin) gmin = pSlots[slot].pHist[x];
                    uint16_t backView = 0;
                    if (pSlots[slot].histLen >= 10) backView = pSlots[slot].histLen - 10;
                    if (x >= backView) {
                        avg += pSlots[slot].pHist[x];
                        navg++;
                        if (pSlots[slot].pHist[x] > dmax) dmax = pSlots[slot].pHist[x];
                        if (pSlots[slot].pHist[x] < dmin) dmin = pSlots[slot].pHist[x];
                    }
                }
                float ref;
                if (navg > 0.0)
                    ref = avg / navg;
                else
                    ref = 0.0;
                pSlots[slot].deltaDir = pSlots[slot].currentValue - ref;
            }
        }
        if (pSlots[slot].slotType != SlotType::GRAPH) {
            // Main text
            pDisplay->setFont(&FreeSans12pt7b);
            pDisplay->setTextColor(defaultColor);
            pDisplay->setTextSize(1);
            pDisplay->setCursor(x1, y1);
            pDisplay->println(pSlots[slot].currentText);
            pDisplay->setCursor(x1 + 1, y1);
            pDisplay->println(pSlots[slot].currentText);
            if (pSlots[slot].slotType != SlotType::TEXT) {
                // arrow
                if (pSlots[slot].deltaDir != 0.0) {
                    drawArrow(xa, ya, (pSlots[slot].deltaDir > 0.0), 8, 3, 7);
                }
            }
        } else {
            // Graph
            if (pSlots[slot].pHist && pSlots[slot].histLen) {
                float deltaY = gmax - gmin;
                if (deltaY < 0.0001) deltaY = 1;
                float deltaX = (float)(xm1 - xm0) / (float)(pSlots[slot].histLen);
                int lx0, ly0, lx1, ly1;
                int gHeight = (ym1 - ym0) - 11;  // font size of caption.
                for (uint16_t i = 1; i < pSlots[slot].histLen; i++) {
                    lx0 = xm0 + (int)((float)(i - 1) * deltaX);
                    lx1 = xm0 + (int)((float)i * deltaX);
                    ly0 = ym1 - (int)((pSlots[slot].pHist[i - 1] - gmin) / deltaY * (float)(gHeight));
                    ly1 = ym1 - (int)((pSlots[slot].pHist[i] - gmin) / deltaY * (float)(gHeight));
                    uint32_t col;
                    if (ly1 < ly0)
                        col = defaultIncreaseColor;
                    else {
                        if (ly1 == ly0)
                            col = defaultConstColor;
                        else
                            col = defaultDecreaseColor;
                    }
                    pDisplay->drawLine(lx0, ly0, lx1, ly1, col);
                }
            }
        }
        pSlots[slot].hasChanged = false;
        return true;
    }

  public:
    void updateDisplay(bool updateNow, bool forceRedraw = false) {
        if (!updateNow && timeDiff(lastRefresh, millis()) < displayFrameRateMs) {
            return;
        }
        bool update = false;
        lastRefresh = millis();
        uint16_t maxSlotX = 0, maxSlotY = 0;

        if (forceRedraw) {
            pDisplay->clearDisplay(defaultBgColor);
            for (uint16_t slot = 0; slot < slots; slot++) {
                if (pSlots[slot].slotX > maxSlotX) maxSlotX = pSlots[slot].slotX;
                if (pSlots[slot].slotY > maxSlotY) maxSlotY = pSlots[slot].slotY;
            }
            uint16_t y;
            for (uint16_t ly = 0; ly <= maxSlotY + 1; ly++) {
                y = ly * slotResY;
                if (y >= resY) y = resY - 1;
                // XXX slotLenY==1! (maybe implicitly solved by rect fill)
                pDisplay->drawLine(0, y, resX - 1, y, defaultSeparatorColor);
            }
        }
        for (uint16_t slot = 0; slot < slots; slot++) {
            if (pSlots[slot].hasChanged || forceRedraw) {
                if (displaySlot(slot)) update = true;
            }
        }
        if (update || forceRedraw) {
            pDisplay->display();
        }
    }

  private:
    bool updateSlot(uint16_t slot, String msg) {
        if (slot >= slots) return false;
        if (timeDiff(pSlots[slot].lastFrame, millis()) < pSlots[slot].frameRate) return false;
        bool changed = false;
        float k = pSlots[slot].scalingFactor;
        float o = pSlots[slot].offset;
        switch (pSlots[slot].slotType) {
        case SlotType::TEXT:
            if (pSlots[slot].currentText != msg) {
                changed = true;
            }
            pSlots[slot].currentText = msg;
            pSlots[slot].isValid = true;
            pSlots[slot].lastUpdate = time(nullptr);
            pSlots[slot].lastFrame = millis();
            break;
        case SlotType::NUMBER:
        case SlotType::GRAPH:
            pSlots[slot].currentValue = msg.toFloat() * k + o;
            pSlots[slot].isValid = true;
            pSlots[slot].lastUpdate = time(nullptr);
            pSlots[slot].lastFrame = millis();
            String newVal = String(pSlots[slot].currentValue, (uint16_t)pSlots[slot].digits);
            if (pSlots[slot].currentText != newVal) {
                changed = true;
            }
            pSlots[slot].currentText = newVal;
            if (pSlots[slot].pHist && pSlots[slot].histLen > 0) {
                if (!pSlots[slot].histInit) {
                    for (uint16_t i = 0; i < pSlots[slot].histLen; i++) {
                        pSlots[slot].pHist[i] = pSlots[slot].currentValue;
                    }
                    pSlots[slot].lastHistUpdate = millis();
                    pSlots[slot].histInit = true;
                    changed = true;
                } else {
                    while (timeDiff(pSlots[slot].lastHistUpdate, millis()) > pSlots[slot].histSampleRateMs) {
                        for (uint16_t i = 0; i < pSlots[slot].histLen - 1; i++) {
                            pSlots[slot].pHist[i] = pSlots[slot].pHist[i + 1];
                        }
                        pSlots[slot].lastHistUpdate += pSlots[slot].histSampleRateMs;
                        pSlots[slot].pHist[pSlots[slot].histLen - 1] = pSlots[slot].currentValue;
                        if (pSlots[slot].slotType == SlotType::GRAPH) {
                            changed = true;
                        }
                    }
                    pSlots[slot].pHist[pSlots[slot].histLen - 1] = pSlots[slot].currentValue;
                }
            }
            break;
        }
        pSlots[slot].hasChanged = changed;
        return changed;
    }

    void sensorUpdates(String topic, String msg, String originator) {
        if (!active) return;
        bool changed = false;
        for (uint16_t slot = 0; slot < slots; slot++) {
            if (pSlots[slot].topic == topic) {
                if (updateSlot(slot, msg)) changed = true;
            }
        }
        if (changed) updateDisplay(false, false);
    }

    void subsMsg(String topic, String msg, String originator) {
        if (!active) return;
        String toc = name + "/display/slot/";
        if (topic.startsWith(toc)) {
            String sub = topic.substring(toc.length());
            int16_t ind = sub.indexOf("/");
            if (ind != -1) {
                int16_t slot = sub.substring(0, ind).toInt();
                if (slot < slots) {
                    String action = sub.substring(ind + 1);
                    if (action == "caption/get") {
                        publishSlotCaption(slot);
                    } else if (action == "caption/set") {
                        setSlotCaption(slot, msg);
                    } else if (action == "format/get") {
                        publishSlotFormat(slot);
                    } else if (action == "format/set") {
                        setSlotFormat(slot, msg);
                    } else if (action == "topic/get") {
                        publishSlotTopic(slot);
                    } else if (action == "topic/set") {
                        setSlotTopic(slot, msg);
                    } else if (action == "text/get") {
                        publishSlotText(slot);
                    } else if (action == "text/set") {
                        setSlotText(slot, msg);
                    } else if (action == "historysampleratems/get") {
                        publishSlotHistorySampleRateMs(slot);
                    } else if (action == "historysampleratems/set") {
                        setSlotHistorySampleRateMs(slot, msg.toInt());
                    }
                }
            }
        } else if (topic == name + "/display/brightness/set") {
            float br = msg.toFloat();
            if (br < 0.0) br = 0.0;
            if (br > 1.0) br = 1.0;
            setBrightness(br);
        } else if (topic == name + "/display/brightness/get") {
            publishBrightness();
        } else if (topic == name + "/display/contrast/set") {
            float c = msg.toFloat();
            if (c < 0.0) c = 0.0;
            if (c > 1.0) c = 1.0;
            setContrast(c);
        } else if (topic == name + "/display/contrast/get") {
            publishContrast();
        } else if (topic == name + "/display/theme/set") {
            setTheme(msg);
        } else if (topic == name + "/display/theme/get") {
            publishTheme();
        }
    }

};  // SensorDisplay
}  // namespace ustd
