#include "private.h"
#include <Preferences.h>
#include <EspMQTTClient.h>
#include <MQUnifiedsensor.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>

#define DEBUG                       // Uncomment this when you need to debug through serial

#define USER_LED_PIN                D0
#define USER_BUTTON_PIN             D1
#define BUZZER_PIN                  D2
#define MQ6_SENSOR_PIN              A0

#define LPG_DEFAULT_THRESHOLD       60.0

// MQTT topics
#define TOPIC_WEB_CONNECTION_STATUS "your-web-app-connection-status-topic"
#define TOPIC_LPG_THRESHOLD_FB      "your-lpg-threshold-feedback-topic"
#define TOPIC_BUZZER_TERMINATE      "your-buzzer-terminate-topic"
#define TOPIC_LPG_THRESHOLD         "your-lpg-threshold-topic"
#define TOPIC_LPG_ALERT             "your-lpg-alert=topic"
#define TOPIC_LPG_PPM               "your-lpg-ppm-topic"

#define SSID                        "your-ssid"
#define PASSWORD                    "your-password"

/* MQ-6 sensor configuration parameters */
#define BOARD                       ("ESP8266")
#define SENSOR_PIN                  (MQ6_SENSOR_PIN)
#define SENSOR_TYPE                 ("MQ-6")
#define VOLTAGE_RESOLUTION          (3.3)
#define ADC_BIT_RESOLUTION          (10)
#define MQ6_CLEAN_AIR_RATION        (10)

/* MQTT client parameters to connect to the HiveMQ MQTT broker */
EspMQTTClient lpg_client(
  SSID,
  PASSWORD,
  MQTT_BROKER_URL,
  "",
  "",
  "your-client-name",
  1883
);

MQUnifiedsensor MQ6(BOARD, VOLTAGE_RESOLUTION, ADC_BIT_RESOLUTION, SENSOR_PIN, SENSOR_TYPE);

WiFiClientSecure client;
HTTPClient http;
Preferences preferences;

int lpg_threshold = LPG_DEFAULT_THRESHOLD;

float lpg_ppm = 0.0;
float lpg_temp = 0.0;

bool buzzer_status = false;
bool button_press = false;
bool mqtt_button = false;
bool alert = false;

uint32_t current_time1 = 0;
uint32_t previous_time1 = 0;
uint32_t current_time2 = 0;
uint32_t previous_time2 = 0;

void ICACHE_RAM_ATTR terminate_buzzer(void)
{
  if(is_button_pressed())
  {
    button_press = true;
  }
}

void setup(void)
{
  #ifdef DEBUG
  Serial.begin(115200);
  #endif

  // Initialize Preferences
  preferences.begin("lpg-system", false);

  // Peripherals initialization
  peri_init();

  // MQ-6 sensor initialization
  mq6_init();

  // Retrieve or set default R0 value
  mq6_set_r0();

  // Wait for 5 seconds to check if the button is pressed
  check_button_calibrate();

  // Retrieve or set default threshold value
  mq6_set_threshold();

  client.setInsecure();

  uint8_t i = 2;
  while(i--)
  {
    digitalWrite(USER_LED_PIN, LOW);
    delay(500);
    digitalWrite(USER_LED_PIN, HIGH);
    delay(500);
  }

  #ifdef DEBUG
  Serial.println("Running Application...");
  #endif
}

void loop(void)
{
  lpg_client.loop();

  current_time1 = millis();
  current_time2 = millis();

  if (current_time1 - previous_time1 >= 1000)
  {
    MQ6.update();
    lpg_ppm = MQ6.readSensor();

    #ifdef DEBUG
    Serial.println("LPG: " + String(lpg_ppm) + " PPM");
    #endif

    if (lpg_ppm >= lpg_threshold && buzzer_status == false)
    {
      attachInterrupt(USER_BUTTON_PIN, terminate_buzzer, CHANGE);

      sendSMS();

      digitalWrite(BUZZER_PIN, HIGH);
      digitalWrite(USER_LED_PIN, LOW);
      buzzer_status = true;

      lpg_client.publish(TOPIC_LPG_PPM, String(lpg_ppm, 2), true);
      lpg_client.publish(TOPIC_LPG_ALERT, "lpg alert", true);
      alert = true;

      #ifdef DEBUG
      Serial.println("LPG threshold exceeded");
      #endif
    }

    if (buzzer_status && button_press || mqtt_button)
    {
      digitalWrite(BUZZER_PIN, LOW);
      digitalWrite(USER_LED_PIN, HIGH);
      buzzer_status = false;
      button_press = false;
      detachInterrupt(USER_BUTTON_PIN);
      lpg_client.publish(TOPIC_LPG_ALERT, "lpg normal", true);
    }
    previous_time1 = current_time1;
  }

  if(current_time2 - previous_time2 >= 5000)
  {
    if(lpg_temp != lpg_ppm)
    {
      lpg_client.publish(TOPIC_LPG_PPM, String(lpg_ppm, 2), true);
      lpg_temp = lpg_ppm;
    }
    previous_time2 = current_time2;
  }
}

