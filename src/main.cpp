#include <credentials.h>

#include <Arduino.h>

#include "DHT.h"
#include "esp_wifi.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

#define LED_BUILTIN   13
#define PIN_LED_BLUE  16
#define PIN_DHT       22
#define PIN_SOIL      32
#define PIN_POWER     34
#define PIN_LIGHT     33

RTC_DATA_ATTR int bootCount = 0;

// for deep sleep
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  6000ULL
uint64_t sleepus=0;

#define DHTTYPE DHT11

DHT dht(PIN_DHT, DHTTYPE);

//Other things
float moisture = 0.0;
float hum, temp;

void print_wakeup_reason()
{
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:
      Serial.println("Wakeup caused by external signal using RTC_IO");
      break;
    case ESP_SLEEP_WAKEUP_EXT1:
      Serial.println("Wakeup caused by external signal using RTC_CNTL");
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
      Serial.println("Wakeup caused by timer");
      break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
      Serial.println("Wakeup caused by touchpad");
      break;
    case ESP_SLEEP_WAKEUP_ULP:
      Serial.println("Wakeup caused by ULP program");
      break;
    default:
      Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason);
      break;
  }
}

int connectToWiFi(const char * ssid, const char * pwd)
{
#ifdef debug
  Serial.println("Connecting to WiFi network: " + String(ssid));
#endif

  WiFi.begin(ssid, pwd);

  int counter = 0;

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);

#ifdef debug
    Serial.print(".");
#endif

    counter++;
    if (counter > 120) {
      return 2;
    } // try to connect to the network for one minute.
  }

#ifdef debug  
  Serial.println();
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
#endif

  return 0;
}

void post_data() {
  if (connectToWiFi(WIFI_SSID, WIFI_PASSWORD) == 2) {
    WiFi.mode(WIFI_OFF); 
    return;
  }

  HTTPClient http;
      
  //post the humidity
  String aio_url = "http://io.adafruit.com/api/v2/" AIO_NAME "/feeds/" AIO_MOISTURE_FEED "/data";
  http.begin(aio_url);
  http.addHeader("X-AIO-Key", AIO_KEY);
  http.addHeader("Content-Type", "application/json");
  int httpResponseCode = http.POST("{\"value\":\"" + String(moisture) +"\"}");

  aio_url = "http://io.adafruit.com/api/v2/" AIO_NAME "/feeds/" AIO_TEMP_FEED "/data";
  http.begin(aio_url);
  http.addHeader("X-AIO-Key", AIO_KEY);
  http.addHeader("Content-Type", "application/json");
  httpResponseCode = http.POST("{\"value\":\"" + String(temp) +"\"}");

  aio_url = "http://io.adafruit.com/api/v2/" AIO_NAME "/feeds/" AIO_HUMIDITY_FEED "/data";
  http.begin(aio_url);
  http.addHeader("X-AIO-Key", AIO_KEY);
  http.addHeader("Content-Type", "application/json");
  httpResponseCode = http.POST("{\"value\":\"" + String(hum) +"\"}");
 
  // Free resources
  http.end();
  WiFi.mode(WIFI_OFF); 

  return;
}


void setup(void) {
  // put this right at the start of setup to minimise wasted power
  ++bootCount;

  sleepus = TIME_TO_SLEEP * uS_TO_S_FACTOR;
  esp_sleep_enable_timer_wakeup(sleepus);

  if (bootCount != 6) {
    esp_deep_sleep_start();
  }

  bootCount = 0; // this will only run on the 3rd boot.
  
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, 0);
  Serial.begin(115200);

  Serial.println("");
  Serial.print("boot count: ");
  Serial.println(bootCount);

  dht.begin();

  // delay for two minutes on boot to allow upload of a new sketch
  print_wakeup_reason();
  delay(2000);
}

void loop(void) {
  moisture = analogRead(32); //exponential smoothing of soil moisture

  Serial.print("Moisture: ");
  Serial.println(moisture);

  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  hum = dht.readHumidity();
  Serial.print("humidity: ");
  Serial.println(hum);

  // Read temperature as Celsius (the default)
  temp = dht.readTemperature();
  Serial.print("Temperature: ");
  Serial.println(temp);

  post_data();

  Serial.flush(); 
  delay(2000);
  esp_deep_sleep_start();
}
