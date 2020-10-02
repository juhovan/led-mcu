#include <Arduino.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <string.h>

#include "common.h"

#ifdef HTTPUpdateServer
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>

ESP8266WebServer httpUpdateServer(80);
ESP8266HTTPUpdateServer httpUpdater;
#endif

#define LED_ON LOW
#define LED_OFF HIGH

const char *ssid = USER_SSID;
const char *password = USER_PASSWORD;
const char *mqtt_server = USER_MQTT_SERVER;
const int mqtt_port = USER_MQTT_PORT;
const char *mqtt_user = USER_MQTT_USERNAME;
const char *mqtt_pass = USER_MQTT_PASSWORD;
const char *mqtt_client_name = USER_MQTT_CLIENT_NAME;

// Globals
SimpleTimer timer;
RgbwColor stripLeds[NUM_LEDS] = {};
RgbwColor customLeds[NUM_LEDS] = {};
byte enabledLeds[NUM_LEDS / 8 + 1] = {};
Effect effect = eStable;
uint8_t red = 0;
uint8_t green = 0;
uint8_t blue = 0;
uint8_t white = 0;
char gradientMode = 'E';
int gradientExtent = 50;
int sunriseDuration = NUM_LEDS;

// Locals
WiFiClient espClient;
PubSubClient client(espClient);
NeoPixelBus<NeoGrbwFeature, Neo800KbpsMethod> strip(NUM_LEDS);
bool on = true;
char charPayload[MQTT_MAX_PACKET_SIZE];
char effectStr[64] = "stable";
// The colorX are pure color, without brightness applied
uint8_t colorRed = 0;
uint8_t colorGreen = 0;
uint8_t colorBlue = 0;
uint8_t brightness = 0;
int transition = 1;
int transitionCounter = 0;
int transitionTimerID = -1;

int char2int(char input)
{
  if (input >= '0' && input <= '9')
  {
    return input - '0';
  }
  if (input >= 'A' && input <= 'F')
  {
    return input - 'A' + 10;
  }
  if (input >= 'a' && input <= 'f')
  {
    return input - 'a' + 10;
  }
  return 0;
}

void publishAttrChange()
{
  char buf[256];
  snprintf(buf, 256,
           "{\"mcu_name\":\"" USER_MQTT_CLIENT_NAME "\","
           "\"num_leds\":%d,"
           "\"gradient_mode\":\"%c\","
           "\"gradient_extent\":%d}",
           NUM_LEDS, gradientMode, gradientExtent);
  client.publish(USER_MQTT_CLIENT_NAME "/attributes", buf, true);
}

void publishStateChange()
{
  char buf[256];
  snprintf(buf, 256, "%s,%d,%d,%d,%d,%d,%d,%s", (on ? "on" : "off"), transition, colorRed, colorGreen, colorBlue, white, brightness, effectStr);
  client.publish(USER_MQTT_CLIENT_NAME "/state", buf, true);
}

void rgbwChange()
{
  red = map(colorRed, 0, 255, 0, brightness);
  green = map(colorGreen, 0, 255, 0, brightness);
  blue = map(colorBlue, 0, 255, 0, brightness);
  switch (effect)
  {
  case eCustom:
  case eSunrise:
  case eColorLoop:
    // Setting the color or white value should stop effects that don't use the configured color
    effect = eStable;
    break;
  default:
    break;
  }
  startEffect(effect);
}

void processTransition()
{
  int transitionStep = 1;
  if (transition <= 1)
  {
    transitionStep = 2; // one second transition is too short for full 256 steps
  }
  transitionCounter -= transitionStep;
  if (transitionCounter > 0)
  {
    transitionTimerID = timer.setTimeout(transition * 1000 / transitionStep / 255, processTransition);
  }
  else if (!on)
  {
    stopEffect(); // Stop the effect when transition is finished and the new state is off
  }
}

