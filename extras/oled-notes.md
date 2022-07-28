Oled mupplet
======================

![OLED](oled.png)

The Oled mupplet reads it's basic configuration from a JSON file on ESPs data partition.
The name of the json file is determined by the name of the instance of the mupplet:

```cpp
ustd::SensorDisplay display("display1", 128,64,0x3c);
```

This Display will look for a json file `display1.json`. A sample content could be:

```json
    "layout": "S|FF",
    "topics": ["clock/timeinfo", "!hastates/sensor/temperature/state", "sensor/temperature"],
    "captions": ["Time _wday hh:mm", "Out _C", "Studio _C"],
```

- **`layout`** defines the slots that are available to display information. Each slot is described by a single letter, and `|` marks
  a new line. The example displays a `S` string in line 1, and two `F` floats with one decimal in line. Possible letter codes are
  - `S`: simply display content of MQTT message as string.
  - `I`: display MQTT converted to integer.
  - `P`: display MQTT as int(value * 100.0)%. (percentage)
  - `F`: display as float with one decimal
  - `D`: with two decimals
  - `T`: with three decimals
- **`topics`** define a list of external MQTT topics (marked by leading '!') or internal message topics (no leading '!'). A special
  topic `clock/timeinfo` displays weekday and time. Since there are three display-slots (`S`, `F`, `F`), there are three topics.
- **`captions`** Each display-slot has a caption. Default font is bold. A `_` character can be used to switch between bold and normal.
  e.g. `Out _C` will print as **`Out`**` C`.
  
