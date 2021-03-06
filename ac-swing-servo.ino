#include <Arduino.h>

#include <Servo.h>
Servo myservo;
const uint8_t SERVO_PIN = 9;

#include <IRremote.h>
const uint8_t RECV_PIN = 11;
IRrecv irrecv(RECV_PIN);
decode_results results;

const uint8_t LED_PIN = 13;


/** Led 13 blink on known command received */
namespace Led {

  enum LED_MODE {
    LED_IDLE,
    LED_OFF,
    LED_ON,
    LED_BLINK,
    LED_FLASH,
  };

  bool state;
  uint8_t mode;

  unsigned long millisStart; 
  unsigned int ledOnDuration; //automatically goes LED_OFF after x ms

  unsigned long flashingCycleStart;
  unsigned int flashingInterval;

  void on() {
    mode = LED_ON;
    // state = HIGH;
    // digitalWrite(LED_PIN, HIGH);
  }

  void off() {
    mode = LED_OFF;
    // state = LOW;
    // digitalWrite(LED_PIN, LOW);
  }

  void blink(unsigned int duration) {
    mode = LED_BLINK;
    ledOnDuration = duration;
    millisStart = millis();
    state = HIGH;
    digitalWrite(LED_PIN, state);
  }

  void flash(unsigned int duration, unsigned int interval) {
    mode = LED_FLASH;
    ledOnDuration = duration;
    flashingInterval = interval;
    flashingCycleStart = millisStart = millis();
    state = LOW;
    digitalWrite(LED_PIN, state);
  }

  void routine(unsigned long currentMillis) {
    switch(mode){
      case LED_IDLE:
        //noop
      break;
      case LED_OFF:
        mode = LED_IDLE;
        state = LOW;
        digitalWrite(LED_PIN, state);
        break;
      case LED_ON:
        mode = LED_IDLE;
        state = HIGH;
        digitalWrite(LED_PIN, state);
        break;
      case LED_BLINK: 
        if (currentMillis - millisStart > ledOnDuration) {
          mode = LED_IDLE;
          state = LOW;
          digitalWrite(LED_PIN, state);
        }
        break;
      case LED_FLASH:
        if  ((currentMillis - flashingCycleStart) > flashingInterval) {
          flashingCycleStart = currentMillis;
          state = !state;
          digitalWrite(LED_PIN, state);
        }
        if (currentMillis - millisStart > ledOnDuration) {
          state = LOW;
          digitalWrite(LED_PIN, state);
          mode = LED_IDLE;
        }
        break;
    }
  }

}



namespace ServoProgram {

  const uint8_t ANGLE_MIN = 100;
  const uint8_t ANGLE_MAX = 180;
  const uint8_t ANGLE_INITIAL = ANGLE_MIN + ((ANGLE_MAX - ANGLE_MIN) / 2) + 10; //middle

  const uint8_t SWING_ANGLE_MIN = ANGLE_MIN;
  const uint8_t SWING_ANGLE_MAX = ANGLE_MAX - 25; //reduce down swing

  uint8_t angle = ANGLE_INITIAL; //current angle
  unsigned long startedMillis; //used for delays

  uint8_t stepSize;
  unsigned int stepDelay;

  uint8_t parkStep;

  uint8_t seekTargetAngle = ANGLE_MAX;

  const unsigned int IDLE_TIMEOUT = 3000; //turn servo off after idling for this long

  enum modes {
    MODE_OFF,
    MODE_IDLE,
    MODE_PARK,
    MODE_SEEK,
    MODE_SWING,
  };

  uint8_t mode = MODE_IDLE;

  void off() {
    Serial.print("\nOFF");

    mode = MODE_OFF;
    myservo.detach();
  }

  void idle() {
    Serial.print("\nIDLE");

    mode = MODE_IDLE;
    stepDelay = IDLE_TIMEOUT;
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
    stepDelay = 151; //roughtly 7s from min to max ((SWING_ANGLE_MIN - SWING_ANGLE_MAX) / stepSize * stepDelay)
    mode = MODE_SWING;
  }

  void park() {
    Serial.print("\nPARK");
    parkStep = 0;
    stepDelay = 0;
    mode = MODE_PARK;
  }



  void idleRoutine(unsigned long currentMillis) {
    if (currentMillis - startedMillis > stepDelay) { //after idling for this long
      startedMillis = currentMillis;
      off(); //turn off
    }
  }

  //the park routine wiggles the serve over and under the desired angle in order to settle accurately 
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

      myservo.write(a);

