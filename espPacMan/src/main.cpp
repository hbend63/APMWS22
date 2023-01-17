// Include Libraries
#include <WiFi.h>
#include <esp_now.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <MFRC522.h>

// Set here the mac-Address of the MBot2
uint8_t botMacAddr[]={0xac,0x0b,0xfb,0x25,0x5c,0x44};
uint8_t masterMacAddr[6]={0};

int CHANNEL=6;

enum states{BEGIN,INIT,RUN,PAUSE,STOP,DONE};

states prgState=BEGIN;

#define SS_PIN  5  // ESP32 pin GIOP5 
#define RST_PIN 27 // ESP32 pin GIOP27 

MFRC522 rfid(SS_PIN, RST_PIN);

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
 
void setMaster(const uint8_t *macAddr)
{
  esp_now_peer_info_t peerInfo = {};
  memcpy(&peerInfo.peer_addr, macAddr, 6); 
  if (!esp_now_is_peer_exist(macAddr))
  {
    // Set esp-now-Channel
    peerInfo.channel=CHANNEL;
    memcpy(masterMacAddr, macAddr, 6); 

    esp_now_add_peer(&peerInfo);
    Serial.println("Master added");
    String message="HELLO?HELLO?";
    String value=WiFi.macAddress();
    String toSend=toMessage(message,value);
    esp_err_t result = esp_now_send(masterMacAddr, (const uint8_t *)toSend.c_str(), toSend.length());   
    
    message="BOT_LED_OK";
    value=WiFi.macAddress();
    toSend=toMessage(message,value);
    result = esp_now_send(botMacAddr, (const uint8_t *)toSend.c_str(), toSend.length());   
    
    prgState=RUN;
  } 
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

  if (prgState==INIT)
  {
     if (iMessage=="BotOk!")
        Serial.println("Bot ok!");
     else if (iMessage=="HELLO?")   
        setMaster(macAddr);
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

void connectBot()
{
  esp_now_peer_info_t peerInfo = {};
  memcpy(&peerInfo.peer_addr, botMacAddr, 6); 
  if (!esp_now_is_peer_exist(botMacAddr))
  {
    // Set esp-now-Channel
    peerInfo.channel=CHANNEL;
    esp_now_add_peer(&peerInfo);
  }
  String message="BOT?";
  String value=WiFi.macAddress();
  
  String toSend=toMessage(message,value);
  esp_err_t result = esp_now_send(botMacAddr, (const uint8_t *)toSend.c_str(), toSend.length());   
}

void initSYSTEM()
{
   prgState=INIT;
   initESP();
   connectBot();
}


void setup()
{ 
  // Set up Serial Monitor
  Serial.begin(115200);
  delay(1000);
  initSYSTEM();
  SPI.begin(); // init SPI bus
  rfid.PCD_Init(); // init MFRC522
  
}


char* hex02str(byte b)
{
  static char str[]="02"; // RAM für einen 2-Zeichen string reservieren.
  snprintf(str,sizeof(str),"%02x",b);
  return str;
}

String UIDtoString()
// Formats MAC Address
{
  String result="";
  for (int i = 0; i < rfid.uid.size; i++) {          
      result+=hex02str(rfid.uid.uidByte[i]);     
  }
  return result;
}



states lastState=INIT;
 
void loop()
{  
  if ((lastState==INIT) && (prgState==RUN))
  {
    lastState=prgState;
    Serial.println("Tap an RFID/NFC tag on the RFID-RC522 reader");  
  }
  if (prgState==RUN)
  {
    if (rfid.PICC_IsNewCardPresent()) { // new tag is available
    if (rfid.PICC_ReadCardSerial()) { // NUID has been readed
      MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
      Serial.print("RFID/NFC Tag Type: ");
      Serial.println(rfid.PICC_GetTypeName(piccType));
      
      // print UID in Serial Monitor in the hex format
      Serial.print("UID:");     
     
      for (int i = 0; i < rfid.uid.size; i++) {
        Serial.print(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
        Serial.print(rfid.uid.uidByte[i], HEX);
        
      }
      Serial.println();
      String value=UIDtoString();
      Serial.println(value);
      String message="RFID:";
      String toSend=toMessage(message,value);
      esp_err_t result = esp_now_send(botMacAddr, (const uint8_t *)toSend.c_str(), toSend.length());     
      message+="#"+WiFi.macAddress();  
      toSend=toMessage(message,value);
      result = esp_now_send(masterMacAddr, (const uint8_t *)toSend.c_str(), toSend.length()); 
      
      rfid.PICC_HaltA(); // halt PICC
      rfid.PCD_StopCrypto1(); // stop encryption on PCD
    }
   }
  } else {
    Serial.println("Waiting for Master..");
    delay(1000);
  }
}