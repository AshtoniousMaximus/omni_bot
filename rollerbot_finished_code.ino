// includes the servo library
#include <Servo.h>
// makes headServo recognized as a servo motor
Servo headServo;

// assigns the reciever channels to their designated pins
#define CH1 50
#define CH2 51
#define CH3 52
#define CH4 53
#define SWITCH 11

// assigns integers for the raspberry pi serial input
int pi1;
int pi2;
int pi3;

// selects a pin for the servo
const int HEAD_PIN = 40;

// assigns an integer for each channel value
int ch1Value; 
int ch2Value; 
int ch3Value; 
int ch4Value; 

// assigns the pins for the motor signal wires
int m1left = 2;
int m1right = 3;
int m2left = 4;
int m2right = 5;
int m3left = 6;
int m3right = 7;
int m4left = 8;
int m4right = 9;

// creates base values for each channel prevous value
float ch1Prev = 0;
float ch2Prev = 0;
float ch3Prev = 90;
float ch4Prev = 0;

// creates a setup function
void setup() {
  // starts a serial to read from the raspberry pi
  Serial1.begin(115200);
  // starts a serial to print out the values of the channels and the motors
  Serial.begin(115200);

  // attaches the headServo to the headpin which is 40
  headServo.attach(HEAD_PIN);

  // assigns the reciever pins as inputs
  pinMode(CH1, INPUT);
  pinMode(CH2, INPUT);
  pinMode(CH3, INPUT);
  pinMode(CH4, INPUT);
  pinMode(SWITCH, INPUT);

  // assigns motor pins for motor control as outputs
  pinMode(m1left, OUTPUT);
  pinMode(m1right, OUTPUT);

  pinMode(m2left, OUTPUT);
  pinMode(m2right, OUTPUT);

  pinMode(m3left, OUTPUT);
  pinMode(m3right, OUTPUT);

  pinMode(m4left, OUTPUT);
  pinMode(m4right, OUTPUT);
}

// creates a function to map channel inputs
int readChannel(int channelInput, int minLimit, int maxLimit, int defaultValue) {
  // reads the channel and waits 30 miliseconds for a response
  int ch = pulseIn(channelInput, HIGH, 30000);
  // if the input is smaller than 100 then it returns the default value
  if (ch < 100) return defaultValue;
  // returns a mapped version of the channel with values between 1000 and 2000
  return map(ch, 1000, 2000, minLimit, maxLimit);
}

// creates  function to apply a deadband value
int applyDeadband(int value, int threshold = 50) {
  // if the signals are smaller than the threshold value than they are ignored entirely and changed to a zero
  if (abs(value) < threshold) return 0;
  // returns the value
  return value;
}

// creates a function to smooth out the input values to prevent harsh accelerations
int applySmoothing(int input, float alpha, float &previous) {
  // changes the input value by a multiple of alpha to change how quickly it responds
  float smoothed = (input * alpha) + (previous * (1.0 - alpha));
  previous = smoothed;
  // returns the smoothed value
  return (int)smoothed;
}

// creates a drive motor function that returns nothing
void driveMotor(int power, int in1, int in2) {
  // constrains the pwm signal to be no greater than 255 and down to 0
  int pwm = constrain(abs(power), 0, 255);
  
  // if the input power is greater than 15, it sets the motors to drive in one direction
  if (power > 15) {
    digitalWrite(in1, LOW);
    analogWrite(in2, pwm);
  }
  // if the input power is less than -10 than the motors are commanded to run in the opposite direction
  else if (power < -10) {
    digitalWrite(in2, LOW);
    analogWrite(in1, pwm);
  }
  // otherwise the motors are told to stop
  else {
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
  }
}

