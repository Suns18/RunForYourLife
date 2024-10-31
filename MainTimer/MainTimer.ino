#include "CountDown.h"
#include "StopWatch.h"
#include <TM1637Display.h>

//MQTT code
#include <WiFiS3.h>
#include <MQTTClient.h>

const char WIFI_SSID[] = "changeyourssid";          // CHANGE TO YOUR WIFI SSID
const char WIFI_PASSWORD[] = "changeyourpassword";  // CHANGE TO YOUR WIFI PASSWORD

const char MQTT_BROKER_ADRRESS[] = "mqtt-dashboard.com";  // CHANGE TO MQTT BROKER'S ADDRESS
//const char MQTT_BROKER_ADRRESS[] = "192.168.0.11";  // CHANGE TO MQTT BROKER'S IP ADDRESS
const int MQTT_PORT = 1883;
const char MQTT_CLIENT_ID[] = "runforyourlifeid";  // CHANGE IT AS YOU DESIRE
const char MQTT_USERNAME[] = "";                        // CHANGE IT IF REQUIRED, empty if not required
const char MQTT_PASSWORD[] = "";                        // CHANGE IT IF REQUIRED, empty if not required

// The MQTT topics that Arduino should publish/subscribe
const char PUBLISH_TOPIC[] = "runforurlife";       // CHANGE IT AS YOU DESIRE
const char SUBSCRIBE_TOPIC[] = "runforurlife";  // CHANGE IT AS YOU DESIRE

const int PUBLISH_INTERVAL = 5000;  // 5 seconds

WiFiClient network;
MQTTClient mqtt = MQTTClient(256);

unsigned long lastPublishTime = 0;
//end MQTT code

//Timer code
struct Timer {
  CountDown CD;
  StopWatch SW;
  unsigned long prevTime;
  bool isStarted;
  bool isSWMode;
};

//Set CountDown Time
unsigned long cdSec = 99;

bool isStarted = false;

CountDown CD(CountDown::MILLIS);
StopWatch SW(StopWatch::SECONDS);

Timer timer = {CD, SW, millis(), false, true};

unsigned int CLK = 2, DIO = 3;
TM1637Display display(CLK, DIO);

unsigned int MODE_BTN = 7, MODE_LED = 8;
unsigned int RED_BTN = 5, START_BTN = 6;

void updateTimer(Timer *time);

//ultrasonic sensor setting
const int trigPin = 9;
const int echoPin = 10;
long duration;
int distanceCm, distanceInch;
int startdistance[10];
int avg_distanceCm = 0;
int count = 0;
char text[15];

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  display.setBrightness(4);

  // MQTT connect code
  int status = WL_IDLE_STATUS;
  while (status != WL_CONNECTED) {
    Serial.print("Arduino UNO R4 - Attempting to connect to SSID: ");
    Serial.println(WIFI_SSID);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // wait 10 seconds for connection:
    delay(10000);
  }
  // print your board's IP address:
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  connectToMQTT();

  // timer.CD.start(cdSec);

  pinMode(MODE_BTN, INPUT_PULLUP);
  pinMode(MODE_LED, OUTPUT);
  pinMode(START_BTN, INPUT_PULLUP);
  pinMode(RED_BTN, INPUT_PULLUP);
  // ultrasonic pin
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  mqtt.loop();

  //start ultrasonic when timer start
  if (timer.isStarted){
    // Clear the trigPin
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2); //delay
    // Set the trigPin HIGH for 10 microseconds
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10); //delay
    digitalWrite(trigPin, LOW);
    // Read the echoPin, return the sound wave travel time in microseconds
    duration = pulseIn(echoPin, HIGH);
    // Calculate the distance in cm and inches
    distanceCm = duration * 0.034 / 2;
    distanceInch = duration * 0.0133 / 2;
    //When people pass the ultrasonic (finish the run)
    if (avg_distanceCm && distanceCm <= (avg_distanceCm / 2)){
      // Print the distances to the Serial Monitor
      Serial.print("Distance: ");
      Serial.print(distanceCm);
      Serial.println(" cm");
      //stop time
      timer.SW.stop();
      timer.CD.stop();
      if (timer.isSWMode){
        unsigned int sec = timer.SW.elapsed();
        unsigned int minute = sec / 60;
        sec = sec % 60;
        sprintf(text, "Time: %d:%02d", minute, sec);
      } else {
        unsigned int milsec = timer.CD.remaining();
        unsigned int sec = milsec / 1000;
        if (sec < 60){
          milsec = milsec % 1000;
          sprintf(text, "Time: %d.%03d s", sec, milsec);
        } else {
          unsigned int minute = sec / 60;
          sec = sec % 60;
          sprintf(text, "Time: %d:%02d", minute, sec);
        }
      }
      Serial.println(text);
      sendToMQTT(text);
      timer.isStarted = false;
    }
    if (!avg_distanceCm){
      if (count >= 5){
        for (int i = 0; i < count; i++){
          avg_distanceCm += startdistance[i];
        }
        avg_distanceCm = avg_distanceCm / count;
        Serial.print("Average: ");
        Serial.print(avg_distanceCm);
        Serial.println(" cm");
      } else {
        startdistance[count++] = distanceCm;
      }
    }
  } else {
    count = 0;
    avg_distanceCm = 0;
  }

  if (digitalRead(MODE_BTN) == LOW && !timer.isStarted) {
    timer.isSWMode = !timer.isSWMode;
    delay(200);
  }

  if (timer.isSWMode) {
    digitalWrite(MODE_LED, LOW);
    if (!digitalRead(START_BTN) && !timer.isStarted) {
      timer.SW.reset();
      updateTimer(&timer);
      timer.SW.start();
      timer.isStarted = true;
    }

    if (!digitalRead(RED_BTN) && timer.isStarted) {
      //button end StopWatch
      timer.SW.stop();
      
      unsigned int sec = timer.SW.elapsed();
      unsigned int minute = sec / 60;
      sec = sec % 60;
      sprintf(text, "Time: %d:%02d", minute, sec);
      Serial.println(text);
      sendToMQTT(text);

      timer.isStarted = false;
    }
  } else {
    digitalWrite(MODE_LED, HIGH);

    if (timer.CD.isStopped() && millis() - timer.prevTime > 10 && timer.isStarted) {
      sprintf(text, "Timeout");
      Serial.println(text);
      sendToMQTT(text);

      timer.isStarted = false;
      //timeout
    }

    if (!digitalRead(START_BTN) && !timer.isStarted) {
      updateTimer(&timer);
      timer.CD.start(cdSec * 1000);
      timer.isStarted = true;
    }

    if (!digitalRead(RED_BTN) && timer.isStarted) {
      //button end countdown
      timer.CD.stop();

      unsigned int milsec = timer.CD.remaining();
      unsigned int sec = milsec / 1000;
      if (sec < 60){
        milsec = milsec % 1000;
        sprintf(text, "Time: %d.%03d s", sec, milsec);
      } else {
        unsigned int minute = sec / 60;
        sec = sec % 60;
        sprintf(text, "Time: %d:%02d", minute, sec);
      }
      Serial.println(text);
      sendToMQTT(text);

      timer.isStarted = false;
    }
  }

  updateTimer(&timer);
}


