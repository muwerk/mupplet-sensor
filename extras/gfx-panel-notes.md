GFX_Panel mupplet
======================

![OLED](oled.png)

The `mup_gfx_panel` mupplet displays sensor values as numbers or plots. Supported displays are:
* Oled SSD1306 128x64
* TFT ST7735 128x128, 128x160

![OLED](https://github.com/muwerk/mupplet-sensor/blob/master/extras/oled.gif)

The display is divded into slots of either 32x32 or 64x32 pixels. Each slot can either display
a short string, a number or a graphical plot. Data source for the values displayed are pub/sub
topics (either internal, or, if available, MQTT topics).

It can optionally read it's basic configuration from a JSON file on ESPs data partition.
The name of the json file is determined by the name of the instance of the mupplet:

```cpp
ustd::SensorDisplay display("display1", 128,64,0x3c);
```

This Display will look for a json file `display1.json`. A sample content could be:

```json
    "layout": "S|ff",
    "topics": ["clock/timeinfo", "!hastates/sensor/temperature/state", "sensor/temperature"],
    "captions": ["Time _wday hh:mm", "Out _C", "Studio _C"],
```

- **`layout`** defines the slots that are available to display information. Each slot is described by a single letter, and `|` marks
  a new line. An upper-case letter defines a large 64x32 slot, lower-case is used for 32x32 slots.
  
  ![TFT](https://github.com/muwerk/mupplet-sensor/blob/master/extras/tft.gif)
  
  The example displays a `S` large 64x32 string in line 1, and two 32x32 `f` floats with one decimal in line. 
  
  Possible letter codes are
  - `S`: simply display content of MQTT message as string.
  - `I`: display MQTT converted to integer.
  - `P`: display MQTT as int(value * 100.0)%. (percentage)
  - `F`: display as float with one decimal
  - `D`: with two decimals
  - `T`: with three decimals
  - `G`: show a graphical plot of the value history
   
- **`topics`** define a list of external MQTT topics (marked by leading '!') or internal message topics (no leading '!'). A special
  topic `clock/timeinfo` displays weekday and time. Since there are three display-slots (`S`, `f`, `f`), there are three topics.
- **`captions`** Each display-slot has a caption. Default font is bold. A `_` character can be used to switch between bold and normal.
  e.g. `Out _C` will print as **`Out`**` C`.
  
