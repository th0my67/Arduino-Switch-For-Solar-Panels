#include <SPI.h>
#include <Ethernet.h>
#include <string>

// DEBUG LED :
// ON 4s ; OFF 0.5s ; ON 1s ; OFF 0.5s : ETH Cable ERROR
// ON 1s ; OFF 0.5s ; ON 1s ; OFF 0.5s : DHCP Error
// ON 4s ; OFF 4s                      : No connection with the Host
// ON 1s ; OFF 4s                      : Waiting for the Next Request
// ON 0.1s ; OFF 0.1s                  : Waiting for the Response

// replace the MAC address below by the MAC address printed on a sticker on the Arduino Shield 2
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

EthernetClient client;

int    HTTP_PORT   = 80;
String HTTP_METHOD = "GET";
char   HOST_NAME[] = "192.168.1.170";
String PATH_NAME   = "/solar_api/v1/GetInverterRealtimeData.cgi";
String queryString = "?Scope=Device&DeviceId=1&DataCollection=CumulationInverterData";

const char ComparedBuffer[5] = {'"','P','A','C','"'};

const int SwitchPin = 6;
const int PowerLimit = 5000; 
const int DelayBetweenCheck = 90000;

void EthernetSetup(){
  if (Ethernet.linkStatus() != LinkON){
    // Debug INFO
    //Serial.println("Link status: Off");
    while(Ethernet.linkStatus() != LinkON){
      //LED ETH ERROR PATTERN
      digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
      delay(4000);                      
      digitalWrite(LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW
      delay(500); 
      digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
      delay(1000);  
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
      delay(1000);                      
      digitalWrite(LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW
      delay(500); 
      digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
      delay(1000);  
      digitalWrite(LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW
      delay(500); 
      ETHCode = Ethernet.begin(mac);
    }
  }
  
}

bool CompareBuffers(const char* pComparedBuffer, char* pParsBuffer, unsigned char Offset) {
    if (pComparedBuffer && pParsBuffer) {
        char DeltaPos;
        for (char i = 0; i < 5; i++) {
            DeltaPos = i + Offset;
            if (DeltaPos > 4) {
              DeltaPos -= 5;
            }
            if (*(pComparedBuffer + i) != *(pParsBuffer + DeltaPos)) {
                return false;
            }
        }
        return true;
    }
    return false;
}


int GetInverterPower(){
  // connect to web server on port 80:
  int CurrentPower = 0;
  if(client.connect(HOST_NAME, HTTP_PORT)) {
    // if connected: // Debug INFO
    //Serial.println("Connected to server");
    // make a HTTP request:
    // send HTTP header
    client.println(HTTP_METHOD + " " + PATH_NAME + queryString + " HTTP/1.1");
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
      while (!CompareBuffers(ComparedBuffer, ParsBuffer, ParsBufferPos)){
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
    delay(4000);                      // wait for a second
    digitalWrite(LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW
    delay(4000); 
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
  // initialize the Ethernet shield using DHCP:
  EthernetSetup();
}

void loop() {
  //LED Request Always ON
  digitalWrite(LED_BUILTIN, HIGH); 
  if (GetInverterPower() > PowerLimit){
    digitalWrite(SwitchPin, HIGH);
  } else {
    digitalWrite(SwitchPin, LOW);
  }
  
  //LED Waiting Pattern
  for (int i = 0; i <= DelayBetweenCheck ; i  += 5000){
    digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
    delay(1000);                      // wait for a second
    digitalWrite(LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW
    delay(4000); 
  }
  //Maintain Internet Connection and If NOT : Setup It Again
  char Maintain = Ethernet.maintain();
  //Serial.println(Maintain);
  if(Maintain == 1 || Maintain == 3){
      EthernetSetup();
  }
}