void startTransition()
{
  if (transitionTimerID != -1)
  {
    timer.deleteTimer(transitionTimerID);
  }
  transitionTimerID = -1;
  processTransition();
}

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  String newTopic = topic;
  Serial.print(topic);
  Serial.print("] ");
  memset(&charPayload, 0, sizeof(charPayload));
  memcpy(charPayload, payload, min(static_cast<unsigned int>(sizeof(charPayload)), length));
  charPayload[length] = '\0';
  String newPayload = String(charPayload);
  int intPayload = newPayload.toInt();
  Serial.println(newPayload);
  newPayload.toCharArray(charPayload, newPayload.length() + 1);

  if (newTopic == USER_MQTT_CLIENT_NAME "/command")
  {
    bool onOffTransition = false;
    transition = 1;
    char *token, *strPtr, *str;
    strPtr = str = strdup(charPayload);
    for (int i = 0; (token = strsep(&str, ",")); i++)
    {
      if (!*token)
      {
        continue;
      }
      switch (i)
      {
      case 0: // on/off
        if (strcmp(token, "on") == 0)
        {
          if (!on)
          {
            onOffTransition = true;
          }
          on = true;
        }
        else if (strcmp(token, "off") == 0)
        {
          if (on)
          {
            onOffTransition = true;
          }
          on = false;
        }
        break;
      case 1: // transition
        transition = atoi(token);
        break;
      case 2: // r
        colorRed = atoi(token);
        rgbwChange();
        break;
      case 3: // g
        colorGreen = atoi(token);
        rgbwChange();
        break;
      case 4: // b
        colorBlue = atoi(token);
        rgbwChange();
        break;
      case 5: // w
        white = atoi(token);
        rgbwChange();
        break;
      case 6: // brightness
        brightness = atoi(token);
        rgbwChange();
        break;
      case 7: // effect
        if (strcmp(token, "stable") == 0)
        {
          startEffect(eStable);
        }
        else if (strcmp(token, "colorloop") == 0)
        {
          startEffect(eColorLoop);
        }
        else if (strcmp(token, "gradient") == 0)
        {
          startEffect(eGradient);
        }
        else if (strcmp(token, "custom") == 0)
        {
          startEffect(eCustom);
        }
        else if (strcmp(token, "sunrise") == 0)
        {
          startEffect(eSunrise);
        }
        else
        {
          Serial.print("Unknown effect: ");
          Serial.println(token);
          continue;
        }
        strcpy(effectStr, token);
        break;
      }
    }
    free(strPtr);
    if (onOffTransition)
    {
      transitionCounter = transition != 0 ? 256 : 0;
    }
    startEffect(effect);
    publishStateChange();
    startTransition();
  }
  else if (newTopic == USER_MQTT_CLIENT_NAME "/wakeAlarm")
  {
    on = true;
    sunriseDuration = intPayload;
    startEffect(eSunrise);
    publishStateChange();
  }
  else if (newTopic == USER_MQTT_CLIENT_NAME "/setGradient")
  {
    if (newPayload.length() >= 3)
    {
      char mode = newPayload[0];
      switch (mode)
      {
      case 'N':
      case 'F':
      case 'C':
      case 'E':
        gradientMode = mode;
        gradientExtent = newPayload.substring(2).toInt();
        break;
      default:
        Serial.print("Invalid gradient: ");
        Serial.println(charPayload);
        return;
      }
      startEffect(eGradient);
      publishStateChange();
      publishAttrChange();
    }
    else
    {
      Serial.print("Invalid gradient: ");
      Serial.println(charPayload);
      return;
    }
  }
  else if (newTopic == USER_MQTT_CLIENT_NAME "/setCustom")
  {
    for (int i = 0; i < NUM_LEDS; i++)
    {
      customLeds[i] = RgbwColor(0, 0, 0, 0);
    }
    for (int i = 0; charPayload[i] && i / 8 <= NUM_LEDS; i++)
    {
      int value;
      if (i % 2)
      {
        value = char2int(charPayload[i]);
      }
      else
      {
        value = char2int(charPayload[i]) << 4;
      }
      switch (i / 2 % 4)
      {
      case 0:
        customLeds[i / 8].R |= value;
        break;
      case 1:
        customLeds[i / 8].G |= value;
        break;
      case 2:
        customLeds[i / 8].B |= value;
        break;
      case 3:
        customLeds[i / 8].W |= value;
        break;
      }
    }
    startEffect(eCustom);
    publishStateChange();
    // TODO: add this to attributes and publishAttrChange();
  }
  else if (newTopic == USER_MQTT_CLIENT_NAME "/setEnabledLeds")
  {
    memset(&enabledLeds, 0, sizeof(enabledLeds));
    for (int i = 0; charPayload[i] && i / 2 <= NUM_LEDS / 8; i++)
    {
      if (i % 2)
      {
        enabledLeds[i / 2] |= char2int(charPayload[i]);
      }
      else
      {
        enabledLeds[i / 2] = char2int(charPayload[i]) << 4;
      }
    }
    // TODO: add this to attributes and publishAttrChange();
  }
  else if (newTopic == USER_MQTT_CLIENT_NAME "/state")
  {
    // restore previous state after a reboot
    client.publish(USER_MQTT_CLIENT_NAME "/command", charPayload);
    client.unsubscribe(USER_MQTT_CLIENT_NAME "/state");
  }
}

