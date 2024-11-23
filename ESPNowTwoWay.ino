#include <ESP8266WiFi.h>
#include <espnow.h>
#include <Embedis.h>
#include <EEPROM.h>
#include "spi_flash.h"

Embedis embedis(Serial);
bool isConfigMode = false;

// REPLACE WITH THE MAC Address of your receiver  
byte broadcastAddress[] =  {0x24, 0x4C, 0xAB, 0x48, 0xD4, 0x0E};

#define UART_BAUD 115200
#define packTimeout 5 // ms (if nothing more on UART, then send packet)
#define bufferSize 200 // using esp now you can only send 250 bytes at a time

#define CONFIG_PIN 14

//Structure example to send data
//Must match the receiver structure
typedef struct struct_message {
    uint8_t buf1[bufferSize];    
} struct_message;

// Counters for incoming data through UART
uint16_t i1=0;

// Create a struct_message to hold outgoing data
struct_message outgoingData;

// Create a struct_message to hold incoming data
struct_message incomingData;

// Callback when data is sent
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  // Serial.print("Last Packet Send Status: ");
  // if (sendStatus == 0){
  //   Serial.println("Delivery success");
  // }
  // else{
  //   Serial.println("Delivery fail");
  // }
}

// Callback when data is received
void OnDataRecv(uint8_t * mac, uint8_t *_incomingData, uint8_t len) {
  memcpy(&incomingData, _incomingData, sizeof(incomingData));
    // Serial.print("Bytes received: ");
    // Serial.println(len);
    // for( uint8_t i = 0; i < len; i++ ) { // with all bytes of DataStream 
    //   Serial.print(incomingData[i], HEX); Serial.write(' ');
    // } 
    // Serial.println(); 

    Serial.flush();
    for(int k = 0; k < len; k++){
        Serial.write(incomingData.buf1[k]);
        // Serial.print(_sendBuffer[k],HEX);
    }
    Serial.flush();    
}
 
void setup() {
  // Init Serial Monitor
  Serial.begin(UART_BAUD);
  Serial.println("");
  // Checking if esp is in config MODE or not (isConfigMode)
  pinMode(CONFIG_PIN, INPUT_PULLUP);
  int configPinState = 0;
  for (int i = 0; i < 50; i++) {
    if(digitalRead(CONFIG_PIN) == LOW) {
      configPinState++;
    }
    delay(10);
  }
  if(configPinState > 40) {
    isConfigMode = true;
    Serial.println("ESP is now in Config Mode...");
  }

  // Create a key-value Dictionary in emulated EEPROM (FLASH actually...)
  EEPROM.begin(SPI_FLASH_SEC_SIZE);
  Embedis::dictionary( 
      "EEPROM",
      SPI_FLASH_SEC_SIZE,
      [](size_t pos) -> char { return EEPROM.read(pos); },
      [](size_t pos, char value) { EEPROM.write(pos, value); },
      []() { EEPROM.commit(); }
  );

  if(isConfigMode) {

    // Add command to set broadcastAddress to EEPROM
    Embedis::command( F("setbrdaddr"), [](Embedis* e) {
        if (e->argc != 2) return e->response(Embedis::ARGS_ERROR);
        String addrString = String(e->argv[1]);
        if(addrString.length() != 12) {
            return e->response(Embedis::ARGS_ERROR);
        }
        setSetting("broadcastAddress", addrString);
        EEPROM.commit();
        Serial.println("Setting broadcast address...");
    });

    // Add command to read broadcastAddress from EEPROM
    Embedis::command( F("getbrdaddr"), [](Embedis* e) {
        if (e->argc != 1) return e->response(Embedis::ARGS_ERROR);
        Serial.println("Reading broadcast address...");
        String addrString = getSetting("broadcastAddress");
        if(addrString.length() != 12) {
            return e->response(Embedis::ARGS_ERROR);
        }        
        char_array_to_byte_array(addrString.c_str(), broadcastAddress, addrString.length());
        for (uint8_t i = 0; i < 6; i++) {
            Serial.print(broadcastAddress[i], HEX);
        }
        Serial.println("");
    });      
  }

  // Load the broadcast address from EEPROM
  Serial.println("Reading broadcast address...");
  String addrString = getSetting("broadcastAddress");
  if(addrString.length() != 12) {
      Serial.println("Stored broadcastAddress address is WRONG...");
  } else {     
    char_array_to_byte_array(addrString.c_str(), broadcastAddress, addrString.length());
    for (uint8_t i = 0; i < 6; i++) {
        Serial.print(broadcastAddress[i], HEX);
    }
    Serial.println("");
  }

  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Init ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Set ESP-NOW Role
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  
  // Register peer
  esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_COMBO, 1, NULL, 0);

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);
  
  // Register for a callback function that will be called when data is received
  esp_now_register_recv_cb(OnDataRecv);

  Serial.println("Setup done.......");
}
 
void loop() {

  if(isConfigMode) {
    embedis.process();
      /* give delay - for any internal RTOS to switch context */
    delay(20);
    return;
  }

  if(Serial.available()) {

    // read the data until pause:
    while(1) {
      if(Serial.available()) {
        outgoingData.buf1[i1] = (char)Serial.read(); // read char from UART
        if(i1<bufferSize-1) {
          i1++;
        } else {
          // Serial.println("Buffer full......");
          break;
        }
      } else {
        //delayMicroseconds(packTimeoutMicros);
        delay(packTimeout);
        if(!Serial.available()) {
          // Serial.println("Receiving serial is done......");
          break;
        }
      }
    }
    
    // esp_now_send(broadcastAddress, (uint8_t *) &buf1[0], sizeof(buf1));
    esp_now_send(broadcastAddress, (uint8_t *) &outgoingData, i1);
    // Serial.println("Sending dtat to esp......");
    i1 = 0;
  }
}

// Function for converting hex string into byte array
void char_array_to_byte_array(const char *str, byte byte_arr[], uint8_t string_length) {
    int index, index1, index2;
    char temp[3];
    // Sometimes we are sending 32 as fixed in string_length, but if str does not support that than we should return from here. 
    if(strlen(str) < string_length){
        return ; 
    }
    //   Serial.println(str);
    // Use 'nullptr' or 'NULL' for the second parameter.
    for (index = 0; index < string_length - 1; index += 2) {
        for (index1 = index, index2 = 0; index1 < index + 2; index1++, index2++) {
            temp[index2] = str[index1];
        }
        temp[index2] = '\0';
        unsigned long number = strtoul(temp, nullptr, 16);

        byte_arr[index / 2] = (byte)number;
    }
}

// get/set functions for the stored data

template <typename T> String getSetting(const String& key, T defaultValue) {
    String value;
    if (!Embedis::get(key, value)) value = String(defaultValue);
    return value;
}

template <typename T> String getSetting(const String& key, unsigned int index, T defaultValue) {
    return getSetting(key + String(index), defaultValue);
}

String getSetting(const String& key) {
    return getSetting(key, "");
}

template <typename T> bool setSetting(const String& key, T value) {
    return Embedis::set(key, String(value));
}

template <typename T> bool setSetting(const String& key, unsigned int index, T value) {
    return setSetting(key + String(index), value);
}

bool delSetting(const String& key) {
    return Embedis::del(key);
}

bool delSetting(const String& key, unsigned int index) {
    return delSetting(key + String(index));
}
