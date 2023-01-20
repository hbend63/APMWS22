// Include Libraries
#include <WiFi.h>
#include <esp_now.h>
#include <ArduinoJson.h>
#include "FastLED.h"

#define NUM_LEDS 38
#define X_MAX 8
#define Y_MAX 6

CRGB leds[NUM_LEDS];
bool ledStates[NUM_LEDS]{0};

struct position
{
  // Konstruktor, damit korrekt initialisiert wird.
  position(String s,int x, int y, int led): uid(s), x(x),y(y),LEDadress(led) {};
  String uid;
  int x,y;
  int LEDadress;
};

int score{0};

int CHANNEL=6;
enum states{BEGIN,INIT,RUN,PAUSE,STOP,DONE};
states prgState=BEGIN;

// Global copy of slave
#define NUMSLAVES 20
esp_now_peer_info_t slaves[NUMSLAVES] = {};
int SlaveCnt = 0;
String slavePos[2][NUMSLAVES] = {};

//String uidList[3] = {"2d277fa8","da9b0eb0","04bc21d1700000"};

// Positopnen der RFID_Tags im x-y-Raster
position tagList[X_MAX*Y_MAX] =
{position("8de524a8",0,0,0),
 position("8d6629a8",1,0,1),
 position("4de524a8",2,0,0),
 position("5db52ba8",3,0,1),
 position("fd1234a8",4,0,0),
 position("4d8432a8",5,0,1),
 position("ddb52ba8",6,0,0),
 position("9d0c2ea8",7,0,1),

 position("ad4025a8",0,1,0),
 position("1d3227a8",1,1,1),
 position("6d4025a8",2,1,0),
 position("5d082ba8",3,1,1),
 position("fdf531a8",4,1,0),
 position("bdf531a8",5,1,1),
 position("9ddb35a8",6,1,0),
 position("7d6030a8",7,1,1),

 position("4d6629a8",0,2,0),
 position("1dd328a8",1,2,1),
 position("ada626a8",2,2,0),
 position("9d082ba8",3,2,1),
 position("dd032ea8",4,2,0),
 position("5d0d2ea8",5,2,1),
 position("cd1536a8",6,2,0),
 position("1d092ba8",7,2,1),

 position("dd3127a8",0,3,0),
 position("5d3227a8",1,3,1),
 position("ad602da8",2,3,0),
 position("ddd228a8",3,3,1),
 position("5db32fa8",4,3,0),
 position("3d6130a8",5,3,1),
 position("1d1636a8",6,3,0),
 position("6d612da8",7,3,1),

 position("0d6729a8",0,4,0),
 position("cd6629a8",1,4,1),
 position("5dd328a8",2,4,0),
 position("8d8432a8",3,4,1),
 position("1d0d2ea8",4,4,0),
 position("adb72fa8",5,4,1),
 position("5d8034a8",6,4,0),
 position("2db72fa8",7,4,1),

 position("1db52ba8",0,5,0),
 position("9dd328a8",1,5,1),
 position("dd3227a8",2,5,0),
 position("7d8334a8",3,5,1),
 position("fd6030a8",4,5,0),
 position("dd0c2ea8",5,5,1),
 position("6db72fa8",6,5,0),
 position("bd6030a8",7,5,1)
};
//
String stopUID = "f3aa90a9";

void initLED();

int findTag(String uid)
{
  int i=0;
  while (i<3) 
  {
    if (uid==tagList[i].uid)
       return i;
    i++;
  }
  return -1;
}

void updateSlavePos(String macAddress, String uuid)
{
  int i=0;
  while (i<3) 
  {
    if (macAddress==slavePos[0][i])
    {
       slavePos[1][i]=uuid;
       return;
    }
    i++;
  }
}

void showPositions()
{
  for (int i=0; i < SlaveCnt; i++)
  {
    Serial.print("slave: ");
    Serial.print(slavePos[0][i]);
    Serial.print(" ");
    Serial.println(slavePos[1][i]);
  }   
}

String toMessage(String message, String value)
{
    DynamicJsonDocument doc(1024);
    doc["message"]=message;
    doc["value"]=value;
    String output0;    
    serializeJson(doc, output0); // Serialize Json Document to output String
    return output0;    
}

