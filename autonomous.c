#include "main.h"

#include "config.h"

#define RIGHTANGLE 780

#define TURNENCODERPRECISION 6

#define MOVEENCODERPRECISION 7

#define NUMDRIVEMOTORS 6

#define VELOCITYLIMIT 3

#define GYRODRIFTRATE 0.0

#define KP 0.10   //0.12

#define KI 0.0025 //0.003

#define INTEGRALLIMIT 6

#define DRIVEP 0.100

#define DRIVEI 0.0018

#define DRIVED 0.055

#define CORRD 0.3 //0.75

#define TDRIVEP 0.145 //.130

#define TDRIVEI 0.0089

#define TDRIVED 0.07 //0.04

#define DRIVEINTEGRALLIMIT 1000

#define DRIVEMAXVEL 1

#define DRIVEPOSTOL 50

#define TDRIVEINTEGRALLIMIT 1000

#define TDRIVEMAXVEL 5

#define TDRIVEPOSTOL 90 //6

#define STARTHEADING 0

#define GYRO_START 0

#define TOPSENSOR 2525

//#define TOPSENSORINDEX 2400

#define BOTTOMSENSOR 2700

#define CORNERGOALTIMEOUT 2000

#define MIDDLEGOALTIMEOUT 1500

bool run_intake = false;

int intakeTimeout = 0;

bool run_flywheel = false;

int flywheelTimeout = 0;

int currentheading = 0;

uint32_t programStartTime = 0;

int  imu_sensor = 1;

adi_gyro_t gyro;

extern void middleGoal(bool auton, int timeout);

extern void middleGoalOneRed(bool auton, int timeout);

extern void cornerGoal(bool auton, int timeout);

extern void cornerGoalFast(bool auton, int timeout);

extern void cornerGoalOneRed(bool auton);

extern void cornerGoalOneRedFast(bool auton);

extern void middleGoal2and6(bool auton, int timeout);

task_t headingTask;
task_t intakeIndexTask;
task_t flywheelIndexTask;

#define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define min(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

void clearDriveMotors(){
    motor_tare_position(PORT_DRIVELEFTFRONT);
    motor_tare_position(PORT_DRIVELEFTMIDDLE);
    motor_tare_position(PORT_DRIVELEFTBACK);
    motor_tare_position(PORT_DRIVERIGHTFRONT);
    motor_tare_position(PORT_DRIVERIGHTMIDDLE);
    motor_tare_position(PORT_DRIVERIGHTBACK);
}

void assignDriveMotorsAuton(int power){
    motor_move(PORT_DRIVELEFTFRONT, power);
    motor_move(PORT_DRIVELEFTMIDDLE, power);
    motor_move(PORT_DRIVERIGHTFRONT, power);
    motor_move(PORT_DRIVELEFTBACK, power);
    motor_move(PORT_DRIVERIGHTMIDDLE, power);
    motor_move(PORT_DRIVERIGHTBACK, power);
}

void assignDriveMotorsPower(int rightSide, int leftSide){
    motor_move(PORT_DRIVELEFTFRONT, leftSide);
    motor_move(PORT_DRIVELEFTMIDDLE, leftSide);
    motor_move(PORT_DRIVELEFTBACK, leftSide);
    motor_move(PORT_DRIVERIGHTFRONT, rightSide);
    motor_move(PORT_DRIVERIGHTMIDDLE, rightSide);
    motor_move(PORT_DRIVERIGHTBACK, rightSide);
}

double averageVelocity(){
    return (abs(motor_get_actual_velocity(PORT_DRIVELEFTFRONT)) + abs(motor_get_actual_velocity(PORT_DRIVELEFTBACK)) +
        abs(motor_get_actual_velocity(PORT_DRIVERIGHTFRONT)) + abs(motor_get_actual_velocity(PORT_DRIVERIGHTBACK)) +
        abs(motor_get_actual_velocity(PORT_DRIVELEFTMIDDLE)) + abs(motor_get_actual_velocity(PORT_DRIVERIGHTMIDDLE)))/
        NUMDRIVEMOTORS;
}

void resetGyro(){
    adi_gyro_reset(gyro);
    imu_reset(IMU_PORT);
}

void getHeading(){
  int wrap = 0;
  int imucur = STARTHEADING;
  while(true){
      int previousHeading = imucur;
      imucur = imu_get_heading(IMU_PORT)*10 + STARTHEADING;
      if(previousHeading - imucur > 1000)
      {
        wrap ++;
      }
      else if(imucur - previousHeading > 1000)
      {
        wrap --;
      }
      currentheading = imucur + 3600*wrap;
      delay(5);
  }
}

void assignDriveMotorsDist(int leftSide, int rightSide, int power, bool clear, bool turn){
    if (clear)
    {
        clearDriveMotors();
    }

    int currentPrecision = MOVEENCODERPRECISION;
    if (turn)
    {
      currentPrecision = TURNENCODERPRECISION;
    }

    motor_move_absolute(PORT_DRIVELEFTFRONT, leftSide, power);
    motor_move_absolute(PORT_DRIVELEFTMIDDLE, leftSide, power);
    motor_move_absolute(PORT_DRIVELEFTBACK, leftSide, power);
    motor_move_absolute(PORT_DRIVERIGHTFRONT, rightSide, power);
    motor_move_absolute(PORT_DRIVERIGHTMIDDLE, rightSide, power);
    motor_move_absolute(PORT_DRIVERIGHTBACK, rightSide, power);
    delay(100);
    uint32_t timeout = millis();
    while ((abs(motor_get_position(PORT_DRIVELEFTFRONT) - leftSide) + abs(motor_get_position(PORT_DRIVELEFTBACK) - leftSide) + abs(motor_get_position(PORT_DRIVELEFTMIDDLE) - leftSide) +
      (abs(motor_get_position(PORT_DRIVERIGHTFRONT) - rightSide) + abs(motor_get_position(PORT_DRIVERIGHTBACK) - rightSide)) + abs(motor_get_position(PORT_DRIVERIGHTMIDDLE) - rightSide)) >
      currentPrecision * NUMDRIVEMOTORS) // ||
                                                  //(turn && (averageVelocity() >= VELOCITYLIMIT)))
    {
        uint32_t ctime = millis();
        if(ctime - timeout > 1100){
          break;
        }
        delay(20);
    }

    delay(250);
    assignDriveMotorsPower(0, 0);
}

