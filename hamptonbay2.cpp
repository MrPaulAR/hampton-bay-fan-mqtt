#include "rf-fans.h"


#define BASE_TOPIC HAMPTONBAY2_BASE_TOPIC

#define SUBSCRIBE_TOPIC_CMND_POWER "cmnd/" BASE_TOPIC "/+/power"        
#define SUBSCRIBE_TOPIC_CMND_FAN "cmnd/" BASE_TOPIC "/+/fan"
#define SUBSCRIBE_TOPIC_CMND_SPEED "cmnd/" BASE_TOPIC "/+/speed"     
#define SUBSCRIBE_TOPIC_CMND_LIGHT "cmnd/" BASE_TOPIC "/+/light"  

#define TX_FREQ 304.015 // a25-tx028 Hampton
            
// RC-switch settings
#define RF_PROTOCOL 14
#define RF_REPEATS  8 
                                    
// Keep track of states for all dip settings
static fan fans[16];

static int long lastvalue;
static unsigned long lasttime;

static void postStateUpdate(int id) {
  char outTopic[100];
  sprintf(outTopic, "stat/%s/%s/power", BASE_TOPIC, idStrings[id]);
  client.publish(outTopic, fans[id].powerState ? "ON":"OFF", true);
  sprintf(outTopic, "stat/%s/%s/fan", BASE_TOPIC, idStrings[id]);
  client.publish(outTopic, fans[id].fanState ? "ON":"OFF", true);
  sprintf(outTopic, "stat/%s/%s/speed", BASE_TOPIC, idStrings[id]);
  client.publish(outTopic, fanStateTable[fans[id].fanSpeed], true);
  sprintf(outTopic, "stat/%s/%s/light", BASE_TOPIC, idStrings[id]);
  client.publish(outTopic, fans[id].lightState ? "ON":"OFF", true);
}

static void transmitState(int fanId, int code) {
  mySwitch.disableReceive();         // Receiver off
  ELECHOUSE_cc1101.setMHZ(TX_FREQ);
  ELECHOUSE_cc1101.SetTx();           // set Transmit on
  mySwitch.enableTransmit(TX_PIN);   // Transmit on
  mySwitch.setRepeatTransmit(RF_REPEATS); // transmission repetitions.
  mySwitch.setProtocol(RF_PROTOCOL);        // send Received Protocol

  // Build out RF code
  //   Code follows the 24 bit pattern
  //   111111000110aaaa011cccdd
  //   Where a is the inversed/reversed dip setting, 
  //     ccc is the command (111 Power, 101 Fan, 100 Light, 011 Dim/Temp)
  //     dd is the data value
  int rfCode = 0xfc6000 | dipToRfIds[fanId] << 8 | (code&0xff);
  
  mySwitch.send(rfCode, 24);      // send 24 bit code
  mySwitch.disableTransmit();   // set Transmit off
  ELECHOUSE_cc1101.setMHZ(RX_FREQ);
  ELECHOUSE_cc1101.SetRx();      // set Receive on
  mySwitch.enableReceive(RX_PIN);   // Receiver on
  Serial.print("Sent command hamptonbay2: ");
  Serial.print(fanId);
  Serial.print(" ");
  Serial.println(code);
  postStateUpdate(fanId);
}

