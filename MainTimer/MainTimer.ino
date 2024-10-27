#include "CountDown.h"
#include "StopWatch.h"
#include <TM1637Display.h>

struct Timer {
  CountDown CD;
  StopWatch SW;
  unsigned long prevTime;
  bool isStarted;
  bool isSWMode;
};

unsigned long cdSec = 10;

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

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  display.setBrightness(4);

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
      timer.SW.stop();
      timer.isStarted = false;
    }
  } else {
    digitalWrite(MODE_LED, HIGH);

    if (timer.CD.isStopped() && millis() - timer.prevTime > 10) {
      timer.isStarted = false;
    }

    if (!digitalRead(START_BTN) && !timer.isStarted) {
      updateTimer(&timer);
      timer.CD.start(cdSec * 1000);
      timer.isStarted = true;
    }

    if (!digitalRead(RED_BTN) && timer.isStarted) {
      timer.CD.stop();
      timer.isStarted = false;
    }
  }

  updateTimer(&timer);
  //delay(1000); // Wait 1 second before the next measurement
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