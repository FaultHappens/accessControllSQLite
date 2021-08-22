//RC522

#include <virtuabotixRTC.h>
#include <EEPROM.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <WiFi.h>
#include "time.h"
#include <MFRC522.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>

SPIClass spiSD(HSPI);
#define SD_CS 12

#define DOOR_PIN 26
#define BIZZER_PIN 27

WebServer server(80);
HTTPClient http;

constexpr uint8_t RST_PIN1 = 2;
constexpr uint8_t SS_PIN1 = 4;
constexpr uint8_t RST_PIN2 = 21;
constexpr uint8_t SS_PIN2 = 5;
MFRC522 rfid1(SS_PIN1, RST_PIN1);
MFRC522 rfid2(SS_PIN2, RST_PIN2);
MFRC522::MIFARE_Key key1;
MFRC522::MIFARE_Key key2;

virtuabotixRTC myRTC(25, 26, 27);

char ssid[100];
char password[100];

String apiKey = "";                                               //აქეთ აპის გასაღები
//String sendRequestTo = "http://1494lp.com/api/service/inout.php";

String sendRequestTo = "http://192.168.88.138:8090/";

byte requestsInQueue = 0;
String request[100] = "";

char* ssidR1;
char* passwordR1;

int userCount = 0;
String tag;
String mID;

String Users[2000];

TaskHandle_t Task1;

sqlite3 *db1;
char *zErrMsg = 0;
int rc;



void setup() {
  xTaskCreatePinnedToCore(Task1Code, "Task1", 10000, NULL, 0, &Task1, 0);
  Serial.begin(115200);
  spiSD.begin(14, 32, 13, 15); //SCK,MISO,MOSI,SS //HSPI1

  while (!SD.begin( SD_CS, spiSD ))
  {
    Serial.println("Card Mount Failed");
  }
  sqlite3_initialize();
  getWifi(SD);
  getMyId();
  Serial.println("Time is " + getTime());

  Serial.print("Connecting to ");
  Serial.print(ssidR1);
  Serial.print(" with password ");
  Serial.println(passwordR1);
  WiFi.begin(ssidR1, passwordR1);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  server.on("/add", handleAddRequest);
  server.on("/delete", handleDeleteRequest);
  server.begin();
  Serial.println("HTTP server started");
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());
  Serial.println(getTime());


  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }
  getUsersFromSD(SD);
  SPI.begin();
  rfid1.PCD_Init();
  rfid2.PCD_Init();
  pinMode(DOOR_PIN, OUTPUT);
  Serial.println("Loading complete!");

}


void loop() {
  while (true) {
    server.handleClient();
  }
  server.handleClient();
  if (!rfid1.PICC_IsNewCardPresent() && !rfid2.PICC_IsNewCardPresent()) {
    return;
  }
  if (rfid1.PICC_ReadCardSerial()) {
    for (byte i = 0; i < 4; i++) {
      tag += rfid1.uid.uidByte[i];
    }

    Serial.print(tag);
    Serial.println(" IN");
    if (isAuthorized(tag) != -1) {

      Serial.println("Welcome, " + tag);
      openDoor();
    } else {
      Serial.println("You`r not one of us, go away!");
    }
    makeLog(SD, tag, "IN");
    tag = "";
    rfid1.PICC_HaltA();
    rfid1.PCD_StopCrypto1();
  }
  else if (rfid2.PICC_ReadCardSerial()) {
    for (byte i = 0; i < 4; i++) {
      tag += rfid2.uid.uidByte[i];
    }
    Serial.print(tag);
    Serial.println(" OUT");
    if (isAuthorized(tag) != -1) {

      Serial.println("Welcome, " + tag);
      openDoor();
    } else {
      Serial.println("You`r not one of us, go away!");
    }
    makeLog(SD, tag, "OUT");
    tag = "";
    rfid2.PICC_HaltA();
    rfid2.PCD_StopCrypto1();
  }
}
void makeLog(fs::FS & fs, String user, String a) {
  if (openDb("/sd/accessControll.db", &db1))
    return;
  String strSql = "INSERT INTO logs VALUES('" + user + "', '" + getTime() + "', '" + a + "')";
  const char *sql = strSql.c_str();
  rc = db_exec(db1, sql);
  if (rc != SQLITE_OK) {
    sqlite3_close(db1);
    return;
  }

  sqlite3_close(db1);

  String httpRequest = "api_key=";
  httpRequest += apiKey;
  httpRequest += "&mID=";
  httpRequest += mID;
  httpRequest += "&userID=";
  httpRequest += user;
  httpRequest += "&DATETIME=";
  httpRequest += getTime();
  httpRequest += "&IN/OUT=";
  httpRequest += a;

  Serial.println("Request was added to queue with index " + String(requestsInQueue));
  requestsInQueue++;
  request[requestsInQueue - 1] = httpRequest;


}




void Task1Code(void * parameter) {
  for (;;) {
    delay(100);
    while (requestsInQueue != 0) {
      http.begin(sendRequestTo);
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");
      Serial.println("-------------------");
      Serial.println("Trying to send request from queue # " +  String(requestsInQueue - 1));
      int responseCode = http.POST(request[requestsInQueue - 1]);
      Serial.print("Response code is:");
      Serial.println(responseCode);
      if (responseCode > 0) {
        String response = http.getString();
        Serial.print("Response is: ");
        Serial.println(response);
        if (response == 0) {
          http.end();
          Serial.println("Error, i will try again in 5 minutes");
          delay(300000);
          Serial.println("-------------------");
          break;
        } else {
          http.end();
          Serial.println("Succesfully sent");
          request[requestsInQueue - 1] = "";
          requestsInQueue--;
          Serial.println("-------------------");
        }
      }
      else {
        http.end();
        Serial.println("Error, i will try again in 5 minutes");
        delay(300000);
        Serial.println("-------------------");
        break;
      }
    }
  }
}