void straightPID(int leftSide, int rightSide, int power, int heading){
    leftSide *= 200.0 / 257.0;
    rightSide *= 200.0 / 257.0;
    power *= 200.0 / 257.0;

    if (left)
    {
        heading = -heading;
    }

    clearDriveMotors();

    double curP = DRIVEP;
    double curI = DRIVEI;
    double curD = DRIVED;
    double CORR = CORRD;

    int leftErrorChange = 0;
    int rightErrorChange = 0;
    int leftError = 3 * leftSide;
    int rightError = 3 * rightSide;
    int prevLeftError = 3 * leftSide;
    int prevRightError = 3 * rightSide;
    int leftIntegral = 0;
    int rightIntegral = 0;

    bool endSequence = false;
    int startEnd = 0;
    int endTime = 500;

    while (abs(leftErrorChange) + abs(rightErrorChange) > DRIVEMAXVEL || (!endSequence || (millis() - startEnd < endTime)))
    {
           if (abs(leftError) + abs(rightError) <= DRIVEPOSTOL && !endSequence)
           {
              endSequence = true;
              startEnd = millis();
           }

           leftIntegral += leftError;
           rightIntegral += rightError;

           if (leftIntegral > DRIVEINTEGRALLIMIT)
           {
              leftIntegral = DRIVEINTEGRALLIMIT;
           }
           if (leftIntegral < -DRIVEINTEGRALLIMIT)
           {
              leftIntegral = -DRIVEINTEGRALLIMIT;
           }
           if (rightIntegral > DRIVEINTEGRALLIMIT)
           {
              rightIntegral = DRIVEINTEGRALLIMIT;
           }
           if (rightIntegral < -DRIVEINTEGRALLIMIT)
           {
              rightIntegral = -DRIVEINTEGRALLIMIT;
           }

           leftErrorChange = leftError - prevLeftError;
           rightErrorChange = rightError - prevRightError;

           prevLeftError = leftError;
           prevRightError = rightError;

           int rightCorrection = heading - currentheading;
           assignDriveMotorsPower(min(power, max(-power, leftError * DRIVEP + leftIntegral * DRIVEI + leftErrorChange * DRIVED + rightCorrection * CORR)), min(power, max(-power, rightError * DRIVEP + rightIntegral * DRIVEI + rightErrorChange * DRIVED - rightCorrection * CORR)));

           delay(5);

           leftError = 3 * leftSide - motor_get_position(PORT_DRIVELEFTFRONT) - motor_get_position(PORT_DRIVELEFTBACK) - motor_get_position(PORT_DRIVELEFTMIDDLE);
           rightError = 3 * rightSide - motor_get_position(PORT_DRIVERIGHTFRONT) - motor_get_position(PORT_DRIVERIGHTBACK) - motor_get_position(PORT_DRIVERIGHTMIDDLE);
        }
        assignDriveMotorsPower(0, 0);
     }

void turnGyro(int degrees, int power){
        uint32_t cur = millis();
        if (left)
        {
           degrees = -degrees;
        }

        double curP = TDRIVEP;
        double curI = TDRIVEI;
        double curD = TDRIVED;

        int errorChange = 0;
        int error = degrees - currentheading;
        int prevError = degrees - currentheading;
        int integral = 0;

        bool endSequence = false;
        int startEnd = 0;
        int endTime = 200; //500
                                                                                                                        //1155, 955
        while ((abs(errorChange) > TDRIVEMAXVEL || (!endSequence || (millis() - startEnd < endTime))) && millis() - cur < 800)
        {
           if (abs(error) <= TDRIVEPOSTOL && !endSequence)
           {
              endSequence = true;
              startEnd = millis();
           }

           integral += error;
           if (integral > TDRIVEINTEGRALLIMIT)
           {
              integral = TDRIVEINTEGRALLIMIT;
           }
           else if (integral < -TDRIVEINTEGRALLIMIT)
           {
              integral = -TDRIVEINTEGRALLIMIT;
           }

           errorChange = error - prevError;
           prevError = error;

           delay(5);

           error = degrees - currentheading;
           int powerToAssign = min(power, max(-power, error * TDRIVEP + integral * TDRIVEI + errorChange * TDRIVED));
           assignDriveMotorsPower(powerToAssign, -powerToAssign);
        }
        assignDriveMotorsPower(0, 0);
     }

void coast(int ticks, int power, int heading, bool corr){
        ticks *= 200.0 / 257.0;
        power *= 200.0 / 257.0;
        if (left){
           heading = -heading;
        }

        int error = 0;

        clearDriveMotors();
        assignDriveMotorsPower(power, power);
        while (abs(motor_get_position(PORT_DRIVELEFTFRONT)) + abs(motor_get_position(PORT_DRIVELEFTBACK)) + abs(motor_get_position(PORT_DRIVELEFTMIDDLE)) +
                   abs(motor_get_position(PORT_DRIVERIGHTFRONT)) + abs(motor_get_position(PORT_DRIVERIGHTBACK)) + abs(motor_get_position(PORT_DRIVERIGHTMIDDLE))<
               ticks * NUMDRIVEMOTORS)
        {
           error = heading - currentheading;
           if (!corr){
              error = 0;
           }
           assignDriveMotorsPower(power + error * CORRD, power - error * CORRD);
           //assignDriveMotorsPower(30, 30);

           delay(5);
        }
        assignDriveMotorsPower(0, 0);
     }

void forward(int ticks, int power, int heading){
        straightPID(ticks, ticks, power, heading);
     }

void backward(int ticks, int power, int heading){
        straightPID(-ticks, -ticks, power, heading);
     }

void forwardCoast(int ticks, int power, int heading){
        coast(ticks, power, heading, true);
     }

void backwardCoast(int ticks, int power, int heading){
        coast(ticks, -power, heading, true);
     }

void turnLeft(int degrees, int power){
        turnGyro(degrees, power);
     }

void turnRight(int degrees, int power){
        turnGyro(degrees, power);
     }

