#include <SPI.h>
#include <Ethernet.h>
#include <string>
#include <ArduinoLowPower.h>

// DEBUG LED :
// ON 4s ; OFF 0.5s ; ON 1s ; OFF 0.5s : ETH Cable ERROR
// ON 1s ; OFF 0.5s ; ON 1s ; OFF 0.5s : DHCP Error
// ON 4s ; OFF 4s                      : No connection with the Host
// ON 1s ; OFF 4s                      : Waiting for the Next Request
// ON 0.1s ; OFF 0.1s                  : Waiting for the Response

// replace the MAC address below by the MAC address printed on a sticker on the Arduino Shield 2
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

EthernetClient client;

const int    HTTP_PORT   = 80;
const String HTTP_METHOD = "GET";
const char   HOST_NAME[] = "https://monitoringapi.solaredge.com";
const String PATH_NAME   = "/v2/sites/{id}/power-flow";
const String X_Account_Key = "X-ACCOUNT-KEY: {key}";
const String X_API_Key = "X-API-KEY: {key}";
const String Accept = "Accept: application/json, application/problem+json";


const char ComparedPowerBuffer[4] = {'"','p','v','"'};
const char ComparedTimeBuffer[11] = {'"','u','p','d','a','t','e','d','A','t','"'};

const int SwitchPin = 6;
const int PowerLimit = 3000; 
const int DelayBetweenCheck = 240000;
const int NumberOfRequestBetweenTimeCheck = 60;
const int TimeToSleep = 21; // After 21h it will sleep for "NightSleepDuration"
const int NightSleepDuration = 32400000; // 9h in millisecondes

int NumberOfRequestSinceLastTimeCheck = 0;