int isAuthorized(String user) {
  for (int i = 0; i < userCount; i++) {
    if (Users[i] == user) {
      return i;
    }
  }
  return -1;
}


void getWifi(fs::FS & fs) {
  char i;
  File file = fs.open("/wifi.txt");
  if (!file) {
    Serial.println("Failed to open wifi.txt for reading");
    return;
  }

  int a = 0;
  i = file.read();
  while (i != ';') {
    ssid[a] = i;
    a++;
    i = file.read();

  }
  i = file.read();
  a = 0;
  while (i != ';') {

    password[a] = i;
    a++;
    i = file.read();

  }
  file.close();
  ssidR1 = &ssid[0];
  passwordR1 = &password[0];

}

void openDoor() {
  digitalWrite(DOOR_PIN, LOW);
  delay(3000);
  digitalWrite(DOOR_PIN, HIGH);
}

void getMyId() {
  EEPROM.begin(5);
  Serial.println("Getting my id from EEPROM");
  for (int i = 0; i < 5; i++) {
    mID += EEPROM.read(i);
  }
  Serial.println("Done! My id is " + mID);

}

void addUser(fs::FS & fs, String userUID) {
  if (isAuthorized(userUID) != -1) {
    Serial.println("User " + userUID + " already exists!");
    return;
  }
  userCount++;
  Serial.println("Adding user " + userUID + " to SD and RAM");
  Users[userCount - 1] = userUID;

  if (openDb("/sd/accessControll.db", &db1)) {
    Serial.println("Error opening DB");
    return;
  }
  String strSql = "INSERT INTO users VALUES('" + userUID + "')";
  const char *sql = strSql.c_str();
  rc = db_exec(db1, sql);
  if (rc != SQLITE_OK) {
    Serial.println("Error occured while adding!");
    sqlite3_close(db1);
    return;
  }
  sqlite3_close(db1);
  Serial.println("Succesfully added!");
}

void deleteUser(fs::FS & fs, String userUID) {
  if (isAuthorized(userUID) == -1) {
    Serial.println("User " + userUID + " is not authorized!");
    return;
  }
  userCount = 0;
  Serial.println("Deleting user " + userUID);
  if (openDb("/sd/accessControll.db", &db1)) {
    Serial.println("Error opening DB");
    return;
  }
  String strSql = "DELETE FROM users WHERE UID = '" + userUID + "'";
  const char *sql = strSql.c_str();
  rc = db_exec(db1, sql);
  if (rc != SQLITE_OK) {
    Serial.println("Error occured while deleting!");
    sqlite3_close(db1);
    return;
  }
  sqlite3_close(db1);
  getUsersFromSD(SD);
  Serial.println("Succesfully deleted!");
}

void getUsersFromSD(fs::FS & fs) {
  Serial.println("Getting users from SD and writing to RAM");
  if (openDb("/sd/accessControll.db", &db1)) {
    return;
  }
  const char* data = "Callback function called";
  const char* sql = "SELECT * FROM users";
  Serial.println(sql);
  long start = micros();
  int rc = sqlite3_exec(db1, sql, loadUsersCallback, (void*)data, &zErrMsg);
  if (rc != SQLITE_OK) {
    Serial.printf("SQL error: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
  } else {
    Serial.printf("Operation done successfully\n");
  }
  Serial.print(F("Time taken:"));
  Serial.println(micros() - start);
  if (rc != SQLITE_OK) {
    Serial.println("Error!");
    sqlite3_close(db1);
    return;
  }

  sqlite3_close(db1);
  Serial.println("Done");
}
String getTime() {
  myRTC.updateTime();
  String timeString;
  timeString += myRTC.dayofmonth;
  timeString += "/";
  timeString += myRTC.month;
  timeString += "/";
  timeString += myRTC.year;
  timeString += " ";
  timeString += myRTC.hours;
  timeString += ":";
  timeString += myRTC.minutes;
  return timeString;
}

void handleAddRequest() {
  Serial.println(server.arg("cardUID"));
  addUser(SD, server.arg("cardUID"));
  server.send(303);
}


void handleDeleteRequest() {
  Serial.println(server.arg("cardUID"));
  deleteUser(SD, server.arg("cardUID"));
  server.send(303);
}




const char* data = "Callback function called";
static int loadUsersCallback(void *data, int argc, char **argv, char **azColName) {
  int i;
  Serial.printf("%s: ", (const char*)data);
  for (i = 0; i < argc; i++) {
    userCount++;
    Users[userCount - 1] += argv[i];
    Serial.printf("Loaded user %s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
  }
  Serial.printf("\n");
  return 0;
}


static int callback(void *data, int argc, char **argv, char **azColName) {
}


int openDb(const char *filename, sqlite3 **db) {
  int rc = sqlite3_open(filename, db);
  if (rc) {
    Serial.printf("Can't open database: % s\n", sqlite3_errmsg(*db));
    return rc;
  } else {
    Serial.printf("Opened database successfully\n");
  }
  return rc;
}
int db_exec(sqlite3 * db, const char *sql) {
  Serial.println(sql);
  long start = micros();
  int rc = sqlite3_exec(db, sql, callback, (void*)data, &zErrMsg);
  if (rc != SQLITE_OK) {
    Serial.printf("SQL error: % s\n", zErrMsg);
    sqlite3_free(zErrMsg);
  } else {
    Serial.printf("Operation done successfully\n");
  }
  Serial.print(F("Time taken: "));
  Serial.println(micros() - start);
  return rc;
}
