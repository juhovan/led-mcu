# LED-MCU

Used to control addressable LED strips with MQTT using a ESP8266 NodeMCU, to be used with Home Assistant.

Setup:

- Clone/download this repository
- Copy `include/config.h.example` as `include/config.h`
- Set your configuration in `config.h`
- Build/flash like any other PlatformIO project
- The data pin of the LED strip must be connected to GPIO3 (pin labeled "RX") [due to ESP8266 limitations](https://github.com/Makuna/NeoPixelBus/wiki/ESP8266-NeoMethods).

For Home Assistant you'll want something like this in your configuration.yaml:

```
light:
  - platform: mqtt
    name: Bedroom ledstrip
    command_topic: "LED_MCU/command"
    state_topic: "LED_MCU/state"
    availability_topic: "LED_MCU/availability"
    rgb_command_topic: "LED_MCU/color"
    rgb_state_topic: "LED_MCU/colorState"
    white_value_command_topic: "LED_MCU/white"
    white_value_state_topic: "LED_MCU/whiteState"
    white_value_scale: 255
    effect_command_topic: "LED_MCU/effect"
    effect_state_topic: "LED_MCU/effectState"
    effect_list: 
      - stable
      - sunrise
      - colorloop
    retain: true
```

Over The Air update:

Documentation: https://arduino-esp8266.readthedocs.io/en/latest/ota_updates/readme.html#web-browser

Basic steps:

- Use PlatformIO: Build
- Browse to http://IP_ADDRESS/update or http://hostname.local/update
- Select .pio/build/nodemcuv2/firmware.elf from work directory as Firmware and press Update Firmware