      Serial.print(' ');
      Serial.print(a);
    }
  }


  void seekRoutine(unsigned long currentMillis) {
    if (angle != seekTargetAngle) {
      if (currentMillis - startedMillis > stepDelay) {
        startedMillis = currentMillis;

        //integrate
        if (angle < seekTargetAngle) {
          angle += stepSize;
          if (seekTargetAngle - angle < stepSize) angle = seekTargetAngle; //final step
        }
        else {
          angle -= stepSize;
          if (angle - seekTargetAngle < stepSize) angle = seekTargetAngle; //final step
        }

        //constrain
        if (angle <= ANGLE_MIN) seekTargetAngle = angle = ANGLE_MIN;
        else if (angle >= ANGLE_MAX) seekTargetAngle = angle = ANGLE_MAX;

        myservo.write(angle);

        Serial.print(' ');
        Serial.print(angle);
      }
    }
    else {
      park();
      // Led::blink(100);
    }
  }

  void swingRoutine(unsigned long currentMillis) {

    if (currentMillis - startedMillis > stepDelay) {
      startedMillis = currentMillis;

      //integrate
      if (angle < seekTargetAngle) angle += stepSize;
      else angle -= stepSize;

      //constrain
      if (angle <= SWING_ANGLE_MIN) angle = SWING_ANGLE_MIN;
      else if (angle >= SWING_ANGLE_MAX) angle = SWING_ANGLE_MAX;

      //if reached a stable point, make it oscilate
      if (angle == SWING_ANGLE_MIN) seekTargetAngle = SWING_ANGLE_MAX;
      else if (angle == SWING_ANGLE_MAX) seekTargetAngle = SWING_ANGLE_MIN;
      else if (angle == seekTargetAngle) seekTargetAngle = SWING_ANGLE_MAX;

      myservo.write(angle);

      Serial.print(' ');
      Serial.print(angle);
    }
  }


  void routine(unsigned long currentMillis) {
    switch (mode) {
      case MODE_OFF:
        //do nothing
        break;
      case MODE_IDLE:
        idleRoutine(currentMillis);
        break;
      case MODE_PARK:
        parkRoutine(currentMillis);
        break;
      case MODE_SEEK:
        seekRoutine(currentMillis);
        break;
      case MODE_SWING:
        swingRoutine(currentMillis);
        break;
    }
  }
}



namespace Remote {

  enum ELGIN_REMOTE {
    ELGIN_AUTO = 0x1FE6996,
    ELGIN_ALTA = 0x1FE9966,
    ELGIN_MEDIA = 0x1FED926,
    ELGIN_BAIXA = 0x1FE59A6,
    ELGIN_UP = 0x1FE29D6,
    ELGIN_DOWN = 0x1FEA956,
    ELGIN_FUNCAO = 0x1FEB946,
    ELGIN_TIMER = 0x1FE7986,
    ELGIN_DORMIR = 0x1FEF906,
    ELGIN_POWER = 0x1FE39C6,
  };

  const unsigned int LONGPRESS_DURATION = 500; //time for a button to be considered as holding
  const unsigned int REPEAT_THROTTLE = 300; //throttle for hold to repeat action


  unsigned long previousMillis;

  unsigned long lastCode;
  unsigned long lastCodeMillis;

  bool preventRepeat = false;

  bool seekMode = false;
  const unsigned int SEEK_MODE_DURATION = 10000;
  unsigned long seekModeStart;

  


  void singlePress(unsigned long currentMillis, unsigned long code){
    switch (code) {

      case ELGIN_POWER: //when turning ac on/off always stop
        seekMode = false;
        Led::blink(250);
        if (ServoProgram::mode == ServoProgram::MODE_SWING) { //stop swing
          ServoProgram::seek(ServoProgram::ANGLE_INITIAL);
        }
        else{
          ServoProgram::park();
        }
        break;

        // case ELGIN_AUTO:
        //   if (ServoProgram::mode == ServoProgram::MODE_SWING) { //stop swing
        //     ServoProgram::park();
        //     Led::flash(1500, 250); //-_-_-_
        //   }
        //   break;

      case ELGIN_ALTA:
        if (seekMode) {
          seekModeStart = currentMillis; //renew timeout
          if (ServoProgram::angle > ServoProgram::ANGLE_MIN) {
            ServoProgram::seek(ServoProgram::angle - 5);
            Led::blink(SEEK_MODE_DURATION);
          }
        }
        break;

      case ELGIN_BAIXA:
        if (seekMode) {
          seekModeStart = currentMillis; //renew timeout
          if (ServoProgram::angle < ServoProgram::ANGLE_MAX) {
            ServoProgram::seek(ServoProgram::angle + 5);
            Led::blink(SEEK_MODE_DURATION);
          }
        }
        break;

        case ELGIN_MEDIA:
          if (seekMode) { //stop seekmode
            seekModeStart = currentMillis - SEEK_MODE_DURATION; //force expire
          }
          break;
    }
  }