void hamptonbay2MQTT(char* topic, byte* payload, unsigned int length) {
  if(strncmp(topic, "cmnd/",5) == 0) {
    char payloadChar[length + 1];
    sprintf(payloadChar, "%s", payload);
    payloadChar[length] = '\0';
  
    // Get ID after the base topic + a slash
    char id[5];
    memcpy(id, &topic[sizeof(BASE_TOPIC)+5], 4);
    id[4] = '\0';
    if(strspn(id, idchars)) {
      uint8_t idint = strtol(id, (char**) NULL, 2);
      char *attr;
      // Split by slash after ID in topic to get attribute and action
    
      attr = strtok(topic+sizeof(BASE_TOPIC) + 5 + 5, "/");
          // Convert payload to lowercase
      for(int i=0; payloadChar[i]; i++) {
        payloadChar[i] = tolower(payloadChar[i]);
      }

      if(strcmp(attr,"fan") ==0) {
        if(strcmp(payloadChar,"on") == 0) {
          fans[idint].fanState = true;
          if(fans[idint].powerState==false) {
            fans[idint].powerState=true;
            transmitState(idint,0x7e); // Turn on
          }
          switch(fans[idint].fanSpeed) {
            case FAN_HI:
              transmitState(idint,0x74);
              break;
            case FAN_MED:
              transmitState(idint,0x75);
              break;
            case FAN_LOW:
              transmitState(idint,0x76);
              break;
          }
        } else {
          fans[idint].fanState = false;
          transmitState(idint,0x77);
        }
      } else if(strcmp(attr,"speed") ==0) {
        if(strcmp(payloadChar,"high") ==0) {
          fans[idint].fanState = true;
          fans[idint].fanSpeed = FAN_HI;
          if(fans[idint].powerState==false) {
            fans[idint].powerState=true;
            transmitState(idint,0x7e); // Turn on
          }
          transmitState(idint,0x74);
        } else if(strcmp(payloadChar,"medium") ==0) {
          fans[idint].fanState = true;
          fans[idint].fanSpeed = FAN_MED;
          if(fans[idint].powerState==false) {
            fans[idint].powerState=true;
            transmitState(idint,0x7e); // Turn on
          }
          transmitState(idint,0x75);
        } else if(strcmp(payloadChar,"low") ==0) {
          fans[idint].fanState = true;
          fans[idint].fanSpeed = FAN_LOW;
          if(fans[idint].powerState==false) {
            fans[idint].powerState=true;
            transmitState(idint,0x7e); // Turn on
          }
          transmitState(idint,0x76);
        } else {
          fans[idint].fanState = false;
          transmitState(idint,0x77);
        }
      } else if(strcmp(attr,"light") ==0) {
        if(strcmp(payloadChar,"on") == 0) {
          fans[idint].lightState = true;
          if(fans[idint].powerState==false) {
            fans[idint].powerState=true;
            transmitState(idint,0x7e); // Turn on
          }
          transmitState(idint,0x72);
        } else {
          fans[idint].lightState = false;
          transmitState(idint,0x71);
        }
      } else if(strcmp(attr,"power") ==0) {
        if(strcmp(payloadChar,"on") == 0) {
          fans[idint].powerState = true;
          transmitState(idint,0x7e);
        } else {
          fans[idint].powerState = false;
          transmitState(idint,0x7d);
        }
      }
    } else {
      // Invalid ID
      return;
    }
  }
}

void hamptonbay2RF(int long value, int prot, int bits) {
    if( (prot >10 && prot<15)  && bits == 24 && ((value&0xfff000)==0xfc6000)) {
      unsigned long t=millis();
      if(value == lastvalue) {
        if(t - lasttime < NO_RF_REPEAT_TIME)
          return;
        lasttime=t;
      }
      lastvalue=value;
      lasttime=t;
      int id = (value >> 8)&0x0f;
      // Got a correct id in the correct protocol
      if(id < 16) {
        // Convert to dip id
        int dipId = dipToRfIds[id];
        // Blank out id in message to get light state
        switch(value&0xff) {
          case 0x7e: // PowerOn
            fans[dipId].powerState = true;
            break;
          case 0x7d: // PowerOff
            fans[dipId].powerState = false;
            break;
          case 0x72: // LightOn
            fans[dipId].lightState = true;
            break;
          case 0x71: // LightOff
            fans[dipId].lightState = false;
            break;
          case 0x6e: // Light Dim
            break;
          case 0x6d: // Light Tempature (2k, 3k, 5k)
            break;
          case 0x74: // Fan High
            fans[dipId].fanState = true;
            fans[dipId].fanSpeed = FAN_HI;
            break;
          case 0x75: // Fan Med
            fans[dipId].fanState = true;
            fans[dipId].fanSpeed = FAN_MED;
            break;
          case 0x76: // Fan Low
            fans[dipId].fanState = true;
            fans[dipId].fanSpeed = FAN_LOW;
            break;
          case 0x77: // Fan Off
            fans[dipId].fanState = false;
            break;
        }
        postStateUpdate(dipId);
      }
    }
}

void hamptonbay2MQTTSub() {
  client.subscribe(SUBSCRIBE_TOPIC_CMND_POWER);
  client.subscribe(SUBSCRIBE_TOPIC_CMND_FAN);  
  client.subscribe(SUBSCRIBE_TOPIC_CMND_SPEED);
  client.subscribe(SUBSCRIBE_TOPIC_CMND_LIGHT);
}

void hamptonbay2Setup() {
  lasttime=0;
  lastvalue=0;
  // initialize fan struct 
  for(int i=0; i<16; i++) {
    fans[i].powerState = false;
    fans[i].lightState = false;
    fans[i].fanState = false;  
    fans[i].fanSpeed = FAN_LOW;
  }
}