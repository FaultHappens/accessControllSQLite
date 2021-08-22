#include <virtuabotixRTC.h>

int seconds = 00;
int minutes = 0;
int hour = 0;
int dayOfWeek = 1;
int day = 1;
int month = 7;
int year = 2021;

virtuabotixRTC myRTC(25, 26, 27);

void setup() {
  Serial.begin(115200);
  myRTC.setDS1302Time(seconds, minutes, hour, dayOfWeek, day, month, year);
}

void loop() {
  // put your main code here, to run repeatedly:

}
