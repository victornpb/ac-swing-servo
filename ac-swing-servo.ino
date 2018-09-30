#include <Servo.h>
Servo myservo;

const uint8_t SERVO_PIN = 9;

/** Led 13 blink on known command received */
namespace LedBlink {

const uint8_t pin = 13;
const unsigned int duration = 50; //hold for x ms

bool enabled;
static unsigned long ledBlinkTimeout;

void on() {
  enabled = true;
  ledBlinkTimeout = millis();
  digitalWrite(pin, HIGH);
}

void routine(unsigned long currentMillis) {
  if (enabled) {
    if (currentMillis - ledBlinkTimeout > duration) {
      enabled = false;
      digitalWrite(pin, LOW);
    }
  }
}

}



namespace ServoProgram {

const uint8_t ANGLE_MIN = 90;
const uint8_t ANGLE_MAX = 180;

uint8_t angle = ANGLE_MIN; //current angle
unsigned long startedMillis; //used for delays

uint8_t stepSize;
unsigned int stepDelay;

uint8_t parkStep;

uint8_t seekTargetAngle = ANGLE_MAX;

enum modes {
  MODE_OFF,
  MODE_IDLE,
  MODE_PARK,
  MODE_SEEK,
  MODE_SWING,
};

uint8_t mode = MODE_IDLE;

void off() {
  Serial.println("\nOFF");

  mode = MODE_OFF;
  myservo.detach();
}

void idle() {
  Serial.println("\nIDLE");

  mode = MODE_IDLE;
  stepDelay = 3000;
}

void seek(uint8_t a) {
  Serial.print("\nSEEK:");
  Serial.print(a);

  myservo.attach(SERVO_PIN);

  //constrain
  if (a < ANGLE_MIN) a = ANGLE_MIN;
  else if (a > ANGLE_MAX) a = ANGLE_MAX;

  mode = MODE_SEEK;
  stepSize = 10;
  stepDelay = 50;
  seekTargetAngle = a;
}

void swing() {
  Serial.print("\nSWING");
  myservo.attach(SERVO_PIN);

  stepSize = 1;
  stepDelay = 111; //roughtly 10s from min to max
  mode = MODE_SWING;
}



void park() {
  Serial.print("\nPARK");
  parkStep = 0;
  stepDelay = 0;
  mode = MODE_PARK;
}

void idleRoutine(unsigned long currentMillis) {

  if (currentMillis - startedMillis > stepDelay) {
    startedMillis = currentMillis;
    off();
  }
}

void parkRoutine(unsigned long currentMillis) {
  if (currentMillis - startedMillis > stepDelay) {
    startedMillis = currentMillis;
    uint8_t a = angle;

    parkStep++;
    if (parkStep == 0) {
      stepDelay = 30;
      a -= 8;
    }
    else if (parkStep == 1) {
      stepDelay = 30;
      a += 7;
    }
    else if (parkStep == 2) {
      stepDelay = 30;
      a -= 6;
    }
    else if (parkStep == 3) {
      stepDelay = 30;
      a += 5;
    }
    else if (parkStep == 4) {
      stepDelay = 30;
      a -= 4;
    }
    else if (parkStep == 5) {
      stepDelay = 30;
      a += 3;
    }
    else if (parkStep == 6) {
      stepDelay = 30;
      a -= 2;
    }
    else if (parkStep == 7) {
      stepDelay = 30;
      a += 1;
    }
    else if (parkStep == 8) {
      stepDelay = 100;
      a = angle;
    }
    else {
      idle();
      return;
    }

    Serial.print(' ');
    Serial.print(a);

    myservo.write(a);
  }
}


void seekRoutine(unsigned long currentMillis) {
  if (angle != seekTargetAngle) {
    if (currentMillis - startedMillis > stepDelay) {

      if (angle < seekTargetAngle) {
        angle += stepSize;
        if (seekTargetAngle - angle < stepSize) angle = seekTargetAngle;
      }
      else {
        angle -= stepSize;
        if (angle - seekTargetAngle < stepSize) angle = seekTargetAngle;
      }

      //constrain
      if (angle <= ANGLE_MIN) seekTargetAngle = angle = ANGLE_MIN;
      else if (angle >= ANGLE_MAX) seekTargetAngle = angle = ANGLE_MAX;

      startedMillis = currentMillis;

      Serial.print(' ');
      Serial.print(angle);

      myservo.write(angle);
    }
  }
  else {
    park();
    LedBlink::on();
  }
}

void swingRoutine(unsigned long currentMillis) {

  if (currentMillis - startedMillis > stepDelay) {
    startedMillis = currentMillis;

    //integrate
    if (angle < seekTargetAngle) angle += stepSize;
    else angle -= stepSize;

    //constrain
    if (angle <= ANGLE_MIN) {
      angle = ANGLE_MIN;
    }
    else if (angle >= ANGLE_MAX) {
      angle = ANGLE_MAX;
    }

    //reached stable point?
    if (angle == ANGLE_MIN) seekTargetAngle = ANGLE_MAX;
    else if (angle == ANGLE_MAX) seekTargetAngle = ANGLE_MIN;
    else if (angle == seekTargetAngle) seekTargetAngle = ANGLE_MAX;

    Serial.print(' ');
    Serial.print(angle);

    myservo.write(angle);
  }
}


void routine(unsigned long currentMillis) {
  switch (mode) {
    case MODE_PARK:
      parkRoutine(currentMillis);
      break;
    case MODE_SEEK:
      seekRoutine(currentMillis);
      break;
    case MODE_SWING:
      swingRoutine(currentMillis);
      break;
    case MODE_IDLE:
      idleRoutine(currentMillis);
      break;


  }
}


}

void setup() {
  // put your setup code here, to run once:

  myservo.attach(SERVO_PIN);
  pinMode(13, OUTPUT);

  //  myservo.write(180);
  //  delay(1000);
  //  myservo.write(90);
  //  delay(1000);

  Serial.begin(9600);
  Serial.println("\nBOOT");
}

void loop() {
  // put your main code here, to run repeatedly:
  if (Serial.available() > 0) {
    char c = Serial.read();

    if (c == 's') {
      ServoProgram::swing();
    }
    else if (c == 'a') {
      int a = Serial.parseInt();
      ServoProgram::seek(a);
    }
    else {
      //      Serial.println("garbage");
    }
  }

  unsigned long m = millis();

  ServoProgram::routine(m);
  LedBlink::routine(m);
}



