#include <SPI.h>
#include <WiFiNINA.h>
#include <string>
#include <ArduinoLowPower.h>

#include "arduino_secrets.h"
// DEBUG LED :
// ON 4s ; OFF 4s                      : No connection with the Host
// ON 1s ; OFF 4s                      : Waiting for the Next Request
// ON 0.1s ; OFF 0.1s                  : Waiting for the Response

char ssid[] = SECRET_SSID; // your network SSID (name)
char pass[] = SECRET_PASS; // your network password

WiFiClient client;

const int    HTTP_PORT   = 5000;
const String HTTP_METHOD = "GET";
const char   HOST_NAME[] = "192.168.1.161";//"monitoringapi.solaredge.com";
const String PATH_NAME   = "/";//"/v2/sites/{id}/power-flow";
const String X_Account_Key = "X-ACCOUNT-KEY: {key}";
const String X_API_Key = "X-API-KEY: {key}";
const String Accept = "Accept: application/json, application/problem+json";

#define ComparedPowerBufferLength 4
const char ComparedPowerBuffer[] = {'"','p','v','"'};

const int SwitchPin = 6;
const int PowerLimit = 3000; 
const int DelayBetweenCheck = 240000;
const int NumberOfRequestBetweenTimeCheck = 60;
const int TimeToSleep = 21; // After 21h it will sleep for "NightSleepDuration"
const int NightSleepDuration = 32400000; // 9h in millisecondes

int NumberOfRequestSinceLastTimeCheck = 0;

void WifiSetup(){
  int status = WL_IDLE_STATUS;

  while ( status != WL_CONNECTED) {
    Serial.print("Attempting to connect to WEP network, SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    for (char i = 0; i < 6; i++){
    digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
    delay(1000);                      
    digitalWrite(LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW
    delay(500);
    }
  }

  // once you are connected :
  Serial.print("You're connected to the network");
  Serial.println(ssid);
  
}

bool CompareBuffers(const char* pComparedBuffer, char* pParsBuffer, unsigned char Offset, unsigned char BufferLenght) {
    if (pComparedBuffer && pParsBuffer) {
        char DeltaPos;
        for (char i = 0; i < BufferLenght; i++) {
            DeltaPos = i + Offset;
            if (DeltaPos >= BufferLenght) {
              DeltaPos -= BufferLenght;
            }
            if (*(pComparedBuffer + i) != *(pParsBuffer + DeltaPos)) {
                return false;
            }
        }
        return true;
    }
    return false;
}

int GetTime(){
  return (WiFi.getTime()/ 3600) % 24 + 2;
}

int GetInverterPower(){
  // connect to web server on port 80:
  int CurrentPower = 0;
  if(client.connect(HOST_NAME, HTTP_PORT)) {
    // if connected: // Debug INFO
    Serial.println("Connected to server");
    // make a HTTP request:
    // send HTTP header
    client.println(HTTP_METHOD + " " + PATH_NAME + " HTTP/1.1");
    //client.println(X_Account_Key);
    //client.println(X_API_Key);
    client.println(Accept);
    client.println("Host: " + String(HOST_NAME));
    client.println("Connection: close");
    client.println(); // end HTTP header

    char DelayCounter = 0;
    while (!client.available()){
      digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
      delay(10);
      digitalWrite(LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW
      delay(10); 
      DelayCounter++;
      if (DelayCounter > 50){
        // Debug INFO
        Serial.println("Wait for too long, pass");
        return 0;
      }
    }

    //SKIP the response header
    int HeaderSize = client.available();
    for(unsigned int i = 0; i < HeaderSize; i++){
      client.read();
    }

    delay(200);
    //Fetch the data
    int DataSize = client.available();

    if(DataSize){
      char ParsBuffer[ComparedPowerBufferLength];
      unsigned char ParsBufferPos = 0;
      for (int i = 0 ; i < ComparedPowerBufferLength ; i++ ){
        ParsBuffer[i] = client.read();
      }
      DataSize -= ComparedPowerBufferLength;
      while (!CompareBuffers(ComparedPowerBuffer, ParsBuffer, ParsBufferPos, ComparedPowerBufferLength)){
        ParsBuffer[ParsBufferPos] = client.read();
        ParsBufferPos++;
        DataSize--;
        if(ParsBufferPos >= ComparedPowerBufferLength){
          ParsBufferPos = 0;
          }
        if(DataSize <= 0){
          
          Serial.println();
          Serial.println("Current Power Output: 0W");
          Serial.println();
          Serial.println("disconnected");

          client.stop();
          return 0;
        }
      }

      //SKIP THE " : {"
      for (char i = 0 ; i < 3 ; i++){
        client.read();
      }

      char Content[72];
      Content[0] = client.read();
      unsigned char LastReadPos = 0;
      while((Content[LastReadPos] != '}') && (LastReadPos < 72)){
        Content[LastReadPos + 1] = client.read();
        LastReadPos++;
      }
      Content[LastReadPos] = '\0';
      String ContentString = Content;
      char ValuePos = ContentString.indexOf('power') + 19;
      String PowerString = ContentString.substring(ValuePos, ValuePos + 12);
      PowerString.trim();
      Serial.println(PowerString);
      CurrentPower = PowerString.toInt();
    }
    
    
    // the server's disconnected, stop the client:
    client.stop();

    // Debug INFO
    Serial.println();
    Serial.println("Current Power Output: " + String(CurrentPower) + "W");
    Serial.println();
    Serial.println("disconnected");

  } else {// if not connected:
    // Debug INFO
    Serial.println("connection failed");
    for (int i = 0; i < 5 ; i++){
    digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
    LowPower.sleep(4000);                      // wait for a second
    digitalWrite(LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW
    LowPower.sleep(4000); 
  }
  }
  return CurrentPower;
}

void setup() {
  pinMode(SwitchPin ,OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(9600);
  //Waiting For The Serial Connection
  while (!Serial);
  Serial.println("Connected to Serial");
  // initialize the Wifi connection:
  WifiSetup();
}

void loop() {
  //LED Request Always ON
  digitalWrite(LED_BUILTIN, HIGH);

  //PowerCheck 
  Serial.println("Power Check");
  if (GetInverterPower() > PowerLimit){
    digitalWrite(SwitchPin, HIGH);
    Serial.println("HIGH");
  } else {
    digitalWrite(SwitchPin, LOW);
    Serial.println("LOW");
  }


  //TimeCheck
  if(NumberOfRequestSinceLastTimeCheck >= NumberOfRequestBetweenTimeCheck){
    Serial.println("Time Check");
    if(GetTime() >= TimeToSleep){
      Serial.println("Time to Sleep");
      digitalWrite(SwitchPin, LOW);
      digitalWrite(LED_BUILTIN, LOW);
      LowPower.deepSleep(NightSleepDuration);

    }
    NumberOfRequestSinceLastTimeCheck = 0;
  } else {
    NumberOfRequestSinceLastTimeCheck++;

    //LED Waiting Pattern
    for (int i = 0; i <= DelayBetweenCheck ; i  += 5000){
      digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
      LowPower.sleep(1000);             // wait for a second
      digitalWrite(LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW
      LowPower.sleep(4000); 
    }

  }

}