void fromMessage(const String input, String &_message, String &_value)
{   
  if ((input[0]=='{') && (input[input.length()-1]=='}'))
  {     
     String json=input;
     Serial.print("JSON-String: "); Serial.println(json);
     // Allocate the JSON document
     //
     // Inside the brackets, 200 is the capacity of the memory pool in bytes.
     // Don't forget to change this value to match your JSON document.
     // Use https://arduinojson.org/v6/assistant to compute the capacity.
     StaticJsonDocument<200> doc;
     DeserializationError error = deserializeJson(doc, json);
     // Test if parsing succeeds.
    if (error) {
       Serial.print(F("deserializeJson() failed: "));
       Serial.println(error.f_str());
       return;
    }
    String message = doc["message"];
    String value = doc["value"];    
    _message=message;
    _value=value;    
  }  
} 
 
void formatMacAddress(const uint8_t *macAddr, char *buffer, int maxLength)
// Formats MAC Address
{
  snprintf(buffer, maxLength, "%02x:%02x:%02x:%02x:%02x:%02x", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
}
 
// Check if the slave is already paired with the master.
// If not, pair the slave with master
void manageSlave(const uint8_t *macAddr) {
  if (SlaveCnt > NUMSLAVES) // maximale Slaves erreicht
     return;
  bool exists = esp_now_is_peer_exist(macAddr);
  if (!exists) {
      
      memcpy(&slaves[SlaveCnt].peer_addr,macAddr, 6);
      
      esp_err_t addStatus = esp_now_add_peer(&slaves[SlaveCnt]);
        if (addStatus == ESP_OK) {
          // Pair success
          Serial.print("Pair success: ");
           // Format the MAC address
          char macStr[18];
          formatMacAddress(macAddr, macStr, 18);
          Serial.println(macStr);
          slavePos[0][SlaveCnt]=macStr;
          SlaveCnt++;
        } else if (addStatus == ESP_ERR_ESPNOW_NOT_INIT) {
          // How did we get so far!!
          Serial.println("ESPNOW Not Init");
        } else if (addStatus == ESP_ERR_ESPNOW_ARG) {
          Serial.println("Add Peer - Invalid Argument");
        } else if (addStatus == ESP_ERR_ESPNOW_FULL) {
          Serial.println("Peer list full");
        } else if (addStatus == ESP_ERR_ESPNOW_NO_MEM) {
          Serial.println("Out of memory");
        } else if (addStatus == ESP_ERR_ESPNOW_EXIST) {
          Serial.println("Peer Exists");
        } else {
          Serial.println("Not sure what happened");
        }
  } else
  {
     Serial.print("Already paired: ");
           // Format the MAC address
          char macStr[18];
          formatMacAddress(macAddr, macStr, 18);
          Serial.println(macStr);
  }
} 
 

void updateScoreDisplay()
{
   Serial.print("Score: ");
   Serial.println(score);
}

void setLED(int pos)
{
   if(ledStates[pos])
   {
      leds[pos] = CRGB::Black; FastLED.show();
      ledStates[pos] = false; // off
      score++;        
   }
  updateScoreDisplay();   
} 
 
void receiveCallback(const uint8_t *macAddr, const uint8_t *data, int dataLen)
// Called when data is received
{
  // Only allow a maximum of 250 characters in the message + a null terminating byte
  char buffer[ESP_NOW_MAX_DATA_LEN + 1];
  int msgLen = min(ESP_NOW_MAX_DATA_LEN, dataLen);
  //strncpy(buffer, (const char *)data, msgLen);

   // Format the MAC address
  char macStr[18];
  formatMacAddress(macAddr, macStr, 18);
 
  // Send Debug log message to the serial port
  Serial.printf("Received message from: %s\n", macStr);  
  
  String input{""};
  for (int i=0; i<dataLen; i++)
    input+=(char)*(data+i);
  
  String iMessage;
  String iValue; 
  fromMessage(input, iMessage, iValue);
  Serial.print("message: ");Serial.println(iMessage);
  Serial.print("value  : ");Serial.println(iValue);

  switch (prgState)
  {
    case INIT: if (iMessage=="HELLO?HELLO?")
                  manageSlave(macAddr);
               break;
    case RUN:  if (iMessage.indexOf("RFID:")==0)
               {
                 Serial.print("From Bot: ");
                 Serial.println(iValue);
                 int i = findTag(iValue);
                 Serial.print("Found in List: ");
                 Serial.println(i); 
                 updateSlavePos(macStr,iValue);
                 showPositions(); // for debugging
                 if (i >= 0)
                   setLED(i);                 
                 else
                   if (iValue==stopUID)
                     initLED();
               }
               break;              
  }  
}

void sentCallback(const uint8_t *macAddr, esp_now_send_status_t status)
// Called when data is sent
{
  char macStr[18];
  formatMacAddress(macAddr, macStr, 18);
  Serial.print("Last Packet Sent to: ");
  Serial.println(macStr);
  Serial.print("Last Packet Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}
 
void broadcast(const String &message)
// Emulates a broadcast
{
  // Broadcast a message to every device in range
  uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  esp_now_peer_info_t peerInfo = {};
  memcpy(&peerInfo.peer_addr, broadcastAddress, 6);
  //peerInfo.channel=WiFi.channel();
  if (!esp_now_is_peer_exist(broadcastAddress))
  {
    // Set esp-now-Channel
    peerInfo.channel=CHANNEL;
    esp_now_add_peer(&peerInfo);
  }
  // Send message
  esp_err_t result = esp_now_send(broadcastAddress, (const uint8_t *)message.c_str(), message.length());
 
  // Print results to serial monitor
  if (result == ESP_OK)
  {
    Serial.println("Broadcast message success");
  }
  else if (result == ESP_ERR_ESPNOW_NOT_INIT)
  {
    Serial.println("ESP-NOW not Init.");
  }
  else if (result == ESP_ERR_ESPNOW_ARG)
  {
    Serial.println("Invalid Argument");
  }
  else if (result == ESP_ERR_ESPNOW_INTERNAL)
  {
    Serial.println("Internal Error");
  }
  else if (result == ESP_ERR_ESPNOW_NO_MEM)
  {
    Serial.println("ESP_ERR_ESPNOW_NO_MEM");
  }
  else if (result == ESP_ERR_ESPNOW_NOT_FOUND)
  {
    Serial.println("Peer not found.");
  }
  else
  {
    Serial.println("Unknown error");
  }
}
 
void initESP()
{
  Serial.println("Initializing WiFi ...");
  // Set ESP32 in STA mode to begin with
  WiFi.mode(WIFI_STA);
  // Connect to AP
  WiFi.begin("MBotAccess","makeblock");
  while (!WiFi.isConnected());
  // Get Channel for ESP-Now
  CHANNEL=WiFi.channel();   
  // Print MAC address
  Serial.print("WiFi-MAC Address: ");
  Serial.println(WiFi.macAddress()); 
  // Disconnect from WiFi
  WiFi.disconnect();
  Serial.println("WiFi Init Success");
  // Initialize ESP-NOW
  Serial.println("Initializing ESP-NOW ...");
  if (esp_now_init() == ESP_OK)
  {
    Serial.println("ESP-NOW Init Success");
    esp_now_register_recv_cb(receiveCallback);
    esp_now_register_send_cb(sentCallback);
  }
  else
  {
    Serial.println("ESP-NOW Init Failed");
    delay(3000);
    ESP.restart();
  } 
}

void initPartners()
{
  String message="HELLO?";
  String value=WiFi.macAddress(); // eigene MAC-Adresse
  // 30 Sekunden Initialisierung der Spielepartner
  for (int i=0; i < 6; i++)
  {
     broadcast(toMessage(message,value));
     delay(5000);
  }
  // Ausgabe der Teilnehmerliste
  for (int i=0; i< SlaveCnt; i++)
  {
      Serial.print("Slave: ");
      char macStr[18];
      formatMacAddress(slaves[i].peer_addr, macStr, 18);
      Serial.println(macStr);
  }
  prgState=RUN;
}

void initLED()
{
  FastLED.clear();
  FastLED.addLeds<NEOPIXEL, 16>(leds, NUM_LEDS); 
  // Checkrun
  for (int i=0; i < NUM_LEDS; i++)
    leds[i] = CRGB::Green; FastLED.show();
  delay(200);
  for (int i=0; i < NUM_LEDS; i++)
  {
    leds[i] = CRGB::Black; FastLED.show();
    delay(100);
  }
  // All on
  for (int i=0; i < NUM_LEDS; i++)
  {
    leds[i] = CRGB::Green; FastLED.show();
    ledStates[i] = true; // on
    score=0;
  }  
  delay(200);
  Serial.println("Game setup ok!");
  updateScoreDisplay();
}

void initSYSTEM()
{
   prgState=INIT;
   initESP();
   initPartners();
   initLED();  
}

void setup()
{ 
  // Set up Serial Monitor
  Serial.begin(115200);
  delay(1000);
  initSYSTEM();  
}

 
void loop()
{  
 
}