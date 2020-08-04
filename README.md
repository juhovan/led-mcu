# LED-MCU

Used to control addressable LED strips with MQTT using a ESP8266 NodeMCU, to be used with Home Assistant.

## Setup:

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
      - gradient
      - custom
      - sunrise
      - colorloop
    retain: true
```

## MQTT control

All MQTT commands should be sent with the `retain` flag set to let the MCU restore the desired state and configuration after a reboot.

### Triggering a sunrise

Turns the leds on with the sunrise effect, send a duration in seconds to `LED_MCU/wakeAlarm`

### Configuring the gradient effect

Send a message to `LED_MCU/setGradient` with payload `X Y` where X is one of:

- `N` near, start the gradient from the "MCU end" of the strip
- `F` far, start the gradient from the opposite end
- `C` center, start the gradient from the center
- `E` edges, start the gradient from both ends

And Y is the gradient extent in percentage of strip length (0-100, can go over 100%) from the start

For example `E 50` will start a gradient from both ends that reaches zero intensity in the center. `N 200` will start a gradient from the "MCU end" and reach half intensity at the opposite end.

Do note that due to rounding to 8bits for the leds and the fact that even the dimmest settings of the leds are rather bright, you might need to fiddle with the values a little to find what you want.

### Configuring the custom mode

Send a RGBW hex string to `LED_MCU/setCustom`. The hex string is automatically zero-padded at the end.

For example a payload of `FF00000000FF00FF` will enable set the first led red to max (`FF000000`) and second led green and white to max (`00FF00FF`) with all remaining leds off.

### Configuring the enabled leds

This configuration applies to *all modes*. Useful when your strip has leds that you don't want to use.

Send a bitmask in hex to `LED_MCU/setEnabledLeds`. The bitmask is automatically zero-padded at the end.

For example a payload of `00F1` (which is `0000000011110011` in binary) will enable only leds 9-12 and 15-16 counting from the MCU.

## Over The Air update:

Documentation: https://arduino-esp8266.readthedocs.io/en/latest/ota_updates/readme.html#web-browser

Basic steps:

- Use PlatformIO: Build
- Browse to http://IP_ADDRESS/update or http://hostname.local/update
- Select .pio/build/nodemcuv2/firmware.bin from work directory as Firmware and press Update Firmware
