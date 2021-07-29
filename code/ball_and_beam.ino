#define ENCODER_OPTIMIZE_INTERRUPTS
#include <Encoder.h>       // https://www.arduino.cc/reference/en/libraries/encoder/
#include <MovingAverage.h> // https://www.arduino.cc/reference/en/libraries/movingaverager/

const uint8_t STEP = DD4;
const uint8_t DIR = DD5;
const uint8_t CH1 = DD2;
const uint8_t CH2 = DD3;
const uint8_t SENSOR_LEFT = A2;
const uint8_t SENSOR_RIGHT = A3;
const int FILTER_LENGTH = 80; // Length of the moving average filter, larger value will make it smoother but will also respond more slowly to changing inputs.
const int SPEED = 200;        // Effectively sets the step speed of the motor. In reality, sets the period in microseconds of the drive signal to the stepper driver. Smaller values means faster steps.
const int INTERVAL = 1;       // Delay in microseconds to set timing for the main loop.

const bool LEFT = 1;
const bool RIGHT = 0;

int left_position, right_position = 0; // Raw sensor values for left and right ball position.

MovingAverage left_filter(FILTER_LENGTH);  // Left ball position filter.
MovingAverage right_filter(FILTER_LENGTH); // Right ball position filter.

double left_position_filtered, right_position_filtered, position, offset = 0; // Filtered positions, final position and offset values for ball position.

double dt, timer = 0; // Timer and loop time counters.

double error_old, integral = 0; // PID variables.

int encoder_position = 0; // Raw rotary encoder position.

double angle, desired_angle = 0; // Angles.

Encoder encoder(CH1, CH2); // Optical rotary encoder.

void setup()
{
  Serial.begin(9600);
  pinMode(STEP, OUTPUT);
  pinMode(DIR, OUTPUT);

  digitalWrite(STEP, LOW);
  digitalWrite(DIR, LOW);

  encoder.write(0); // Set encoder to 0 position. (this means you need to have the beam level when you turn on the rig)

  // Calculate a stable 0 position for the optical position sensors by accumulating 5x the filter length and computing the 0 offset from this.
  for (int i = 0; i < FILTER_LENGTH * 5; i++)
  {
    readSensors(false); // false because we want the raw values here WITHOUT the offset applied.
  }
  offset = 0 - position;
}

// Usually the main working loop, in this case, we are just using it to keep time work() is where the work is done!
void loop()
{
  dt = (micros() - timer) / 1000000;
  timer = micros();

  work(); // Main working method.

  while (micros() - timer < INTERVAL)
  {
    //NOOP
  }
}

void work()
{
  readSensors(true);     // Read ball position.
  double moveTo = pid(); // Calculate angle to move to.
  moveToAngle(moveTo);   // Move to angle.

  /*
    Log data.
    
    logAngle() and logPosition() output data to be plotted using the Serial Plotter tool in the arduino IDE.
    
    logData() will output angle and ball position in CSV style for the Serial Monitor.
    
    NOTE: Using this changes the speed that the system runs at. Removing all of them will change the system dynamic and will require the PID controller to be tuned again.
  */

  //logAngle();
  //logPosition();
  logData();
}

double pid()
{
  double error;
  double derivative;
  error = (0 - position);
  integral += error * dt;
  derivative = (error - error_old) / dt;
  error_old = error;

  double kp = 0.07;
  double ki = 0.05;
  double td = 0.06;
  return (kp * error) + (ki * integral) + (td * derivative);
}

void moveToAngle(int newAngle)
{
  // These limits are to stop the rig hitting the table.
  if (newAngle > 25)
  {
    newAngle = 25;
  }

  if (newAngle < -25)
  {
    newAngle = -25;
  }

  double diff = newAngle - angle;
  double absDiff = abs(diff);

  int count = 0;
  int res = 4;                            // Setting a resolution because our motor us stepping in quarter stepping mode, so we want to get to within a quarter of a step.
  while (absDiff >= 1 / res && count < 3) // This loop allows the system to compensate up to 2 times for lost steps.
  {
    if (diff > 0)
    {
      step((int)absDiff * res, RIGHT);
    }
    else
    {
      step((int)absDiff * res, LEFT);
    }

    readSensors(true);
    diff = newAngle - angle;
    absDiff = abs(diff);
    count++;
  }
}

void step(int steps, bool direction)
{
  digitalWrite(DIR, direction);
  for (int i = 0; i < steps; i++)
  {
    digitalWrite(STEP, LOW);  // Stepper performs one step for each LOW -> HIGH transition.
    delayMicroseconds(SPEED); // Delay to set step speed.
    digitalWrite(STEP, HIGH);
    delayMicroseconds(SPEED);
    digitalWrite(STEP, LOW);
  }
}

void readSensors(bool withOffSet)
{
  encoder_position = encoder.read();
  angle = -1 * encoder_position / 16.0;                    // Scaling the encoder to give us roughly correct degrees.
  left_position = 19.7 + (analogRead(SENSOR_LEFT) / 3.33); // Scales the sensors taking into account ball diameter giving us an output in mm.
  left_position_filtered = left_filter.addSample((double)left_position);
  right_position = 19.7 + (analogRead(SENSOR_RIGHT) / 3.33);
  right_position_filtered = right_filter.addSample((double)right_position);
  position = right_position_filtered - left_position_filtered;

  if (withOffSet)
  {
    position = position + offset;
  }
}

void logPosition()
{
  Serial.print("position:");
  Serial.println(position);
}

void logAngle()
{
  Serial.print("angle:");
  Serial.println(angle);
}

void logData()
{
  Serial.print("position:");
  Serial.print(position);
  Serial.print(", ");
  Serial.print("angle:");
  Serial.println(angle);
}
