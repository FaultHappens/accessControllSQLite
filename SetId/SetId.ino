#include <EEPROM.h>

String ID = "11111";   // აქ აიდი ჩაწერე 

void setup() {
  Serial.begin(115200);
  Serial.println("Writing ID " + ID + " to this devices EEPROM");
  EEPROM.begin(5);
  for(int i = 0; i < ID.length(); i++){
    Serial.println("Writing " + String(ID[i]) + " to " + i);
    EEPROM.write(i, String(ID[i]).toInt());
    
  }
  EEPROM.commit();
  Serial.println("Done");
  Serial.println("Now Reading");

  int a = EEPROM.read(0);
  Serial.println(a);

  for (int i = 0; i < 5; i++) {
    Serial.print(EEPROM.read(i));
  }
}

void loop() {
  // put your main code here, to run repeatedly:

}