void turnLeftNOT(int power, int time){
         motor_move(PORT_DRIVELEFTFRONT, power);
         motor_move(PORT_DRIVELEFTMIDDLE, power);
         motor_move(PORT_DRIVERIGHTFRONT, -power);
         motor_move(PORT_DRIVERIGHTMIDDLE, -power);
         motor_move(PORT_DRIVELEFTBACK, power);
         motor_move(PORT_DRIVERIGHTBACK, -power);
         delay(time);
         assignDriveMotorsAuton(0);
     }

void turnRightNOT(int power, int time){
         motor_move(PORT_DRIVELEFTFRONT, -power);
         motor_move(PORT_DRIVELEFTMIDDLE, -power);
         motor_move(PORT_DRIVERIGHTFRONT, power);
         motor_move(PORT_DRIVELEFTBACK, -power);
         motor_move(PORT_DRIVERIGHTMIDDLE, power);
         motor_move(PORT_DRIVERIGHTBACK, power);
         delay(time);
         assignDriveMotorsAuton(0);
     }

void turnRD(int ticks, int power, bool clear){
          assignDriveMotorsDist(-ticks, ticks, power, clear, true);
     }

void turnLD(int ticks, int power, bool clear){
          assignDriveMotorsDist(ticks, -ticks, power, clear, true);
     }
//checks if there is a white line
bool isWhiteLine(int threshold){
       int left = adi_analog_read_calibrated(LINE_TRACKER_LEFT);
       int middle = adi_analog_read_calibrated(LINE_TRACKER_MIDDLE);
       int right = adi_analog_read_calibrated(LINE_TRACKER_RIGHT);
       int avrg = (left+right+middle)/3;
       if (avrg < threshold)
       {
         return true;
       }
       return false;
     }

void autonRollers(int power){
  motor_move(PORT_ROLLERS, -power);
}

void autonFlywheel(int power){
  motor_move(PORT_FLYWHEEL, power);
}

void brake(int power){
  assignDriveMotorsAuton(power);
  delay(150);
  assignDriveMotorsAuton(0);
}

void earlyAuton3Balls(){
  autonRollers(127);
  assignDriveMotorsAuton(50);
  while(!isWhiteLine(1200)) {
    delay(30);
  }
  //brake(-20);
  brake(-40);
  turnRight(1250, 75);
  delay(100);

  forwardCoast(1100, 70, 1250);
  /*
  assignDriveMotorsAuton(60);
  delay(900);
  assignDriveMotorsAuton(0);
  */

  //autonRollers(0);
  autonFlywheel(127);
  delay(700);
  autonFlywheel(0);
  autonRollers(0);
  delay(100);

  autonRollers(127);
  assignDriveMotorsAuton(-50);
  while(!isWhiteLine(1200)) {
    delay(30);
  }
  //brake(-20);
  brake(40);
  turnRight(900, 75);
  delay(100);
  backwardCoast(2500, 70, 900);
  brake(40);
/*
  delay(200);
  autonFlywheel(127);
  delay(140);
  autonFlywheel(0);
  autonRollers(0);
  */
  turnLeft(1800, 75);
  delay(100);
  forwardCoast(250, 70, 1800);
  /*
  assignDriveMotorsAuton(60);
  delay(700);
  assignDriveMotorsAuton(0);
*/
  autonFlywheel(127);
  delay(700);
  autonFlywheel(0);
  delay(100);

//maybe check for gray

  backwardCoast(600, 70, 1800);
  brake(40);
  turnRight(2700, 75);
  delay(100);
  forwardCoast(2700, 70, 2700);
  brake(40);

//changed distance back away at end and turn angle from 2350
  turnLeft(2250, 75);
  autonRollers(127);
  autonFlywheel(127);
  delay(100);
  forwardCoast(1100, 70, 2250);
  /*
  assignDriveMotorsAuton(60);
  delay(900);
  assignDriveMotorsAuton(0);
*/
  //autonFlywheel(127);
  delay(1300);
  autonFlywheel(0);
  autonRollers(0);
  //delay(200);
  backwardCoast(800, 70, 2250);
}

void flipout(){
  autonFlywheel(127);
  delay(150);
  autonFlywheel(0);

}

void indexBall(){
  autonFlywheel(127);
  int timeout = millis();
  while(adi_analog_read_calibrated(LINE_TRACKER_BALL_TOP) > TOPSENSOR && run_flywheel)
  {
    if(flywheelTimeout != 0 && millis() - timeout >= flywheelTimeout)
    {
      break;
    }
    autonFlywheel(127);
    delay(20);
  }
  flywheelTimeout = 0;
  delay(50);
  autonFlywheel(0);
}

void intakeBall(){
  autonRollers(127);
  int timeout = millis();
  while(adi_analog_read_calibrated(LINE_TRACKER_BALL_BOTTOM) > BOTTOMSENSOR && run_intake)
  {
    if(intakeTimeout != 0 && millis() - timeout >= intakeTimeout)
    {
      break;
    }
    autonRollers(127);
    delay(20);
  }
  intakeTimeout = 0;
  delay(75);
  autonRollers(0);
}

void runIntake(){
  while(true)
  {
    if(run_intake)
    {
      intakeBall();
    }
    run_intake = false;
    delay(50);
  }
}

void runFlywheel(){
  while(true)
  {
    if(run_flywheel)
    {
      indexBall();
    }
    run_flywheel = false;
    delay(50);
  }
}

void setFlywheel(int timeout, bool wait){
  run_flywheel = true;
  flywheelTimeout = timeout;
}

void setRollers(int timeout, bool wait){
  run_intake = true;
  intakeTimeout = timeout;
}

