#include <ESP8266WiFi.h>
#include <espnow.h>

/*
-----------------------------------------------------------------------------------
| frame header | control word | command word | length | data | checksum | end     |
-----------------------------------------------------------------------------------
| 2 bytes      | 1 byte       | 1 byte       | 2 bytes| n    | 1 byte   | 2 bytes |
-----------------------------------------------------------------------------------
| 53 59        |  08          |  01          |  00 05 |34 0A 03 00 0A | 05 | 54 43|
-----------------------------------------------------------------------------------
*/

// REPLACE WITH THE MAC Address of your receiver  {0x40, 0x91, 0x51, 0x67, 0xF2, 0x5B};{0x24, 0x4C, 0xAB, 0x48, 0xD4, 0x0E};//
uint8_t broadcastAddress[] = {0x48, 0x55, 0x19, 0xE0, 0x8B, 0xC3}; //{0x40, 0x91, 0x51, 0x67, 0xF2, 0x5B}; // 


#define UART_BAUD 115200
#define packTimeout 5 // ms (if nothing more on UART, then send packet)
#define bufferSize 200 // using esp now you can only send 250 bytes at a time

#define DELAY_BOOT 1
#define BOOT_PIN 14

uint8_t buf1[bufferSize];
uint16_t i1=0;

// uint8_t buf2[bufferSize];
uint16_t i2=0; 

//Structure example to send data
//Must match the receiver structure
typedef struct struct_message {
    uint8_t buf2[bufferSize];    
} struct_message;

// Create a struct_message called DHTReadings to hold sensor readings
struct_message DHTReadings;

// Create a struct_message to hold incoming sensor readings
struct_message incomingReadings;

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
void OnDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len) {
  memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
    // Serial.print("Bytes received: ");
    // Serial.println(len);
    // for( uint8_t i = 0; i < len; i++ ) { // with all bytes of DataStream 
    //   Serial.print(incomingData[i], HEX); Serial.write(' ');
    // } 
    // Serial.println(); 

    Serial.flush();
    for(int k = 0; k < len; k++){
        Serial.write(incomingReadings.buf2[k]);
        // Serial.print(_sendBuffer[k],HEX);
    }
    Serial.flush();    
}
 
void setup() {
  // Init Serial Monitor
  Serial.begin(UART_BAUD);

#if DELAY_BOOT
  pinMode(BOOT_PIN, OUTPUT);
  digitalWrite(BOOT_PIN, LOW);
  Serial.println("Sensor booting up......");
  delay(20000);
  digitalWrite(BOOT_PIN, HIGH);
#endif 
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

  if(Serial.available()) {

    // read the data until pause:
    while(1) {
      if(Serial.available()) {
        DHTReadings.buf2[i2] = (char)Serial.read(); // read char from UART
        if(i2<bufferSize-1) {
          i2++;
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
    
    // esp_now_send(broadcastAddress, (uint8_t *) &buf2[0], sizeof(buf2));
    esp_now_send(broadcastAddress, (uint8_t *) &DHTReadings, i2);
    // Serial.println("Sending dtat to esp......");
    i2 = 0;
  }
}
