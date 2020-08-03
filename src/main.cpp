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
Effect effect = eOff;

WiFiClient espClient;
PubSubClient client(espClient);
bool boot = true;
char charPayload[50];
int sunriseDuration = NUM_LEDS;

uint8_t red = 0;
uint8_t green = 0;
uint8_t blue = 0;
uint8_t white = 0;

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
      stopEffect();
      effect = eOff;
    }
    else if (strcmp(charPayload, "ON") == 0)
    {
      if (effect == eOff)
      {
        stopEffect();
        effect = eStable;
        for (int i = 0; i < NUM_LEDS; i++)
        {
          stripLeds[i] = RgbwColor(red, green, blue, white);
        }
        client.publish(USER_MQTT_CLIENT_NAME "/effect", "stable", true);
        client.publish(USER_MQTT_CLIENT_NAME "/effectState", "stable", true);
      }
    }
    client.publish(USER_MQTT_CLIENT_NAME "/state", charPayload, true);
  }
  else if (newTopic == USER_MQTT_CLIENT_NAME "/effect")
  {
    stopEffect();
    if (strcmp(charPayload, "stable") == 0)
    {
      effect = eStable;
      for (int i = 0; i < NUM_LEDS; i++)
      {
        stripLeds[i] = RgbwColor(red, green, blue, white);
      }
    }
    else if (strcmp(charPayload, "colorloop") == 0)
    {
      effect = eColorLoop;
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
    stopEffect();
    effect = eStable;
    client.publish(USER_MQTT_CLIENT_NAME "/whiteState", charPayload, true);
    client.publish(USER_MQTT_CLIENT_NAME "/effect", "stable", true);
    client.publish(USER_MQTT_CLIENT_NAME "/effectState", "stable", true);
    white = intPayload;
    for (int i = 0; i < NUM_LEDS; i++)
    {
      stripLeds[i] = RgbwColor(red, green, blue, white);
    }
  }
  else if (newTopic == USER_MQTT_CLIENT_NAME "/color")
  {
    stopEffect();
    effect = eStable;
    client.publish(USER_MQTT_CLIENT_NAME "/colorState", charPayload, true);
    client.publish(USER_MQTT_CLIENT_NAME "/effect", "stable", true);
    client.publish(USER_MQTT_CLIENT_NAME "/effectState", "stable", true);
    int firstIndex = newPayload.indexOf(',');
    int lastIndex = newPayload.lastIndexOf(',');

    if ((firstIndex > -1) && (lastIndex > -1) && (firstIndex != lastIndex))
    {
      red = newPayload.substring(0, firstIndex).toInt();
      green = newPayload.substring(firstIndex + 1, lastIndex).toInt();
      blue = newPayload.substring(lastIndex + 1).toInt();
      for (int i = 0; i < NUM_LEDS; i++)
      {
        stripLeds[i] = RgbwColor(red, green, blue, white);
      }
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
          boot = false;
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

  switch (effect)
  {
  case eSunrise:
    sunRise();
    break;
  case eOff:
    for (int i = 0; i < NUM_LEDS; i++)
    {
      strip.SetPixelColor(i, RgbwColor(0, 0, 0, 0));
    }
    break;
  default:
    for (int i = 0; i < NUM_LEDS; i++)
    {
      strip.SetPixelColor(i, stripLeds[i]);
    }
    break;
  }

  strip.Show();

#ifdef HTTPUpdateServer
  httpUpdateServer.handleClient();
#endif
}
