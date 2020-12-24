#include "main.h"
#include "config.h"
#include <stdio.h>
#include <inttypes.h>
extern void initializeDriveMotors();
 #define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

 #define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

extern adi_gyro_t gyro;

/*void assignDriveMotors(int leftSide, int rightSide){
    motor_move(PORT_DRIVELEFTFRONT, leftSide);
    motor_move(PORT_DRIVELEFTBACK, leftSide);
    motor_move(PORT_DRIVERIGHTFRONT, rightSide);
    motor_move(PORT_DRIVERIGHTBACK, rightSide);
}*/

void drive(void* param){
    int i = 100;
    while (true) {
        int forward = -controller_get_analog(CONTROLLER_MASTER, ANALOG_RIGHT_X);
        int turn = controller_get_analog(CONTROLLER_MASTER, ANALOG_LEFT_Y);

        motor_move(PORT_DRIVELEFTFRONT, max(-127, min(127, forward + turn)));
        motor_move(PORT_DRIVELEFTMIDDLE, max(-127, min(127, forward + turn)));
        motor_move(PORT_DRIVELEFTBACK, max(-127, min(127, forward + turn)));
        motor_move(PORT_DRIVERIGHTFRONT, -max(-127, min(127, forward - turn)));
        motor_move(PORT_DRIVERIGHTMIDDLE, -max(-127, min(127, forward - turn)));
        motor_move(PORT_DRIVERIGHTBACK, -max(-127, min(127, forward - turn)));
        delay(20);
    }
}
void shooting(void* param){
  while(true)
  {
    if(controller_get_digital(CONTROLLER_MASTER, DIGITAL_LEFT) || controller_get_digital(CONTROLLER_PARTNER, DIGITAL_LEFT)){
      int ballsintake = 0;
      int ballsshoot = 0;
      motor_move(PORT_FLYWHEEL, 127);
      motor_move(PORT_ROLLERS, -127);
      while(adi_analog_read_calibrated(LINE_TRACKER_BALL_TOP) > 2850)
      {
        if(adi_analog_read_calibrated(LINE_TRACKER_BALL_BOTTOM) < 2850)
        {
          ballsintake++;
          motor_move(PORT_ROLLERS, 0);
        }
        if(ballsintake >= 1)
        {
          motor_move(PORT_ROLLERS, 0);
        }
        delay(20);
      }
      ballsshoot++;
      delay(500);
      while(adi_analog_read_calibrated(LINE_TRACKER_BALL_TOP) > 2850)
      {
        if(adi_analog_read_calibrated(LINE_TRACKER_BALL_BOTTOM) < 2850)
        {
          ballsintake++;
          motor_move(PORT_ROLLERS, 0);
        }
        if(ballsintake >= 1)
        {
          motor_move(PORT_ROLLERS, 0);
        }
        delay(20);
      }
      while(adi_analog_read_calibrated(LINE_TRACKER_BALL_BOTTOM) > 2850 && ballsintake < 1)
      {
        delay(20);
      }
      motor_move(PORT_ROLLERS, 0);
      delay(400);
      motor_move(PORT_FLYWHEEL, 0);
      ballsshoot++;
    }
    if(controller_get_digital(CONTROLLER_MASTER, DIGITAL_UP) || controller_get_digital(CONTROLLER_PARTNER, DIGITAL_UP)){
      int ballsintake = 0;
      int ballsshoot = 0;
      motor_move(PORT_FLYWHEEL, 127);
      motor_move(PORT_ROLLERS, -127);
      while(adi_analog_read_calibrated(LINE_TRACKER_BALL_TOP) > 2850)
      {
        if(adi_analog_read_calibrated(LINE_TRACKER_BALL_BOTTOM) < 2850)
        {
          ballsintake++;
          motor_move(PORT_ROLLERS, 0);
        }
        if(ballsintake >= 2)
        {
          motor_move(PORT_ROLLERS, 0);
        }
        delay(20);
      }
      delay(300);
      ballsshoot = 1;
      while(adi_analog_read_calibrated(LINE_TRACKER_BALL_TOP) > 2850)
      {
        delay(20);
      }
      ballsshoot = 2;
      motor_move(PORT_ROLLERS, -127);
      while(adi_analog_read_calibrated(LINE_TRACKER_BALL_BOTTOM) > 2850 && ballsintake < 2)
      {
        delay(20);
      }
      ballsintake++;
      delay(200);
      while(adi_analog_read_calibrated(LINE_TRACKER_BALL_BOTTOM) > 2850 && ballsintake < 2)
      {
        delay(20);
      }
      ballsintake++;
      motor_move(PORT_ROLLERS, 0);
      motor_move(PORT_FLYWHEEL, 0);
    }
  }
}
void rollers(void* param){
    int intakeDirection = 0;
    int ballCount = 0;
    while (true) {

        if(controller_get_digital(CONTROLLER_MASTER, DIGITAL_R1) || controller_get_digital(CONTROLLER_PARTNER, DIGITAL_R1)){
          if(intakeDirection == 0 || intakeDirection == -1){
            motor_move(PORT_ROLLERS, -127);
            intakeDirection = 1;
          }
          else{
            motor_move(PORT_ROLLERS, 0);
            intakeDirection = 0;
          }
          delay(200);
        }

        if(controller_get_digital(CONTROLLER_MASTER, DIGITAL_R2) || controller_get_digital(CONTROLLER_PARTNER, DIGITAL_R2)){
          if(intakeDirection == 0 || intakeDirection == 1){
            motor_move(PORT_ROLLERS, 127);
            intakeDirection = -1;
          }
          else{
            motor_move(PORT_ROLLERS, 0);
            intakeDirection = 0;
          }
          delay(200);
        }
        delay(20);
    }
}
void flywheel(void* param){
    int flywheelDirection = 0;
    while(true)
    {
      if(controller_get_digital(CONTROLLER_MASTER, DIGITAL_A) || controller_get_digital(CONTROLLER_PARTNER, DIGITAL_A)){
        while(adi_analog_read_calibrated(LINE_TRACKER_BALL_TOP) > 2850)
        {
          flywheelDirection = 1;
          motor_move(PORT_FLYWHEEL, 127);
          delay(20);
        }
        delay(150);
        motor_move(PORT_FLYWHEEL, 0);
        flywheelDirection = 0;
      }
      if(controller_get_digital(CONTROLLER_MASTER, DIGITAL_B) || controller_get_digital(CONTROLLER_PARTNER, DIGITAL_B)){
        motor_move(PORT_FLYWHEEL, 127);
        flywheelDirection = 1;
        delay(210);
        motor_move(PORT_FLYWHEEL, 0);
        flywheelDirection = 0;
      }
      if(controller_get_digital(CONTROLLER_MASTER, DIGITAL_L1) || controller_get_digital(CONTROLLER_PARTNER, DIGITAL_L1)){
        if(flywheelDirection == 0 || flywheelDirection == -1){
          motor_move(PORT_FLYWHEEL, 127);
          motor_move(PORT_TOPBACK, 127);
          flywheelDirection = 1;
        }
        else{
          motor_move(PORT_FLYWHEEL, 0);
          motor_move(PORT_TOPBACK, 0);
          flywheelDirection = 0;
        }
        delay(200);
      }
      if(controller_get_digital(CONTROLLER_MASTER, DIGITAL_L2) || controller_get_digital(CONTROLLER_PARTNER, DIGITAL_L2)){
        if(flywheelDirection == 0 || flywheelDirection == 1){
          motor_move(PORT_FLYWHEEL, -127);
          motor_move(PORT_TOPBACK, -127);
          flywheelDirection = -1;
        }
        else{
          motor_move(PORT_FLYWHEEL, 0);
          motor_move(PORT_TOPBACK, 0);
          flywheelDirection = 0;
        }
        delay(200);
      }
      delay(20);
    }
}
void displayInfo(void *param)
{
   lcd_initialize();
   while (true)
   {
      char tempString1[100];
      char tempString2[100];
      char tempString3[100];
      char tempString4[100];
      char tempString5[100];
      char tempString6[100];

      sprintf(tempString1, "Gyro Value: %d", (int)adi_gyro_get(gyro));
      sprintf(tempString2, "IMU Value: %d", (int)imu_get_heading(IMU_PORT)*10);
      sprintf(tempString3, "Battery Voltage: %d", battery_get_voltage());
      sprintf(tempString4, "Line Sensor Value Top: %d", adi_analog_read_calibrated(LINE_TRACKER_BALL_TOP));
      sprintf(tempString5, "Line Sensor Value Bottom: %d", adi_analog_read_calibrated(LINE_TRACKER_BALL_BOTTOM));

      lcd_set_text(1, tempString1);
      lcd_set_text(2, tempString2);
      lcd_set_text(3, tempString3);
      lcd_set_text(4, tempString4);
      lcd_set_text(5, tempString5);
      lcd_set_text(6, tempString6);

      printf("here\n");

      //controller_print(CONTROLLER_MASTER, 0, 0, "RPM: %.2f", motor_get_actual_velocity(PORT_FLYWHEEL));

      delay(10);
   }
}
void opcontrol() {
  //  initializeDriveMotors();
    task_t driveTask = task_create(drive, "PROS", TASK_PRIORITY_DEFAULT, TASK_STACK_DEPTH_DEFAULT, "Drive Task");
    task_t rolTask = task_create(rollers, "PROS", TASK_PRIORITY_DEFAULT, TASK_STACK_DEPTH_DEFAULT, "Rol Task");
    task_t flyTask = task_create(flywheel, "PROS", TASK_PRIORITY_DEFAULT, TASK_STACK_DEPTH_DEFAULT, "Fly Task");
    task_t shootTask = task_create(shooting, "PROS", TASK_PRIORITY_DEFAULT, TASK_STACK_DEPTH_DEFAULT, "Shooting Task");
    task_t displayInfoTask = task_create(displayInfo, "PROS", TASK_PRIORITY_DEFAULT, TASK_STACK_DEPTH_DEFAULT, "Display Info Task");
}