// starts the main control loop
void loop() {
  // reads the PWM signal from the switch and waits 50 miliseconds for a response (25 miliseconds wasn't long enough)
  int switchValue = pulseIn(SWITCH, HIGH, 50000);

  // checks if the raspberry pi serial is available
  if (Serial1.available()){
    // if it is available, it reads the input with a new line after every signal
    String input = Serial1.readStringUntil('\n');

    // sets base values for every pwm signal
    int pwm1 = 1500, pwm2 = 1500, pwm3 = 1500;
    // scans the input string and extracts each pwm signal from the serial
    sscanf(input.c_str(), "%d,%d,%d", &pwm1, &pwm2, &pwm3);
    // maps the forward/back pwm into values between -255 and 255
    pi1 = map(pwm1, 1000, 2000, -255, 255);
    // maps the turn left/right pwm into values between -255 and 255
    pi2 = map(pwm2, 1000, 2000, -255, 255);
    // maps the look up/down pwm into values between 0 abd 180
    pi3 = map(pwm3, 1000, 2000, 0,180);
  }
  // // checks the pwm value from the switch and selects modes based on its value
  if (switchValue < 1300) {
    // if the value is less than 1300, then it is in mode one: only listen to reciever
    // applies a deadband to each channel to eliminate random signal spikes from driving the robot
    ch1Value = applyDeadband(readChannel(CH1, -255, 255, 0));
    ch2Value = applyDeadband(readChannel(CH2, -255, 255, 0));
    ch3Value = readChannel(CH3, 0, 180, 90);
    ch4Value = applyDeadband(readChannel(CH4, -255, 255, 0));
  }
  else if (switchValue < 1700) {
    // if the switch value is less than 1700, then it reads channel 1 and 2 from the reciever and channels 3 and 4 from the raspberry pi
    ch1Value = applyDeadband(readChannel(CH1, -255, 255, 0));
    ch2Value = applyDeadband(readChannel(CH2, -255, 255, 0));
    ch3Value = pi3;
    ch4Value = -pi2;
  }
  else {
    // otherwise it listens to the raspberry pi for everything but channel 1
    ch1Value = applyDeadband(readChannel(CH1, -255, 255, 0));
    ch2Value = pi1;
    ch3Value = pi3;
    ch4Value = -pi2;
  }

    // applies smoothing to each channel to stop abrupt movement
    ch1Value = applySmoothing(ch1Value, 0.25, ch1Prev);
    ch2Value = applySmoothing(ch2Value, 0.25, ch2Prev);
    ch3Value = applySmoothing(ch3Value, 0.03, ch3Prev);
    // has multiplied rotation value so it will react faster, but without overshooting due to too much torque
    ch4Value = applySmoothing(ch4Value, 0.5, ch4Prev)*(.15);

  // tells each motor to listen to each channel with
  int frontMotor = ch1Value - ch4Value;
  int rightMotor = -ch2Value - ch4Value;
  int backMotor = -ch1Value - ch4Value;
  int leftMotor = ch2Value - ch4Value;

  // writes the value of the channel3 input as an angle to the servo motor
  headServo.write(ch3Value);

  // constrains the motors between -255 and 255 for control
  leftMotor = constrain(leftMotor, -255, 255);
  rightMotor = constrain(rightMotor, -255, 255);
  frontMotor = constrain(frontMotor, -255, 255);
  backMotor = constrain(backMotor, -255, 255);

  // commands the motors to drive based on the values of their wire inputs
  driveMotor(leftMotor, m1left, m1right);
  driveMotor(rightMotor, m2left, m2right);
  driveMotor(frontMotor, m3left, m3right);
  driveMotor(backMotor, m4left, m4right);

  // prints out the channel values for debugging
  Serial.print("CH1: "); Serial.print(ch1Value);
  Serial.print(" | CH2: "); Serial.print(ch2Value);
  Serial.print(" | CH3: "); Serial.print(ch3Value);
  Serial.print(" | CH4: "); Serial.print(ch4Value);
  Serial.print(" | Left: "); Serial.print(leftMotor);
  Serial.print(" | Right: "); Serial.print(rightMotor);
  Serial.print(" | front: "); Serial.print(frontMotor);
  Serial.print(" | Back: "); Serial.println(backMotor);

  // adds a 10 milisecond delay
  delay(10);
}