void EthernetSetup(){
  if (Ethernet.linkStatus() != LinkON){
    // Debug INFO
    //Serial.println("Link status: Off");
    while(Ethernet.linkStatus() != LinkON){
      //LED ETH ERROR PATTERN
      digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
      LowPower.sleep(4000);                      
      digitalWrite(LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW
      delay(500); 
      digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
      LowPower.sleep(1000);  
      digitalWrite(LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW
      delay(500); 
    }
  } else {
    // Debug INFO
    //Serial.println("Link status: On");

  }
  int ETHCode = Ethernet.begin(mac);
  if (ETHCode == 0) {
    // Debug INFO
    //Serial.println("Failed to obtain an IP address using DHCP");
    delay(10000);
    while(ETHCode == 0){
      // LED DHCP ERROR PATTERN
      digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
      LowPower.sleep(1000);                      
      digitalWrite(LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW
      delay(500); 
      digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
      LowPower.sleep(1000);  
      digitalWrite(LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW
      delay(500); 
      ETHCode = Ethernet.begin(mac);
    }
  }
  
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

int GetInverterTime(){

  int CurrentTime = 0;
  if(client.connect(HOST_NAME, HTTP_PORT)) {
    // if connected: // Debug INFO
    //Serial.println("Connected to server");
    // make a HTTP request:
    // send HTTP header
    client.println(HTTP_METHOD + " " + PATH_NAME + " HTTP/1.1");
    client.println(X_Account_Key);
    client.println(X_API_Key);
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
        //Serial.println("Wait for too long, pass");
        return 0;
      }
    }
    int DataSize = client.available();
    if(DataSize){
      char ParsBuffer[11];
      unsigned char ParsBufferPos = 0;
      for (int i = 0 ; i < 11 ; i++ ){
        ParsBuffer[i] = client.read();
      }
      DataSize -= 11;
      while (!CompareBuffers(ComparedTimeBuffer, ParsBuffer, ParsBufferPos, 11)){
        ParsBuffer[ParsBufferPos] = client.read();
        ParsBufferPos++;
        DataSize--;
        if(ParsBufferPos > 10){
          ParsBufferPos = 0;
          }
        if(DataSize <= 0){
          
          //Serial.println();
          //Serial.println("Current Time: 0h");
          //Serial.println();
          //Serial.println("disconnected");

          client.stop();
          return 0;
        }
        }

      //SKIP THE ' : "2024-04-08T'
      for (char i = 0 ; i < 15 ; i++){
        client.read();
      }

      //Take the hours number
      char Content[3];
      Content[0] = client.read();
      Content[1] = client.read();
      Content[2] = '\0';

      //Serial.print(Content);
      //Serial.println("h");

      const String ContentString = Content;
      CurrentTime = ContentString.toInt();
    }
    
    
    // the server's disconnected, stop the client:
    client.stop();

    // Debug INFO
    //Serial.println();
    //Serial.println("Current Time : " + String(CurrentTime) + "h");
    //Serial.println();
    //Serial.println("disconnected");

  } else {// if not connected:
    // Debug INFO
    //Serial.println("connection failed");
    for (int i = 0; i < 5 ; i++){
    digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
    LowPower.sleep(4000);             // wait for a second
    digitalWrite(LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW
    LowPower.sleep(4000); 
  }
  }
  return CurrentTime;
}

int GetInverterPower(){
  // connect to web server on port 80:
  int CurrentPower = 0;
  if(client.connect(HOST_NAME, HTTP_PORT)) {
    // if connected: // Debug INFO
    //Serial.println("Connected to server");
    // make a HTTP request:
    // send HTTP header
    client.println(HTTP_METHOD + " " + PATH_NAME + " HTTP/1.1");
    client.println(X_Account_Key);
    client.println(X_API_Key);
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
        //Serial.println("Wait for too long, pass");
        return 0;
      }
    }
    int DataSize = client.available();
    if(DataSize){
      char ParsBuffer[5];
      unsigned char ParsBufferPos = 0;
      for (int i = 0 ; i < 5 ; i++ ){
        ParsBuffer[i] = client.read();
      }
      DataSize -= 5;
      while (!CompareBuffers(ComparedPowerBuffer, ParsBuffer, ParsBufferPos, 5)){
        ParsBuffer[ParsBufferPos] = client.read();
        ParsBufferPos++;
        DataSize--;
        if(ParsBufferPos > 4){
          ParsBufferPos = 0;
          }
        if(DataSize <= 0){
          
          //Serial.println();
          //Serial.println("Current Power Output: 0W");
          //Serial.println();
          //Serial.println("disconnected");

          client.stop();
          return 0;
        }
        }

      //SKIP THE " : {"
      for (char i = 0 ; i < 4 ; i++){
        client.read();
      }

      char Content[72];
      Content[0] = client.read();
      char LastReadPos = 0;
      while(Content[LastReadPos] != '}'){
        Content[LastReadPos + 1] = client.read();
        LastReadPos++;
      }
      Content[LastReadPos] = '\0';
      String ContentString = Content;
      char ValuePos = ContentString.indexOf('Value') + 5;
      String PowerString = ContentString.substring(ValuePos, ValuePos + 12);
      PowerString.trim();
      CurrentPower = PowerString.toInt();
    }
    
    
    // the server's disconnected, stop the client:
    client.stop();

    // Debug INFO
    //Serial.println();
    //Serial.println("Current Power Output: " + String(CurrentPower) + "W");
    //Serial.println();
    //Serial.println("disconnected");

  } else {// if not connected:
    // Debug INFO
    //Serial.println("connection failed");
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
  //Serial.begin(9600);
  //Waiting For The Serial Connection
  //while (!Serial);
  //Serial.println("Connected to Serial");
  // initialize the Ethernet shield using DHCP:
  EthernetSetup();
}

void loop() {
  //LED Request Always ON
  digitalWrite(LED_BUILTIN, HIGH);

  //PowerCheck 
  //Serial.println("Power Check");
  if (GetInverterPower() > PowerLimit){
    digitalWrite(SwitchPin, HIGH);
  } else {
    digitalWrite(SwitchPin, LOW);
  }

  //TimeCheck
  if(NumberOfRequestSinceLastTimeCheck >= NumberOfRequestBetweenTimeCheck){
    //Serial.println("Time Check");
    if(GetInverterTime() >= TimeToSleep){
      //Serial.println("Time to Sleep");
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

  //Maintain Internet Connection and If NOT : Setup It Again
  char Maintain = Ethernet.maintain();
  //Serial.println(Maintain);
  if(Maintain == 1 || Maintain == 3){
      EthernetSetup();
  }
}