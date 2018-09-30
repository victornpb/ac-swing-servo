#include <Arduino.h>
#include <Servo.h>
Servo myservo;

#include <IRremote.h>
const uint8_t RECV_PIN = 11;
IRrecv irrecv(RECV_PIN);
decode_results results;


const uint8_t SERVO_PIN = 9;

enum ELGIN {
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



/** Led 13 blink on known command received */
namespace Led {

const uint8_t pin = 13;
unsigned int duration = 50; //hold for x ms

bool state;
bool enabled;
unsigned long ledBlinkTimeout;

bool flashing = false;

unsigned int flashingInterval = 50;
unsigned long flashingTimeout;

  void blink(unsigned int d) {
    flashing = false;
    enabled = true;
    duration = d;
    ledBlinkTimeout = millis();
    digitalWrite(pin, HIGH);
  }
  
  void on(){
    state = true;
    digitalWrite(pin, HIGH);
  }

  void off(){
    state = false;
    digitalWrite(pin, LOW);
  }

  void flash(unsigned int d, unsigned int i){
    enabled = true;
    duration = d;
    flashingInterval = i;
    flashing = true;
    state = true;
    ledBlinkTimeout = millis();
  }

  void routine(unsigned long currentMillis)
  {
    if (enabled)
    {
      if (currentMillis - ledBlinkTimeout > duration)
      {
        enabled = false;
        flashing = false;
        digitalWrite(pin, LOW);
      }

      if (flashing && ((currentMillis - flashingTimeout) > flashingInterval))
      {
        flashingTimeout = currentMillis;
        state = !state;
        digitalWrite(pin, state);
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


enum Actions{
  Action_NoAction,
  Action_Swing,
  Action_VoldownAction,
  Action_MuteAction,
};

const int RepeatTimeout = 300; //ms

byte processRemote(){
    
    byte action = Action_NoAction;
    
    
    static unsigned long previousMillis;

    static unsigned long lastCode;
    static unsigned long lastCodeMillis;

    static bool preventRepeat = false;

    if(irrecv.decode(&results)) {
        unsigned long currentMillis = millis();

        unsigned long code = results.value;
        if(code!=0xFFFFFFFF){
          lastCode = code;
          lastCodeMillis = currentMillis;
          preventRepeat = false; //this is a new code
        }
        else { //repeat
          // code = lastCode;
        }

        // Serial.print(F("\nCODE:"));
        // Serial.print(code, HEX);
        
        
        switch(code){
            
            case ELGIN_AUTO:
                // action = Action_Swing;
                // actionTimestamp = currentMillis;
                 if(ServoProgram::mode==ServoProgram::MODE_SWING){
                    ServoProgram::park();
                    Led::flash(1500, 250); //-_-_-_
                 }

                break;

                case ELGIN_ALTA:
                
                if(ServoProgram::mode!=ServoProgram::MODE_SWING){

                  if(ServoProgram::angle>ServoProgram::ANGLE_MIN){
                    ServoProgram::seek(ServoProgram::angle-5);
                    Led::blink(500);
                  }
                }
                break;

                case ELGIN_BAIXA:
                 if(ServoProgram::mode!=ServoProgram::MODE_SWING){
                  if(ServoProgram::angle<ServoProgram::ANGLE_MAX){
                    ServoProgram::seek(ServoProgram::angle+5);
                    Led::blink(500);
                  }
                 }
                break;

            case REPEAT:
                // if(currentMillis - previousMillis < RepeatTimeout){
                //     if(lastAction == Action_Swing || lastAction == Action_VoldownAction){ //only enable repeat for this actions
                //         action = lastAction;
                //     }
                // }

                unsigned int holdTime = currentMillis - lastCodeMillis;
                if (holdTime > 600 && preventRepeat==false) { //after holding for this long


                  switch(lastCode){
                    case ELGIN_AUTO:
                        preventRepeat = true; //only trigger once

                        Serial.print("HOLD ");
                        Serial.print(lastCode);
                        Serial.println();



                        // if(ServoProgram::mode==ServoProgram::MODE_SWING){
                        //   ServoProgram::park();
                        //   Led::flash(1000, 100);
                        // }
                        // else{
                          ServoProgram::swing();
                          Led::blink(10000);
                        // }

                      break;

                      case ELGIN_MEDIA:
                        preventRepeat = true; //only trigger once

                        Serial.print("HOLD ");
                        Serial.print(lastCode);
                        Serial.println();

                        ServoProgram::seek(155);

                        // Led::flash(2000, 200);
                        Led::blink(3000);

                        // action = Action_Swing;
                      break;
                    }
                    
                }
                break;
        }

        irrecv.resume();

 
        
        // if(action != Action_NoAction) Led::blink(100);
    }

    return action;
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

    irrecv.enableIRIn();
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

  byte action = processRemote();

  unsigned long m = millis();

  ServoProgram::routine(m);
  Led::routine(m);
}