void remoteAuton(){
  setFlywheel(0, true);
  //First Goal
  //**********************************************************************************************************
  forwardCoast(1300, 127, 0);
  brake(-20);

  //align with first goal
  turnRight(1275, 127);
  forwardCoast(1100, 127, 1300);
  autonFlywheel(127);
  forwardCoast(300, 127, 1300);
  brake(-20);
  //shoot first goal
  delay(400);
  autonFlywheel(0);

  //Second Goal
  //**********************************************************************************************************
  setFlywheel(2000, true);
  autonRollers(127);

  backwardCoast(1400, 127, 1300);
  brake(-20);
  turnLeft(2700, 127);
  /*forwardCoast(1000, 127, 2900);
  forwardCoast(800, 127, 3100);
  forwardCoast(700, 100, 3300);
  forwardCoast(600, 100, 3600);
  forwardCoast(150, 90, 3600);*/
  forwardCoast(1000, 127, 2900);
  forwardCoast(700, 127, 3100);
  forwardCoast(400, 127, 3300);
  forwardCoast(1200, 95, 3500);
  //forwardCoast(600, 90, 3500);
  delay(100);
  run_intake = true;
  backwardCoast(700, 127, 3600);
  brake(20);
  turnLeft(1800, 110);
  forwardCoast(2100, 127, 1800);
  //brake(-20);
  assignDriveMotorsAuton(20);
  autonFlywheel(127);
  autonRollers(-127);
  delay(1000);
  autonFlywheel(0);
  assignDriveMotorsAuton(0);
  /*turnLeft(900, 70);

  backwardCoast(2100, 110, 900);
  brake(-20);
  autonRollers(0);*/

    //align with second goal
  /*turnRight(1800, 90);
  forwardCoast(500, 100, 1800);

  //brake(-20);
  //shoot second goal
  autonFlywheel(127);
  delay(700);
  autonFlywheel(0);
  */
  //Third Goal
  //**********************************************************************************************************

  backwardCoast(700, 127, 1800);
  brake(-20);
  turnRight(2700, 127);
  autonRollers(127);
  forwardCoast(2500, 127, 2700);
  brake(-20);

  run_intake = false;

    //align with third goal
  turnLeft(2225, 127);
  autonRollers(127);
  forwardCoast(1050, 127, 2225);

  brake(-20);
  //shoot third goal
  autonFlywheel(127);
  delay(2000);

  backwardCoast(1400, 127, 2225);
  autonFlywheel(0);
  autonRollers(0);
}

void remoteAutonBlue(){
  setFlywheel(0, true);
  //First Goal
  //**********************************************************************************************************
  forwardCoast(1300, 127, 0);
  brake(-20);

  //align with first goal
  turnRight(1275, 127);
  forwardCoast(1100, 127, 1300);
  autonFlywheel(127);
  forwardCoast(300, 127, 1300);
  brake(-20);
  //shoot first goal
  delay(400);
  autonFlywheel(0);

  //Second Goal
  //**********************************************************************************************************
  setFlywheel(2000, true);
  autonRollers(127);

  backwardCoast(1400, 127, 1300);
  brake(-20);
  turnLeft(2700, 127);
  /*forwardCoast(1000, 127, 2900);
  forwardCoast(800, 127, 3100);
  forwardCoast(700, 100, 3300);
  forwardCoast(600, 100, 3600);
  forwardCoast(150, 90, 3600);*/
  forwardCoast(1000, 127, 2900);
  forwardCoast(700, 127, 3100);
  forwardCoast(400, 127, 3300);
  forwardCoast(1200, 95, 3500);
  //forwardCoast(600, 90, 3500);
  delay(100);
  run_intake = true;
  backwardCoast(700, 127, 3600);
  brake(20);
  turnLeft(1800, 110);
  forwardCoast(2100, 127, 1800);
  //brake(-20);
  assignDriveMotorsAuton(20);
  autonFlywheel(127);
  autonRollers(-127);
  delay(900);
  autonFlywheel(0);
  assignDriveMotorsAuton(0);

  /*turnLeft(900, 70);

  backwardCoast(2100, 110, 900);
  brake(-20);
  autonRollers(0);*/

    //align with second goal
  /*turnRight(1800, 90);
  forwardCoast(500, 100, 1800);

  //brake(-20);
  //shoot second goal
  autonFlywheel(127);
  delay(700);
  autonFlywheel(0);
  */
  //Third Goal
  //**********************************************************************************************************

  backwardCoast(800, 127, 1800);
  brake(-20);
  turnRight(2700, 127);
  autonRollers(127);
  forwardCoast(2600, 127, 2700);
  brake(-20);

  run_intake = false;

    //align with third goal
  turnLeft(2200, 127);
  autonRollers(127);
  forwardCoast(900, 127, 2200);

  brake(-20);
  //shoot third goal
  autonFlywheel(127);
  delay(1600);

  backwardCoast(1400, 127, 2200);
  autonFlywheel(0);
  autonRollers(0);
}

void sixBallAuton(){
  setFlywheel(0, true);
  //First Goal
  //**********************************************************************************************************
  forwardCoast(1200, 127, 0);
  brake(-20);

  //align with first goal
  turnLeft(-1275, 127);
  forwardCoast(1100, 127, -1300);
  autonFlywheel(127);
  autonRollers(-127);
  forwardCoast(400, 127, -1300);
  //shoot first goal
  delay(400);
  autonFlywheel(0);
  //autonRollers(-127);
  backwardCoast(1700, 127, -1300);
  turnLeft(0, 127);
  autonRollers(127);
  run_flywheel = true;
  forwardCoast(1200, 127, 0);
  forwardCoast(500, 80, 0);
  brake(-20);
  backwardCoast(1300, 127, 0);

  forwardCoast(1100, 127, 900);
  forwardCoast(800, 127, 700);
  autonRollers(0);
  forwardCoast(700, 127, 500);

  forwardCoast(700, 100, 300);
  forwardCoast(100, 90, 100);
  brake(-10);
  //autonFlywheel(127);
  delay(50);
  autonRollers(0);

  backwardCoast(100, 127, 100);
  backwardCoast(100, 127, 500);
  backwardCoast(300, 127, 700);
  backwardCoast(2100, 127, 900);
  autonRollers(127);
  turnLeft(575, 127);
  run_intake = true;
  forwardCoast(925, 80, 575);
  brake(-5);
  delay(600);
  backwardCoast(1300, 127, 575);
  forwardCoast(400, 127, 650);
  forwardCoast(400, 127, 800);
  forwardCoast(400, 127, 900);

  forwardCoast(500, 127, 1100);
  forwardCoast(500, 127, 1300);
  forwardCoast(500, 127, 1500);
  forwardCoast(500, 127, 1700);
  forwardCoast(1000, 127, 1800);
  brake(-5);
  autonFlywheel(127);


  /*turnRight(525, 127);
  forwardCoast(600, 127, 525);
  forwardCoast(150, 127, 700);

  //Second Goal
  //**********************************************************************************************************
  /*setFlywheel(2000, true);
  autonRollers(127);

  backwardCoast(1400, 127, 1300);
  brake(-20);
  turnLeft(2700, 127);
  forwardCoast(1000, 127, 2900);
  forwardCoast(800, 127, 3100);
  forwardCoast(700, 100, 3300);
  forwardCoast(600, 100, 3600);
  //forwardCoast(150, 90, 3600);
  forwardCoast(1000, 127, 2900);
  forwardCoast(700, 127, 3100);
  forwardCoast(400, 127, 3300);
  forwardCoast(1200, 95, 3500);
  //forwardCoast(600, 90, 3500);
  delay(100);
  run_intake = true;
  */
  /*backwardCoast(700, 127, 0);
  brake(20);
  turnLeft(-1600, 110);
  forwardCoast(1700, 127, -1800);
  //brake(-20);
  autonFlywheel(127);
  autonRollers(-127);
  delay(500);
  autonFlywheel(0);
  //Third Goal
  //**********************************************************************************************************

  backwardCoast(700, 127, -1800);
  brake(-20);
  turnRight(-2700, 127);
  autonRollers(127);
  forwardCoast(2100, 127, -2700);
  brake(-20);

  run_intake = false;

    //align with third goal
  turnLeft(-2225, 127);
  forwardCoast(1200, 127, -2225);
  autonRollers(-127);
  delay(500);
  autonRollers(0);*/
}

