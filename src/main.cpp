#include <Arduino.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>

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

SimpleTimer timer;
NeoPixelBus<NeoGrbwFeature, Neo800KbpsMethod> strip(NUM_LEDS);
RgbwColor stripLeds[NUM_LEDS] = {};
Effect effect = eStable;
uint8_t red = 0;
uint8_t green = 0;
uint8_t blue = 0;
uint8_t white = 0;

WiFiClient espClient;
PubSubClient client(espClient);
bool boot = true;
bool on = true;
char charPayload[50];
int sunriseDuration = NUM_LEDS;

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  String newTopic = topic;
  Serial.print(topic);
  Serial.print("] ");
  memset(&charPayload, 0, sizeof(charPayload));
  memcpy(charPayload, payload, min<unsigned long>(sizeof(charPayload), (unsigned long)length));
  charPayload[length] = '\0';
  String newPayload = String(charPayload);
  int intPayload = newPayload.toInt();
  Serial.println(newPayload);
  Serial.println();
  newPayload.toCharArray(charPayload, newPayload.length() + 1);

  if (newTopic == USER_MQTT_CLIENT_NAME "/command")
  {
    if (strcmp(charPayload, "OFF") == 0)
    {
      on = false;
      stopEffect();
    }
    else if (strcmp(charPayload, "ON") == 0)
    {
      on = true;
      startEffect();
    }
    client.publish(USER_MQTT_CLIENT_NAME "/state", charPayload, true);
  }
  else if (boot && !on)
  {
    // Recently booted and effect is already set to off, that means we already got a OFF command.
    // Therefore all other (possibly "out-of-order") messages right after boot should be ignored to
    // prevent unintentionally turning the leds on after a reboot if the desired state is OFF.
    // Despite that color and white values should be restored to memory.
    if (newTopic == USER_MQTT_CLIENT_NAME "/white")
    {
      white = intPayload;
    }
    else if (newTopic == USER_MQTT_CLIENT_NAME "/color")
    {
      int firstIndex = newPayload.indexOf(',');
      int lastIndex = newPayload.lastIndexOf(',');
      if ((firstIndex > -1) && (lastIndex > -1) && (firstIndex != lastIndex))
      {
        red = newPayload.substring(0, firstIndex).toInt();
        green = newPayload.substring(firstIndex + 1, lastIndex).toInt();
        blue = newPayload.substring(lastIndex + 1).toInt();
      }
    }
  }
  else if (newTopic == USER_MQTT_CLIENT_NAME "/effect")
  {
    stopEffect();
    if (strcmp(charPayload, "stable") == 0)
    {
      effect = eStable;
      startEffect();
    }
    else if (strcmp(charPayload, "colorloop") == 0)
    {
      effect = eColorLoop;
      startEffect();
    }
    else if (strcmp(charPayload, "gradient") == 0)
    {
      effect = eGradient;
      startEffect();
    }
    else if (strcmp(charPayload, "sunrise") == 0)
    {
      effect = eSunrise;
      startSunrise(sunriseDuration);
    }
    client.publish(USER_MQTT_CLIENT_NAME "/effectState", charPayload, true);
  }
  else if (newTopic == USER_MQTT_CLIENT_NAME "/wakeAlarm")
  {
    stopEffect();
    effect = eSunrise;
    sunriseDuration = intPayload;
    startSunrise(intPayload);
    client.publish(USER_MQTT_CLIENT_NAME "/effect", "sunrise", true);
    client.publish(USER_MQTT_CLIENT_NAME "/effectState", "sunrise", true);
    client.publish(USER_MQTT_CLIENT_NAME "/state", "ON", true);
  }
  else if (newTopic == USER_MQTT_CLIENT_NAME "/white")
  {
    white = intPayload;
    switch (effect)
    {
    case eSunrise:
    case eColorLoop:
      // Setting the white value should stop effects that don't care about color
      effect = eStable;
      client.publish(USER_MQTT_CLIENT_NAME "/effect", "stable", true);
      client.publish(USER_MQTT_CLIENT_NAME "/effectState", "stable", true);
      break;
    default:
      break;
    }
    startEffect();
    client.publish(USER_MQTT_CLIENT_NAME "/whiteState", charPayload, true);
  }
  else if (newTopic == USER_MQTT_CLIENT_NAME "/color")
  {
    int firstIndex = newPayload.indexOf(',');
    int lastIndex = newPayload.lastIndexOf(',');
    if ((firstIndex > -1) && (lastIndex > -1) && (firstIndex != lastIndex))
    {
      red = newPayload.substring(0, firstIndex).toInt();
      green = newPayload.substring(firstIndex + 1, lastIndex).toInt();
      blue = newPayload.substring(lastIndex + 1).toInt();
      switch (effect)
      {
      case eSunrise:
      case eColorLoop:
        // Setting the color should stop effects that don't care about color
        effect = eStable;
        client.publish(USER_MQTT_CLIENT_NAME "/effect", "stable", true);
        client.publish(USER_MQTT_CLIENT_NAME "/effectState", "stable", true);
        break;
      default:
        break;
      }
      startEffect();
      client.publish(USER_MQTT_CLIENT_NAME "/colorState", charPayload, true);
    }
  }
}

void setup_wifi()
{
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  WiFi.mode(WIFI_STA);

  WiFi.hostname(USER_MQTT_CLIENT_NAME);

  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect()
{
  // Loop until we're reconnected
  int retries = 0;
  while (!client.connected())
  {
    if (retries < 150)
    {
      Serial.print("Attempting MQTT connection...");
      // Attempt to connect
      if (client.connect(mqtt_client_name, mqtt_user, mqtt_pass, USER_MQTT_CLIENT_NAME "/availability", 0, true, "offline"))
      {
        Serial.println("connected");
        client.publish(USER_MQTT_CLIENT_NAME "/availability", "online", true);
        if (boot == true)
        {
          client.publish(USER_MQTT_CLIENT_NAME "/checkIn", "rebooted");
        }
        else if (boot == false)
        {
          client.publish(USER_MQTT_CLIENT_NAME "/checkIn", "reconnected");
        }
        client.subscribe(USER_MQTT_CLIENT_NAME "/command");
        client.subscribe(USER_MQTT_CLIENT_NAME "/effect");
        client.subscribe(USER_MQTT_CLIENT_NAME "/color");
        client.subscribe(USER_MQTT_CLIENT_NAME "/white");
        client.subscribe(USER_MQTT_CLIENT_NAME "/wakeAlarm");
      }
      else
      {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
        retries++;
        // Wait 5 seconds before retrying
        delay(5000);
      }
    }
    else
    {
      ESP.restart();
    }
  }
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
}

void loop()
{
  if (!client.connected())
  {
    digitalWrite(LED_BUILTIN, LED_ON);
    reconnect();
    digitalWrite(LED_BUILTIN, LED_OFF);
  }
  client.loop();
  timer.run();

  if (on)
  {
    if (effect == eSunrise)
    {
      sunRise();
    }
    else
    {
      for (int i = 0; i < NUM_LEDS; i++)
      {
        strip.SetPixelColor(i, stripLeds[i]);
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

  if (boot && millis() > 30000)
  {
    boot = false;
  }
}
