#include <Arduino.h>

const uint8_t LED_PIN = 13;
const uint8_t SENSOR_PIN = A2; // ADC used to sense if the AC unit is currently ON or OFF

#include <Servo.h>
Servo myservo;
const uint8_t SERVO_PIN = 9;

#include <IRremote.h>
const uint8_t RECV_PIN = 11;
IRrecv irrecv(RECV_PIN);
decode_results results;


namespace MainProg{
  void startup();
  void powerOn();
  void powerOff();
  void swing();
  void seek();
  void exitSeek();
  void enterSeek();
  void up();
  void down();
}

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

  const uint8_t ANGLE_MIN = 75;
  const uint8_t ANGLE_MAX = 170;
  const uint8_t ANGLE_INITIAL = 142; //ANGLE_MIN + ((ANGLE_MAX - ANGLE_MIN) / 2) + 20; //middle

  const uint8_t SWING_ANGLE_MIN = ANGLE_MIN;
  const uint8_t SWING_ANGLE_MAX = ANGLE_MAX - 10; //reduce down swing

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
    Serial.print("\nServo:OFF");

    mode = MODE_OFF;
    myservo.detach();
  }

  void idle() {
    Serial.print("\nServo:IDLE");

    mode = MODE_IDLE;
    stepDelay = IDLE_TIMEOUT;
  }

  void seek(uint8_t a) {
    Serial.print("\nServo:SEEK (");
    Serial.print(a);
    Serial.print(")");

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
    Serial.print("\nServo:SWING");
    myservo.attach(SERVO_PIN);

    stepSize = 1;
    stepDelay = 151; //roughtly 7s from min to max ((SWING_ANGLE_MIN - SWING_ANGLE_MAX) / stepSize * stepDelay)
    mode = MODE_SWING;
  }

  void park() {
    Serial.print("\nServo:PARK");
    parkStep = 0;
    stepDelay = 0;
    mode = MODE_PARK;
    if (angle != seekTargetAngle) angle = seekTargetAngle;
  }


  void center() {
    Serial.print("\nServo:CENTER");
    seek(ANGLE_INITIAL);  
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
        a -= 4;
      }
      else if (parkStep == 1) {
        stepDelay = 30;
        a += 4;
      }
      else if (parkStep == 2) {
        stepDelay = 30;
        a -= 3;
      }
      else if (parkStep == 3) {
        stepDelay = 30;
        a += 3;
      }
      else if (parkStep == 4) {
        stepDelay = 30;
        a -= 2;
      }
      else if (parkStep == 5) {
        stepDelay = 30;
        a += 2;
      }
      else if (parkStep == 6) {
        stepDelay = 30;
        a -= 1;
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
      else if (angle == seekTargetAngle) seekTargetAngle = SWING_ANGLE_MIN;

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

  bool debugRemote = false;

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
  
  void singlePress(unsigned long currentMillis, unsigned long code){
    switch (code) {

      case ELGIN_POWER: //when turning ac on/off always stop
          //MainProg::power();
        break;

      case ELGIN_AUTO:
        //   if (ServoProgram::mode == ServoProgram::MODE_SWING) { //stop swing
        //     ServoProgram::park();
        //     Led::flash(1500, 250); //-_-_-_
        //   }
        break;

      case ELGIN_ALTA:
        MainProg::up();
        break;

      case ELGIN_BAIXA:
        MainProg::down();
        break;

      case ELGIN_MEDIA:
        MainProg::exitSeek();
        break;
    }
  }

  void longPress(unsigned long currentMillis, unsigned long code){
    preventRepeat = true;

    switch (code) { // long press actions

      case ELGIN_AUTO:
        MainProg::swing();
        break;

      case ELGIN_MEDIA:
        MainProg::enterSeek();
        break;

      default:
        preventRepeat = false;
      }
  }

  void repeatPress(unsigned long currentMillis, unsigned long code) {
    switch (code) {
      case ELGIN_ALTA:
        MainProg::up();
        break;

      case ELGIN_BAIXA:
        MainProg::down();
        break;
    }
  }

  void routine(unsigned long currentMillis) {
    if (irrecv.decode(&results)) {
      unsigned long code = results.value;

      if (debugRemote){
         Serial.print("\n(debug) IRCODE:");
         Serial.print(code, HEX);
      }
      
      // handle NEC REPEAT CODE
      if (code != REPEAT) {
        lastCode = code;
        lastCodeMillis = currentMillis;
        preventRepeat = false; //this is a new code
      }
      else { //repeat
        // code = lastCode;
      }

     if (code != REPEAT){
        singlePress(currentMillis, code);
     }
     else {
        if (currentMillis - previousMillis > REPEAT_THROTTLE){
          previousMillis = currentMillis;
          repeatPress(currentMillis, lastCode);
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



namespace Sensor {
  bool debugSensor = false;
  
  const unsigned int poolingInterval = 100;
  const unsigned int threshold = 950; // Fluctuates around OFF=900~800 ON=990~1010
  uint8_t debounce = 10; // number of consecutive readings
  
  unsigned long previousMillis;
  uint8_t newStateCount = 0;
  bool state = false;

  void onChange(bool state){
    Serial.print("\n\n[MAIN BOARD] ");
    Serial.println(state?"ON":"OFF");
    
    if (state==true) MainProg::powerOn();
    else MainProg::powerOff();
  }

  void routine(unsigned long currentMillis) {
    if (currentMillis - previousMillis > poolingInterval) {
      previousMillis = currentMillis;

      unsigned int value = analogRead(SENSOR_PIN);
      bool s = value > threshold;

      if (debugSensor) {
        Serial.print("(Debug) Sensor:");
        Serial.print(value);
        Serial.print(" state:");
        Serial.print(s?"ON":"OFF");
        Serial.print(" reading:");
        Serial.print(newStateCount);
        Serial.println();
      }
      
      if (s != state){ // detected edge
        newStateCount++;

        if (newStateCount > debounce) { // only flip state after x readings
          state = s;
          newStateCount = 0;
           // dispatch state change
          onChange(state);
        }
      }
      else{
        newStateCount = 0; // clear new state
      }
    }
  }
  
}



namespace SerialProgram {

  void routine(unsigned long currentMillis) {
    if (Serial.available() > 0) {
      char c = Serial.read();
      int a = Serial.parseInt();
      
      switch(c) {
        case 'r': 
          MainProg::startup();
          break;
        case '1': 
          MainProg::powerOn();
          break;
        case '0': 
          MainProg::powerOff();
          break;
        case 'q': 
          MainProg::swing();
          break;
        case 'e': 
          MainProg::seek();
          break;
        case 'w': 
          MainProg::up();
          break;
        case 's': 
          MainProg::down();
          break;
        
        // debug
        case 'a': //seek to a angle (e.g. "a90")
          ServoProgram::seek(a);
          break;
        case 'p': //debug sensor ADC
          Sensor::debugSensor = !Sensor::debugSensor;
          break;
        case 'i': //debug remote IR codes
          Remote::debugRemote = !Remote::debugRemote;
          break;

        case 'h': //help
          Serial.println("\nHELP\n");
          Serial.println("h  print help");
          Serial.println("r  startup");
          Serial.println("1  powerOn");
          Serial.println("0  powerOff");
          Serial.println("q  toggle Swing");
          Serial.println("e  toggle Seek");
          Serial.println("w  up");
          Serial.println("s  down");
          Serial.println("");
          Serial.println("a  Seek to a angle (e.g. 'a90')");
          Serial.println("p  Debug ADC Sensor");
          Serial.println("i  Debug IR Remote Codes");
          break;
      }
    }
  }

}



namespace MainProg {
  bool isOn = false; // is AC on/off

  bool seekMode = false;
  const unsigned int SEEK_MODE_DURATION = 15000;
  unsigned long seekModeStart;

  uint8_t previousMode;
  uint8_t previousAngle = 85;

  void startup() {
    Serial.print("> Startup sequence\n");
    //Home servo
    myservo.write(ServoProgram::ANGLE_MAX);
    delay(500);
    myservo.write(ServoProgram::ANGLE_MIN);
    delay(1000);

    ServoProgram::center();
  }

  void powerOn() {
    //if (isOn) return;
    isOn = true;
    Serial.print("> Power ON\n");
    Led::blink(5000);
    
    // restore previous state
    if (previousMode == ServoProgram::MODE_SWING) {
      ServoProgram::swing();
    }
    else {
      ServoProgram::seek(previousAngle);
    }
  }

  void powerOff(){
    //if (!isOn) return;
    isOn = false;
    Serial.print("> Power OFF\n");
    
    // remember previous state
    previousMode = ServoProgram::mode;
    previousAngle = ServoProgram::angle;

    // power off sequence
    seekMode = false;
    Led::blink(500);
    ServoProgram::center();
  }

  void swing() {
    seekMode = false;
    // preventRepeat = true; //only trigger once
    if (ServoProgram::mode == ServoProgram::MODE_SWING) { //stop swing
      Serial.print("> Swing Stop\n");
      ServoProgram::park();
      Led::flash(1500, 250); //-_-_-_
    }
    else {
      Serial.print("> Swing\n");
      ServoProgram::swing();
      Led::blink(10000);
    }
  }


  void seek(){
    if(seekMode) exitSeek();
    else enterSeek();
  }

  void enterSeek(){
    // preventRepeat = true; //only trigger once
    if(!seekMode){
      Serial.print("> Enter Seek\n");
      seekMode = true;
      seekModeStart = millis();

      // ServoProgram::seek(155); //center
      ServoProgram::park();
      Led::blink(SEEK_MODE_DURATION);
    }
  }

  void exitSeek(){
    if (seekMode) { //stop seekmode
      Serial.print("> Exit Seek\n");
      seekModeStart = millis() - SEEK_MODE_DURATION; //force expire
    }
  }

  void up(){
    if (seekMode) {
      Serial.print("> Up\n");
      seekModeStart = millis(); //renew timeout
      if (ServoProgram::angle > ServoProgram::ANGLE_MIN) {
        ServoProgram::seek(ServoProgram::angle - 5);
        Led::blink(SEEK_MODE_DURATION);
      }
    }
  }

  void down(){
    if (seekMode) {
      Serial.print("> Down\n");
      seekModeStart = millis(); //renew timeout
      if (ServoProgram::angle < ServoProgram::ANGLE_MAX) {
        ServoProgram::seek(ServoProgram::angle + 5);
        Led::blink(SEEK_MODE_DURATION);
      }
    }
  }

  void routine(unsigned long currentMillis) {
    // timeout seekMode
    if(seekMode && currentMillis-seekModeStart>SEEK_MODE_DURATION){
      Serial.print("> Seek timeout\n");
      seekMode = false;
      ServoProgram::park();
      Led::flash(1500, 250); //_-_-_-
    }
  }
}



void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(SENSOR_PIN, INPUT);

  Serial.begin(115200);
  Serial.print("\nBOOT\n");
  Serial.println("\nVictor N (c) 2018 - https://github.com/victornpb/ac-swing-servo \n");

  myservo.attach(SERVO_PIN);

  Led::blink(500);

  irrecv.enableIRIn();

  MainProg::startup();
}

void loop() {
  MainProg::routine(millis());
  SerialProgram::routine(millis());
  Remote::routine(millis());
  ServoProgram::routine(millis());
  Led::routine(millis());
  Sensor::routine(millis());
}