void fiveBallAutonRed(){
  setFlywheel(0, true);
  //First Goal
  //**********************************************************************************************************
  forwardCoast(1000, 127, 0);
  brake(-20);

  //align with first goal
  turnLeft(-1275, 127);
  forwardCoast(1100, 127, -1275);
  autonFlywheel(127);
  autonRollers(-127);
  forwardCoast(400, 100, -1275);
  //shoot first goal
  delay(400);
  autonFlywheel(0);
  //autonRollers(-127);
  //pick up middle line ball
  backwardCoast(1450, 127, -1275);
  turnLeft(0, 127);
  autonRollers(127);
  run_flywheel = true;
  forwardCoast(1200, 127, 0);
  forwardCoast(925, 80, 0);
  brake(-20);
  backwardCoast(1450, 127, 0);

  //hit ball into middle goal
  forwardCoast(1100, 127, 900);
  forwardCoast(750, 127, 650);
  autonRollers(0);
  forwardCoast(700, 127, 500);

  forwardCoast(700, 100, 300);
  forwardCoast(500, 90, 100);
  brake(-10);
  //autonFlywheel(127);
  delay(50);
  autonRollers(0);


  backwardCoast(1100, 127, 0);
  brake(20);
  turnLeft(-1600, 110);
  forwardCoast(1000, 127, -1650);
  forwardCoast(700, 80, -1700);
  //brake(-20);
  autonFlywheel(127);
  autonRollers(-127);
  delay(500);
  autonFlywheel(0);
  //Third Goal
  //**********************************************************************************************************

  backwardCoast(700, 127, -1800);
  brake(-20);
  turnRight(-2700, 127);
  autonRollers(127);
  forwardCoast(2300, 127, -2700);
  brake(-20);

  run_intake = false;

    //align with third goal
  turnLeft(-2225, 127);
  forwardCoast(1200, 127, -2225);
  autonRollers(-127);
  delay(500);
  autonRollers(0);
}

void fiveBallAutonBlue(){
  setFlywheel(0, true);
  //First Goal
  //**********************************************************************************************************
  forwardCoast(1100, 127, 0);
  brake(-20);

  //align with first goal
  turnLeft(-1275, 127);
  forwardCoast(1100, 127, -1275);
  autonFlywheel(127);
  autonRollers(-127);
  forwardCoast(400, 100, -1275);
  //shoot first goal
  delay(400);
  autonFlywheel(0);
  //autonRollers(-127);
  //pick up middle line ball
  backwardCoast(1545, 127, -1275);
  turnLeft(0, 127);
  autonRollers(127);
  run_flywheel = true;
  forwardCoast(1200, 127, 0);
  forwardCoast(600, 80, 0);
  brake(-20);
  backwardCoast(1300, 127, 0);

  //hit ball into middle goal
  forwardCoast(1100, 127, 900);
  forwardCoast(800, 127, 675);
  autonRollers(0);
  forwardCoast(700, 127, 500);

  forwardCoast(700, 100, 300);
  forwardCoast(200, 90, 100);
  brake(-10);
  //autonFlywheel(127);
  delay(50);
  autonRollers(0);


  backwardCoast(800, 127, 0);
  brake(20);
  turnLeft(-1600, 110);
  forwardCoast(1000, 127, -1700);
  forwardCoast(700, 127, -1700);
  //brake(-20);
  autonFlywheel(127);
  autonRollers(-127);
  delay(500);
  autonFlywheel(0);
  //Third Goal
  //**********************************************************************************************************

  backwardCoast(700, 127, -1800);
  brake(-20);
  turnRight(-2700, 127);
  autonRollers(127);
  forwardCoast(2100, 127, -2700);
  brake(-20);

  run_intake = false;

    //align with third goal
  turnLeft(-2225, 127);
  forwardCoast(1200, 127, -2225);
  autonRollers(-127);
  delay(500);
  autonRollers(0);
}

