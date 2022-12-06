/*
  ESP-NOW Multi Unit Demo
  esp-now-multi.ino
  Broadcasts control messages to all devices in network
  Load script on multiple devices
 
  DroneBot Workshop 2022
  https://dronebotworkshop.com
*/
 
// Include Libraries
#include <WiFi.h>
#include <esp_now.h>

int CHANNEL=6;


String toMessage(String message, String value)
{
    // String wird komplett zusammengesetzt. Für # werden die Steuerkennzeichen eingefügt   
    String toSend{"#####{\"message\": "+message+", \"value\": "+value+"}###"}; 
    int len=toSend.length();
    // Vorspann #####
    toSend[0]=(char)0xf3; // Steuerkennzeichen Start
    toSend[1]=(char)(16+message.length()); // Steuerkennzeichen Länge der Message ohne Wert
    toSend[2]=(char)(26+message.length()+value.length()); // Steuerkennzeichen Länge der gesamten Message mit Wert
    toSend[3]=(char)(0); // Steuerkennzeichen immer Null
    toSend[4]=(char)(3); // Steuerkennzeichen immer Drei
    // Prüfsummenberechnung über alle Bytes von Byte 3 bis Byte len-3
    uint8_t psum=0;
    for (int i=3; i<len-3;i++)
      psum+=(uint8_t)toSend[i];
    // Nachspann ###
    toSend[len-3]=(char)(0);  // Steuerkennzeichen immer Null
    toSend[len-2]=(char)(psum); // Steuerkennzeichen Prüfsumme
    toSend[len-1]=(char)(0xf4); // Steuerkennzeichen Ende
    Serial.println("My message");
    
    for (int i=0; i < toSend.length(); i++)
    {
      char dt[10];
      snprintf(dt,sizeof(dt),"%02x",toSend[i]);
      Serial.print(dt);
      Serial.print(" ");    
    }  
    Serial.println("");
    Serial.println(toSend);
    return toSend;
}

void fromMessage(const String input, String &message, String &value)
{ 
  // Testausgabe Rohdaten   
  Serial.println("Got: ");
  for (int i=0; i < input.length(); i++)
  {
     char dt[10];
     snprintf(dt,sizeof(dt),"%02x",input[i]);
     Serial.print(dt);
     Serial.print(" ");    
  }  
  Serial.println("");  
 
  if ((input[0]==0xf3) && (input[input.length()-1]==0xf4))
  {

     int l1= input[1];
     int l2= input[2];
    
     String st1=input.substring(6,l1);
     String st2=input.substring(l1+2,l2+1);
    
     message=st1.substring(st1.indexOf(':')+1,st1.length());
     value=st2.substring(st2.indexOf(':')+1,st2.length());
    
     Serial.print("Message: "); Serial.println(message);
     Serial.print("Value: "); Serial.println(value);
  }  
} 
 
void formatMacAddress(const uint8_t *macAddr, char *buffer, int maxLength)
// Formats MAC Address
{
  snprintf(buffer, maxLength, "%02x:%02x:%02x:%02x:%02x:%02x", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
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
 
void setup()
{
 
  // Set up Serial Monitor
  Serial.begin(115200);
  delay(1000);
 
  // Set ESP32 in STA mode to begin with
  WiFi.mode(WIFI_STA);
  // Connect to AP
  WiFi.begin("MBotAccess","makeblock");
  while (!WiFi.isConnected());
  // Get Channel
  CHANNEL=WiFi.channel();
  
  Serial.println("ESP-NOW Broadcast Demo");
 
  // Print MAC address
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
 
  // Disconnect from WiFi
  WiFi.disconnect();

 

 
  // Initialize ESP-NOW
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


String message="\"message\"";
String value="100";
 
void loop()
{  
 broadcast(toMessage(message,value));
 delay(5000);
}