  void longPress(unsigned long currentMillis, unsigned long code){
    preventRepeat = true;

    switch (code) { // long press actions

      case ELGIN_AUTO:
        seekMode = false;
        // preventRepeat = true; //only trigger once
        if (ServoProgram::mode == ServoProgram::MODE_SWING) { //stop swing
          ServoProgram::park();
          Led::flash(1500, 250); //-_-_-_
        }
        else {
          ServoProgram::swing();
          Led::blink(10000);
        }
        break;

      case ELGIN_MEDIA:
        // preventRepeat = true; //only trigger once
        if(!seekMode){
          seekMode = true;
          seekModeStart = currentMillis;

          // ServoProgram::seek(155); //center
          ServoProgram::park();
          Led::blink(SEEK_MODE_DURATION);
        }
        break;

        default:
          preventRepeat = false;
        }
  }

  void repeatPress(unsigned long currentMillis, unsigned long code){
  }

  void routine(unsigned long currentMillis) {

    //timeout seekMode
    if(seekMode && currentMillis-seekModeStart>SEEK_MODE_DURATION){
      seekMode = false;
      ServoProgram::park();
      Led::flash(1500, 250); //_-_-_-
    }

    if (irrecv.decode(&results)) {
      unsigned long code = results.value;

      // Serial.print("\nCODE:");
      // Serial.print(code, HEX);

      // handle NEC REPEAT CODE
      if (code != REPEAT) {
        lastCode = code;
        lastCodeMillis = currentMillis;
        preventRepeat = false; //this is a new code
      }
      else { //repeat
        // code = lastCode;
      }

     if(code!=REPEAT){
        singlePress(currentMillis, code);
     }
     else {
        if(currentMillis - previousMillis < REPEAT_THROTTLE){
          repeatPress(currentMillis, lastCode);
          // previousMillis = currentMillis;
        }
        unsigned int holdTime = currentMillis - lastCodeMillis;
        if (preventRepeat == false && holdTime > LONGPRESS_DURATION) {
          longPress(currentMillis, lastCode);
        }
      }

      irrecv.resume();
    }
  }
  

}



namespace SerialProgram {

  void routine(unsigned long currentMillis){
    if (Serial.available() > 0) {
      char c = Serial.read();
          int a = Serial.parseInt();
      switch(c){
        // simulated remote
        case 't': //single press POWER
          Remote::singlePress(currentMillis, Remote::ELGIN_POWER);
          break;
        case 'q': //single press AUTO
          Remote::singlePress(currentMillis, Remote::ELGIN_AUTO);
          break;
        case 'Q': //long press AUTO
          Remote::longPress(currentMillis, Remote::ELGIN_AUTO);
          break;
        case 'w': //single press ALTA
          Remote::singlePress(currentMillis, Remote::ELGIN_ALTA);
          break;
        case 'e': //single press MEDIA
          Remote::singlePress(currentMillis, Remote::ELGIN_MEDIA);
          break;
        case 'E': //long press MEDIA
          Remote::longPress(currentMillis, Remote::ELGIN_MEDIA);
          break;
        case 'r': //single press BAIXA
          Remote::singlePress(currentMillis, Remote::ELGIN_BAIXA);
          break;

        // debug
        case 's': //swing
          ServoProgram::swing();
          break;
        case 'a': //seek to a angle (e.g. "a90")
          ServoProgram::seek(a);
          break;

      }
    }
  }

}



void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(A3, INPUT); //Float this pin, hardware layout this pin is bridged to pin 11

  Serial.begin(9600);
  Serial.print("\nBOOT\n");

  irrecv.enableIRIn();

  myservo.attach(SERVO_PIN);

  Led::blink(500);

  //Home servo
  myservo.write(ServoProgram::ANGLE_MAX);
  delay(500);
  myservo.write(ServoProgram::ANGLE_MIN);
  delay(1000);

  ServoProgram::seek(ServoProgram::ANGLE_INITIAL);
  ServoProgram::park();

}

void loop() {
  SerialProgram::routine(millis());
  Remote::routine(millis());
  ServoProgram::routine(millis());
  Led::routine(millis());
}