void progSkills(bool left){
  setFlywheel(0, true);
  //First Goal
  //**********************************************************************************************************
  //pick up first ball
  setRollers(0, true);
              //1250
  forwardCoast(1355, 127, 0);
  delay(50);
  brake(-20);

  //align with first goal
  if(left){
    //-1275
    turnLeft(-1295, 110);
  }
  else{
    turnRight(-1295, 110);
  }
  //forwardCoast(1400, 127, -1295);
  forwardCoast(1100, 127, -1295);
  //delay(200);
  //shoot first goal
  //assignDriveMotorsAuton(40);
  cornerGoalFast(true, CORNERGOALTIMEOUT);
  //assignDriveMotorsAuton(0);

  //Second Goal
  //**********************************************************************************************************
  //spit out one ball
  setFlywheel(0, false);
  backwardCoast(100, 127, -1295);
  autonRollers(-127);
  backwardCoast(200, 127, -1275);
  //autonRollers(0);
  //delay(20);
  autonRollers(-127);
  backwardCoast(600, 127, -1275);

  backwardCoast(600, 127, -1275);
  autonRollers(0);
  delay(200);
  autonRollers(127);

  if(left){
    turnRight(-900, 110);
  }
  else{
    turnLeft(-900, 110);
  }

  forwardCoast(1500, 105, -900);
  setRollers(0, false);
  delay(300);
  run_flywheel = false;

  backwardCoast(400, 127, -900);
  brake(20);

  //pick up other ball
  if(left){
    turnLeft(780, 110);
  }
  else{
    turnRight(780, 110);
  }
  //eject ball
  autonFlywheel(-127);
  forwardCoast(1500, 127, 780);
  autonFlywheel(0);

  autonRollers(127);
  setFlywheel(0, true);
  forwardCoast(1000, 127, 780);
  setFlywheel(0, true);
  forwardCoast(1300, 100, 780);
  setRollers(0, false);
  brake(-30);

  //shoot second goal
  if(left){
    turnRight(1800, 110);
  }
  else{
    turnLeft(1800, 110);
  }
  //setRollers(0, false);
  forwardCoast(1800, 115, 1800);

  middleGoal2and6(true, MIDDLEGOALTIMEOUT);


  //Third Goal
  //**********************************************************************************************************
  //Spit out blue ball
  backwardCoast(450, 127, 1800);
  brake(20);
  autonRollers(127);
  if(left){
    turnLeft(900, 110);
  }
  else{
    turnLeft(900, 110);
  }

  //ejecting ball
  autonFlywheel(-127);

  //pick up corner ball
  forwardCoast(700, 127, 900);
  forwardCoast(1200, 115, 900);
  //stop ejection
  autonFlywheel(0);
  forwardCoast(900, 80, 900);

  brake(-20);
  delay(50);


  //pick up wall ball
  if(left){
    turnLeft(650, 110);
  }
  else{
    turnLeft(650, 110);
  }
  setFlywheel(0, true);

  forwardCoast(500, 100, 650);

  forwardCoast(500, 100, 650);
  delay(175);
  setRollers(0, false);
  backwardCoast(1500, 110, 900);
  brake(20);
  if(left)
  {
    turnLeft(1300, 110);
  }
  else
  {
    turnRight(1300, 110);
  }
  forwardCoast(1350, 110, 1300);
  autonRollers(0);
  cornerGoalFast(true, CORNERGOALTIMEOUT);
  autonRollers(-127);

  //Fourth Goal
  //**********************************************************************************************************
  //go to next ball next to middle goal
  setFlywheel(0, false);
  backwardCoast(100, 127, 1300);
  autonRollers(127);
  //run_intake = true;
  backwardCoast(400, 127, 1300);
  autonRollers(0);
  backwardCoast(700, 127, 1300);
  brake(20);
  autonFlywheel(-127);
  if(left)
  {
    turnLeft(-180, 100);
  }
  else
  {
    turnRight(-180, 100);
  }


  //ejecting
  run_flywheel = false;
  autonFlywheel(-127);
  autonRollers(127);
  forwardCoast(1000, 127, -180);
  autonFlywheel(-127);
  autonRollers(127);
  forwardCoast(1000, 115, -180);
  forwardCoast(1350, 75, -180);
  brake(-20);
  autonFlywheel(0);
  run_flywheel = true;

  //turn to shoot 4th goal
  if(left)
  {
    turnLeft(900, 100);
  }
  else
  {
    turnRight(900, 100);
  }
  setRollers(0, false);
  forwardCoast(600, 100, 900);
  run_flywheel = false;
  forwardCoast(1200, 100, 900);

  delay(350);
  autonRollers(0);
  assignDriveMotorsAuton(20);
  middleGoal2and6(true, MIDDLEGOALTIMEOUT);
  assignDriveMotorsAuton(0);
  delay(20);

  //Fifth Goal
  //**********************************************************************************************************
  autonRollers(-127);
  backwardCoast(500, 115, 900);
  autonRollers(127);
  backwardCoast(700, 115, 900);
  brake(10);

  autonRollers(127);
  //ejecting blue ball
  autonFlywheel(-127);
  if(left)
  {
    turnLeft(0, 100);
  }
  else
  {
    turnRight(0, 100);
  }
  forwardCoast(2300, 127, 0);
  forwardCoast(450, 100, 0);
  delay(200);
  setFlywheel(0, false);
  backwardCoast(1100, 127, 0);
  brake(20);

  //turn towards goal
  if(left)
  {
    turnLeft(475, 100);
  }
  else
  {
    turnRight(475, 100);
  }

  forwardCoast(1800, 110, 475);
  autonRollers(0);
  delay(175);
  run_flywheel = false;
  assignDriveMotorsAuton(10);
  cornerGoalOneRedFast(true);
  assignDriveMotorsAuton(0);
  delay(20);

  //Sixth Goal
  //**********************************************************************************************************
  int headingChange = 1800;

  //spit out one ball
  setFlywheel(0, false);
  autonRollers(-127);
  backwardCoast(200, 127, -1325 + headingChange);
  //autonRollers(0);
  //delay(20);
  autonRollers(-127);
  backwardCoast(1000, 120, -1325 + headingChange);
  delay(20);
  delay(200);
  autonRollers(0);


  if(left){
    turnRight(-900 + headingChange, 110);
  }
  else{
    turnLeft(-900 + headingChange, 110);
  }
  autonRollers(127);
  setRollers(0, false);

  forwardCoast(1700, 100, 900);
  delay(175);
  run_flywheel = false;

  //turn to pick up ball
  backwardCoast(400, 110, 900);
  brake(20);
  //pick up other ball
  if(left){
    turnLeft(750 + headingChange, 110);
  }else{
    turnRight(750 + headingChange, 110);
  }
  //eject ball
  autonFlywheel(-127);
  forwardCoast(1500, 120, 750 + headingChange);
  autonFlywheel(0);

  autonRollers(127);
  setFlywheel(0, true);
  forwardCoast(1000, 120, 750 + headingChange);
  setFlywheel(0, true);
  forwardCoast(1100, 100, 750 + headingChange);
  setRollers(0, false);
  brake(-20);

  //shoot second goal
  if(left){
    turnRight(1800 + headingChange, 110);
  }
  else{
    turnLeft(1800 + headingChange, 110);
  }
  //run_flywheel = true;
  forwardCoast(1800, 110, 1800 + headingChange);
  assignDriveMotorsAuton(10);
  middleGoal2and6(true, MIDDLEGOALTIMEOUT);
  assignDriveMotorsAuton(0);

  //Seventh Goal
  //**********************************************************************************************************
  //Spit out blue ball
  autonRollers(-80);
  backwardCoast(200, 120, 1800 + headingChange);
  autonRollers(127);
  backwardCoast(300, 120, 1800 + headingChange);
  brake(20);
  autonRollers(127);
  if(left){
    turnLeft(900 + headingChange, 110);
  }
  else{
    turnLeft(900 + headingChange, 110);
  }

  //ejecting ball
  autonFlywheel(-127);

  //pick up corner ball
  forwardCoast(700, 127, 900 + headingChange);
  forwardCoast(1200, 127, 900 + headingChange);
  //stop ejection
  autonFlywheel(0);
  forwardCoast(900, 100, 900 + headingChange);

  brake(-20);
  delay(100);

  //pick up wall ball
  if(left){
    turnLeft(600 + headingChange, 110);
  }
  else{
    turnLeft(600 + headingChange, 110);
  }
  setFlywheel(0, true);

  forwardCoast(500, 120, 600 + headingChange);
  setRollers(0, false);
  forwardCoast(500, 120, 600 + headingChange);
  delay(175);
  backwardCoast(1500, 120, 900 + headingChange);
  brake(20);

  if(left)
  {
    turnLeft(1300 + headingChange, 100);
  }
  else
  {
    turnRight(1300 + headingChange, 100);
  }
              //1500
  forwardCoast(1275, 120, 1300 + headingChange);
  cornerGoalFast(true, CORNERGOALTIMEOUT);
  autonRollers(-127);

  //Eighth Goal
  //**********************************************************************************************************
  setFlywheel(0, false);
  backwardCoast(100, 127, 3100);
  autonRollers(127);
  run_intake = true;
  backwardCoast(200, 127, 3100);
  backwardCoast(450, 120, 3100);
  brake(20);
  //autonRollers(0);
  if(left)
  {
    turnLeft(1800, 110);
  }
  else
  {
    turnRight(1800, 110);
  }

  run_flywheel = false;
  autonRollers(127);

  //Ejecting
  autonFlywheel(-127);
  forwardCoast(2810, 120, 1800);
  autonFlywheel(127);
  if(left)
  {
    turnLeft(2700, 110);
  }
  else
  {
    turnRight(2700, 110);
  }
  forwardCoast(500, 110, 2700);
  delay(900);


  //Ninth Goal
  //**********************************************************************************************************
  backwardCoast(500, 120, 2700);
  brake(20);
  autonFlywheel(-127);

  if(left)
  {
    turnRight(920, 100);
  }
  else
  {
    turnLeft(920, 100);
  }

  autonRollers(127);
  forwardCoast(800, 110, 920);
  delay(200);
  forwardCoast(400, 90, 920);
  delay(200);
  autonFlywheel(0);

  forwardCoast(800, 100, 750);
  backwardCoast(600, 70, 750);
  if(left)
  {
    turnRight(885, 100);
  }
  else
  {
    turnLeft(885, 100);
  }
  autonFlywheel(127);

  forwardCoast(300, 60, 885);
  assignDriveMotorsAuton(22);
  delay(600);
  assignDriveMotorsAuton(0);
  autonRollers(0);
  autonRollers(127);
  backwardCoast(500, 80, 825);
  forwardCoast(1000, 100, 700);
  backwardCoast(500, 80, 725);
  autonFlywheel(0);
  forwardCoast(1000, 100, 800);
  autonRollers(0);
  backwardCoast(500, 80, 875);
}