void onConnectionEstablished()
{
  // Subscribe to the web connection status
  lpg_client.subscribe(TOPIC_WEB_CONNECTION_STATUS, [](const String & payload)
  {
    if(payload == "1")
    {
      lpg_client.publish(TOPIC_LPG_THRESHOLD_FB, String(lpg_threshold), true);
      #ifdef DEBUG
      Serial.println("web app Connected!");
      #endif
    }
  });

  // Subscribe to buzzer control topic
  lpg_client.subscribe(TOPIC_BUZZER_TERMINATE, [](const String & payload)
  {
    if(payload == "ON")
    {
      mqtt_button = true;
    }
    else
    {
      mqtt_button = false;
    }
    #ifdef DEBUG
    Serial.println("Topic: " + String(TOPIC_BUZZER_TERMINATE) + " Payload: " + payload);
    #endif
  });

  // Subscribe to gas threshold update topic
  lpg_client.subscribe(TOPIC_LPG_THRESHOLD, [](const String & payload)
  {
    #ifdef DEBUG
    Serial.println("Topic: " + String(TOPIC_LPG_THRESHOLD) + " Payload: " + payload);
    #endif
    int threshold = payload.toInt();
    if(preferences.getInt("threshold", LPG_DEFAULT_THRESHOLD) != threshold)
    {
      preferences.putInt("threshold", threshold); // Save threshold to Preferences
    }
  });
}

void peri_init(void)
{
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(USER_LED_PIN, OUTPUT);
  pinMode(USER_BUTTON_PIN, INPUT);

  #ifdef DEBUG
  Serial.println("Peripherals initialized");
  #endif
}

void mq6_init(void)
{
  #ifdef DEBUG
  lpg_client.enableDebuggingMessages();
  #endif

  MQ6.setRegressionMethod(1);
  MQ6.setA(1009.2);
  MQ6.setB(-2.35);
  MQ6.init();

  #ifdef DEBUG
  Serial.println("MQ-6 sensor initialized");
  #endif
}

void mq6_set_r0(void)
{
  float storedR0 = preferences.getFloat("r0", -1.0);

  if (storedR0 > 0) // Check if a valid R0 value is stored
  {
    MQ6.setR0(storedR0);

    #ifdef DEBUG
    Serial.printf("Retrieved R0 from memory: %.2f\n", storedR0);
    #endif
  }
  else
  {
    #ifdef DEBUG
    Serial.println("No valid R0 value found in memory. Calibration needed.");
    #endif
  }
}

void check_button_calibrate(void)
{
  bool button_state = false;
  digitalWrite(USER_LED_PIN, LOW);

  #ifdef DEBUG
  Serial.println("Press the button within 5 seconds to calibrate the sensor.");
  #endif

  ESP.wdtDisable();

  uint32_t start_time = millis();
  while (millis() - start_time < 5000)
  {
    if (is_button_pressed())
    {
      button_state = true;
      break;
    }
    yield();
  }

  ESP.wdtEnable(WDTO_8S);

  if(button_state)
  {
    digitalWrite(USER_LED_PIN, HIGH);

    #ifdef DEBUG
    Serial.println("Button pressed.");
    #endif

    calibrate_sensor();
  }
  else
  {
    digitalWrite(USER_LED_PIN, HIGH);

    #ifdef DEBUG
    Serial.println("Button not pressed.");
    #endif
  }
}

void mq6_set_threshold(void)
{
  lpg_threshold = preferences.getInt("threshold", LPG_DEFAULT_THRESHOLD);

  #ifdef DEBUG
  Serial.printf("LPG Threshold: %d\n", lpg_threshold);
  #endif
}

bool is_button_pressed()
{
  uint32_t debounce_time = millis();
  while (millis() - debounce_time < 50)
  {
    if (digitalRead(USER_BUTTON_PIN))
    {
      return true;
    }
  }
  return false;
}

void calibrate_sensor()
{
  #ifdef DEBUG
  Serial.println("Calibrating the sensor...");
  #endif

  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZER_PIN, LOW);

  float calcR0 = 0;
  for (int i = 1; i <= 10; i++)
  {
    MQ6.update();
    calcR0 += MQ6.calibrate(MQ6_CLEAN_AIR_RATION);
    digitalWrite(USER_LED_PIN, LOW);
    delay(500);
    digitalWrite(USER_LED_PIN, HIGH);
    delay(500);
  }
  calcR0 /= 10;
  MQ6.setR0(calcR0);

  // Store the calculated R0 value in Preferences
  preferences.putFloat("r0", calcR0);

  #ifdef DEBUG
  Serial.printf("Calibration completed! Stored R0: %.2f\n", calcR0);
  #endif

  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZER_PIN, LOW);
}

void sendSMS(void) {

  String var1 = "LPG gas level exceeded. And LPG";
  String var2 = String(lpg_ppm) + " PPM";

  int retry_count = 3; // Retry 3 times if SMS sending fails

  while (retry_count > 0)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      String apiUrl = CIRCUIT_DIGEST_URL + String(TEMPLATE_ID);
      http.begin(client, apiUrl);
      http.addHeader("Authorization", APIKEY);
      http.addHeader("Content-Type", "application/json");

      String payload = "{\"mobiles\":\"" + String(NUMBER) + "\",\"var1\":\"" + var1 + "\",\"var2\":\"" + var2 + "\"}";
      int httpResponseCode = http.POST(payload);

      if (httpResponseCode == 200)
      {
        #ifdef DEBUG
        Serial.println("SMS sent successfully!");
        Serial.println(http.getString());
        #endif
        break; // Exit loop on success
      }
      else
      {
        #ifdef DEBUG
        Serial.print("Failed to send SMS. Error code: ");
        Serial.println(httpResponseCode);
        #endif
      }
      retry_count--;
    }
    else
    {
      #ifdef DEBUG
      Serial.println("WiFi not connected!");
      #endif
      retry_count--;
    }
    delay(2000); // Wait before retrying
  }

  http.end(); // End connection
}