void setup_wifi()
{
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.mode(WIFI_STA);
  WiFi.hostname(USER_MQTT_CLIENT_NAME);

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void checkConnection()
{
  if (client.connected())
  {
    return;
  }
  digitalWrite(LED_BUILTIN, LED_ON);
  int retries = 0;
  while (!client.connected())
  {
    if (retries < 150)
    {
      Serial.print("Attempting MQTT connection...");
      if (client.connect(mqtt_client_name, mqtt_user, mqtt_pass, USER_MQTT_CLIENT_NAME "/availability", 0, true, "offline"))
      {
        Serial.println("connected");
        client.publish(USER_MQTT_CLIENT_NAME "/availability", "online", true);
        client.subscribe(USER_MQTT_CLIENT_NAME "/command");
        client.subscribe(USER_MQTT_CLIENT_NAME "/wakeAlarm");
        client.subscribe(USER_MQTT_CLIENT_NAME "/setGradient");
        client.subscribe(USER_MQTT_CLIENT_NAME "/setCustom");
        client.subscribe(USER_MQTT_CLIENT_NAME "/setEnabledLeds");
        client.subscribe(USER_MQTT_CLIENT_NAME "/state"); // used for state restoration after a reboot
      }
      else
      {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
        retries++;
        delay(5000);
      }
    }
    else
    {
      ESP.restart();
    }
  }
  digitalWrite(LED_BUILTIN, LED_OFF);
}

#ifdef HTTPUpdateServer
void setup_http_server()
{
  MDNS.begin(USER_MQTT_CLIENT_NAME);

  httpUpdater.setup(&httpUpdateServer, USER_HTTP_USERNAME, USER_HTTP_PASSWORD);
  httpUpdateServer.begin();

  MDNS.addService("http", "tcp", 80);
}
#endif

void setup()
{
  memset(&enabledLeds, 0xff, sizeof(enabledLeds));
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LED_ON);

  Serial.begin(115200);
  strip.Begin();
  strip.Show();

  setup_wifi();

#ifdef HTTPUpdateServer
  setup_http_server();
#endif

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  digitalWrite(LED_BUILTIN, LED_OFF);
  checkConnection();
  publishAttrChange();
}

void loop()
{
  checkConnection();
  client.loop();
  timer.run();

  if (transitionCounter > 0)
  {
    float multiplier = map(transitionCounter, 0, 255, 0, 1000) / 1000.f;
    if (on)
    {
      multiplier = 1.f - multiplier;
    }
    for (int i = 0; i < NUM_LEDS; i++)
    {
      if (enabledLeds[i / 8] >> (7 - (i % 8)) & 1)
      {
        strip.SetPixelColor(i, RgbwColor(stripLeds[i].R * multiplier, stripLeds[i].G * multiplier, stripLeds[i].B * multiplier, stripLeds[i].W * multiplier));
      }
      else
      {
        strip.SetPixelColor(i, RgbwColor(0, 0, 0, 0));
      }
    }
  }
  else if (on)
  {
    for (int i = 0; i < NUM_LEDS; i++)
    {
      if (enabledLeds[i / 8] >> (7 - (i % 8)) & 1)
      {
        strip.SetPixelColor(i, stripLeds[i]);
      }
      else
      {
        strip.SetPixelColor(i, RgbwColor(0, 0, 0, 0));
      }
    }
  }
  else
  {
    for (int i = 0; i < NUM_LEDS; i++)
    {
      strip.SetPixelColor(i, RgbwColor(0, 0, 0, 0));
    }
  }

  strip.Show();

#ifdef HTTPUpdateServer
  httpUpdateServer.handleClient();
#endif
}