void progSkillsFast(bool left){
  setFlywheel(0, true);
  //First Goal
  //**********************************************************************************************************
  //pick up first ball
  setRollers(0, true);
              //1250
  forwardCoast(1100, 127, 0);

  //forwardCoast(1400, 127, -1295);
  forwardCoast(2500, 127, -1295);
  //delay(200);
  //shoot first goal
  autonFlywheel(127);
  autonRollers(127);
  //assignDriveMotorsAuton(40);
  cornerGoalFast(true, CORNERGOALTIMEOUT);
  //assignDriveMotorsAuton(0);


  //Second Goal
  //**********************************************************************************************************
  //spit out one ball
  setFlywheel(0, false);
  backwardCoast(100, 127, -1295);
  autonRollers(-127);
  backwardCoast(200, 127, -1275);
  //autonRollers(0);
  //delay(20);
  autonRollers(-127);
  backwardCoast(600, 127, -1275);

  backwardCoast(600, 127, -1275);
  autonRollers(0);
  delay(200);
  autonRollers(127);

  if(left){
    turnRight(-900, 127);
  }
  else{
    turnLeft(-900, 127);
  }

  forwardCoast(1500, 105, -900);
  setRollers(0, false);
  delay(300);
  run_flywheel = false;

  backwardCoast(400, 127, -900);
  brake(20);

  //pick up other ball
  if(left){
    turnLeft(780, 127);
  }
  else{
    turnRight(780, 127);
  }
  //eject ball
  autonFlywheel(-127);
  forwardCoast(1500, 127, 780);
  autonFlywheel(0);

  autonRollers(127);
  setFlywheel(0, true);
  forwardCoast(1000, 127, 780);
  setFlywheel(0, true);
  forwardCoast(1100, 120, 780);
  setRollers(0, false);
  brake(-30);

  //shoot second goal
  if(left){
    turnRight(1800, 127);
  }
  else{
    turnLeft(1800, 127);
  }
  //setRollers(0, false);
  forwardCoast(1800, 115, 1800);

  middleGoal2and6(true, MIDDLEGOALTIMEOUT);

  //Third Goal
  //**********************************************************************************************************
  //Spit out blue ball
  backwardCoast(450, 127, 1800);
  brake(20);
  autonRollers(127);
  if(left){
    turnLeft(900, 110);
  }
  else{
    turnLeft(900, 110);
  }

  //ejecting ball
  autonFlywheel(-127);

  //pick up corner ball
  forwardCoast(700, 127, 900);
  forwardCoast(1200, 115, 900);
  //stop ejection
  autonFlywheel(0);
  forwardCoast(900, 80, 900);

  brake(-20);
  delay(50);

  setFlywheel(0, true);

  forwardCoast(500, 100, 650);

  forwardCoast(800, 100, 650);
  delay(50);
  setRollers(0, false);
  backwardCoast(200, 110, 650);
  backwardCoast(200, 110, 800);
  backwardCoast(300, 110, 950);
  backwardCoast(300, 110, 1100);
  backwardCoast(200, 110, 1250);
  backwardCoast(200, 110, 1250);
  brake(20);
  forwardCoast(1350, 110, 1250);
  autonRollers(0);
  cornerGoalFast(true, CORNERGOALTIMEOUT);

  //Fourth Goal
  //**********************************************************************************************************
  //go to next ball next to middle goal
  setFlywheel(0, false);
  backwardCoast(100, 127, 1300);
  autonRollers(127);
  //run_intake = true;
  backwardCoast(400, 127, 1300);
  autonRollers(0);
  backwardCoast(700, 127, 1300);
  brake(20);
  autonFlywheel(-127);
  if(left)
  {
    turnLeft(-180, 127);
  }
  else
  {
    turnRight(-180, 127);
  }


  //ejecting
  run_flywheel = false;
  autonFlywheel(-127);
  autonRollers(127);
  forwardCoast(1000, 127, -180);
  autonFlywheel(-127);
  autonRollers(127);
  forwardCoast(1000, 120, -180);
  forwardCoast(1200, 110, -180);
  brake(-20);


  //turn to shoot 4th goal
  if(left)
  {
    turnLeft(900, 127);
  }
  else
  {
    turnRight(900, 127);
  }
  autonFlywheel(0);
  run_flywheel = true;
  setRollers(0, false);
  forwardCoast(600, 120, 900);
  run_flywheel = false;
  forwardCoast(1200, 120, 900);

  delay(350);
  autonRollers(0);
  assignDriveMotorsAuton(20);
  middleGoal2and6(true, MIDDLEGOALTIMEOUT);
  assignDriveMotorsAuton(0);
  delay(20);

  //Fifth Goal
  //**********************************************************************************************************
  autonRollers(-127);
  backwardCoast(500, 127, 900);
  autonRollers(127);
  backwardCoast(700, 127, 900);
  brake(10);

  autonRollers(127);
  //ejecting blue ball
  autonFlywheel(-127);
  if(left)
  {
    turnLeft(0, 127);
  }
  else
  {
    turnRight(0, 127);
  }
  forwardCoast(2300, 127, 0);
  forwardCoast(450, 120, 0);
  delay(200);
  setFlywheel(0, false);
  backwardCoast(1100, 127, 0);
  brake(20);

  //turn towards goal
  if(left)
  {
    turnLeft(475, 127);
  }
  else
  {
    turnRight(475, 127);
  }

  forwardCoast(1800, 110, 475);
  autonRollers(0);
  delay(175);
  run_flywheel = false;
  assignDriveMotorsAuton(10);
  cornerGoalOneRedFast(true);
  assignDriveMotorsAuton(0);
  delay(20);

  //Sixth Goal
  //**********************************************************************************************************
  int headingChange = 1800;

  //spit out one ball
  setFlywheel(0, false);
  autonRollers(-127);
  backwardCoast(200, 127, -1325 + headingChange);
  //autonRollers(0);
  //delay(20);
  autonRollers(-127);
  backwardCoast(1000, 127, -1325 + headingChange);
  delay(20);
  delay(200);
  autonRollers(0);


  if(left){
    turnRight(-900 + headingChange, 110);
  }
  else{
    turnLeft(-900 + headingChange, 110);
  }
  autonRollers(127);
  setRollers(0, false);

  forwardCoast(1700, 105, 900);
  delay(175);
  run_flywheel = false;

  //turn to pick up ball
  backwardCoast(400, 110, 900);
  brake(20);
  //pick up other ball
  if(left){
    turnLeft(750 + headingChange, 127);
  }else{
    turnRight(750 + headingChange, 127);
  }
  //eject ball
  autonFlywheel(-127);
  forwardCoast(1500, 127, 750 + headingChange);
  autonFlywheel(0);

  autonRollers(127);
  setFlywheel(0, true);
  forwardCoast(1000, 127, 750 + headingChange);
  setFlywheel(0, true);
  forwardCoast(1100, 127, 750 + headingChange);
  setRollers(0, false);
  brake(-20);

  //shoot second goal
  if(left){
    turnRight(1800 + headingChange, 127);
  }
  else{
    turnLeft(1800 + headingChange, 127);
  }
  //run_flywheel = true;
  forwardCoast(1800, 127, 1800 + headingChange);
  assignDriveMotorsAuton(10);
  middleGoal2and6(true, MIDDLEGOALTIMEOUT);
  assignDriveMotorsAuton(0);
}

void autonomous() {
  imu_sensor = 1;
  bool left = true;

  autonNumber = 2;
  headingTask = task_create(getHeading, "PROS", TASK_PRIORITY_DEFAULT, TASK_STACK_DEPTH_DEFAULT, "Heading Task");
  intakeIndexTask = task_create(runIntake, "PROS", TASK_PRIORITY_DEFAULT, TASK_STACK_DEPTH_DEFAULT, "Intake Task");
  flywheelIndexTask = task_create(runFlywheel, "PROS", TASK_PRIORITY_DEFAULT, TASK_STACK_DEPTH_DEFAULT, "Flywheel Task");
  switch (autonNumber)
   {
   case 1:
      progSkillsFast(left);
      break;
   case 2:
      fiveBallAutonRed();
      break;
   case 3:
      fiveBallAutonBlue();
      break;
   case 4:
      break;
   case 5:
      break;
   case 6:
      break;
   default:
      break;
   }
}