void updateTimer(Timer *time) {
  if (time->isSWMode) {
    updateStopwatchTimer(time);
  } else {
    if (time->CD.remaining() > 60000) {
      countdownSecUpdate(time);
    } else {
      countdownMilliUpdate(time);
    }
  }
}

void updateStopwatchTimer(Timer *tmr) {
  unsigned int sec = tmr->SW.elapsed();
  unsigned long currTime = millis();

  unsigned int displayMin, displaySec;

  if (currTime - tmr->prevTime > 10) {
    tmr->prevTime = currTime;
    displayMin = sec / 60;
    displaySec = sec % 60;
    timerDisplay(displayMin * 100 + displaySec);
  }
}

void countdownSecUpdate(Timer *time) {
  unsigned int frontDigits, lastDigits;

  unsigned int currMillis = millis();
  unsigned int currTime = time->CD.remaining() / 1000;

  if (currMillis - time->prevTime > 10) {
    time->prevTime = currMillis;
    frontDigits = currTime / 60 % 100;
    lastDigits = currTime % 60;
    timerDisplay(frontDigits * 100 + lastDigits);
  }
}

void countdownMilliUpdate(Timer *time) {
  unsigned int frontDigits, lastDigits;

  unsigned int currMillis = millis();
  unsigned int currTime = time->CD.remaining() / 10;

  if (currMillis - time->prevTime > 10) {
    time->prevTime = currMillis;
    frontDigits = currTime / 100 % 100;
    lastDigits = currTime % 100;
    timerDisplay(frontDigits * 100 + lastDigits);
  }

}

void timerDisplay(unsigned int num) {
  // display.clear();
  display.showNumberDecEx(num, 0x40, true);
}

// MQTT funtion
void connectToMQTT() {
  // Connect to the MQTT broker
  mqtt.begin(MQTT_BROKER_ADRRESS, MQTT_PORT, network);

  // Create a handler for incoming messages
  mqtt.onMessage(messageHandler);

  Serial.print("Arduino UNO R4 - Connecting to MQTT broker");

  while (!mqtt.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  if (!mqtt.connected()) {
    Serial.println("Arduino UNO R4 - MQTT broker Timeout!");
    return;
  }

  // Subscribe to a topic, the incoming messages are processed by messageHandler() function
  if (mqtt.subscribe(SUBSCRIBE_TOPIC))
    Serial.print("Arduino UNO R4 - Subscribed to the topic: ");
  else
    Serial.print("Arduino UNO R4 - Failed to subscribe to the topic: ");

  Serial.println(SUBSCRIBE_TOPIC);
  Serial.println("Arduino UNO R4 - MQTT broker Connected!");
}

void sendToMQTT(char *text) {

  // int val = millis();
  String val_str = text;
  char messageBuffer[15];
  val_str.toCharArray(messageBuffer, 15);

  mqtt.publish(PUBLISH_TOPIC, messageBuffer);
  Serial.println("Arduino UNO R4 - sent to MQTT:");
  Serial.print("- topic: ");
  Serial.println(PUBLISH_TOPIC);
  Serial.print("- payload:");
  Serial.println(messageBuffer);
}

void messageHandler(String &topic, String &payload) {
  Serial.println("Arduino UNO R4 - received from MQTT:");
  Serial.println("- topic: " + topic);
  Serial.println("- payload:");
  Serial.println(payload);
}
