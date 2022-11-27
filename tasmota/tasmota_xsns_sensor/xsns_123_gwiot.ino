/*
  xsns_123_gwiot.ino - Support for GWIOT NFC Tags Readers on Tasmota

  Copyright (C) 2022  Chompy and Gerhard Mutz(RDM6300) and Theo Arends

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef USE_GWIOT
/*********************************************************************************************\
 * Gwiot 7951M/7941E? RFID reader 
 *
 * Expected variable byte data (at least 5):
 *  0  1  2  3  4  5  6  7  8    9
 * Hd Len T ---- UID ---- Chksm Tl
 * 02 A0 02 2E 00 B6 D7 B5 F2   03 = 02-A0-02-2E00B6D7B5-F2-03
 *    
\*********************************************************************************************/

#define XSNS_123            123

#define GWIOT_BAUDRATE     9600
#define GWIOT_TIMEOUT       100
#define GWIOT_BLOCK      2 * 10   // 2 seconds block time
#define GWIOT_LEN            50

#include <TasmotaSerial.h>
TasmotaSerial *GWIOTSerial = nullptr;

struct {
  char uid[GWIOT_LEN*2] = "\0";
  uint8_t block_time = 0;
} GWIOT;

/********************************************************************************************/
/*
uint8_t RDM6300HexNibble(char chr) {
  uint8_t rVal = 0;
  if (isdigit(chr)) { rVal = chr - '0'; }
  else if (chr >= 'A' && chr <= 'F') { rVal = chr + 10 - 'A'; }
  else if (chr >= 'a' && chr <= 'f') { rVal = chr + 10 - 'a'; }
  return rVal;
}

// Convert hex string to int array
void RDM6300HexStringToArray(uint8_t array[], uint8_t len, char buffer[]) {
  char *cp = buffer;
  for (uint32_t i = 0; i < len; i++) {
    uint8_t val = RDM6300HexNibble(*cp++) << 4;
    array[i] = val | RDM6300HexNibble(*cp++);
  }
}
*/
/********************************************************************************************/

void GWIOTInit() {
  if (PinUsed(GPIO_GWIOT_RX)) {
    GWIOTSerial = new TasmotaSerial(Pin(GPIO_GWIOT_RX), -1, 1);
    if (GWIOTSerial->begin(GWIOT_BAUDRATE)) {
      if (GWIOTSerial->hardwareSerial()) {
        ClaimSerial();
      }
    }
  }
}

void GWIOTScanForTag() {
  if (!GWIOTSerial) { return; }

  if (GWIOT.block_time > 0) {
    GWIOT.block_time--;
    while (GWIOTSerial->available()) {
      GWIOTSerial->read();               // Flush serial buffer
    }
    return;
  }

  if (GWIOTSerial->available()) {

    char c = GWIOTSerial->read();
    if (c != 2) { return; }                // Head marker
    
    char GWIOT_buffer[GWIOT_LEN];
    uint8_t GWIOT_index = 0;

    GWIOT_buffer[GWIOT_index++] = c;

    // read size
    if (!GWIOTSerial) { return; }
    c = GWIOTSerial->read();
    uint8_t GWIOT_size = c+0;
    
    GWIOT_buffer[GWIOT_index++] = c;

    uint32_t cmillis = millis();
    while (GWIOT_index > GWIOT_size) {
      if (GWIOTSerial->available()) {
        c = GWIOTSerial->read();
        GWIOT_buffer[GWIOT_index++] = c;
      }
      if ((millis() - cmillis) > GWIOT_TIMEOUT) {
        return;                   // Timeout
      }
    }

    AddLogBuffer(LOG_LEVEL_DEBUG, (uint8_t*)GWIOT_buffer, sizeof(GWIOT_buffer));

    if (GWIOT_buffer[GWIOT_size-1] != 3) { return; }   // Tail marker

    GWIOT.block_time = GWIOT_BLOCK;        // Block for 2 seconds

    uint8_t rdm_array[GWIOT_LEN];
    /*
    RDM6300HexStringToArray(rdm_array, sizeof(rdm_array), (char*)rdm_buffer +1);
    uint8_t accu = 0;
    for (uint32_t count = 0; count < 5; count++) {
      accu ^= rdm_array[count];            // Calc checksum,
    }
    if (accu != rdm_array[5]) { return; }  // Checksum error
    */
    //Print in hex
    char * ptr=GWIOT.uid;
    for(uint8_t i=3 ; i<GWIOT_size-2 ; i++){
	sprintf(ptr,"%2X",GWIOT_buffer[i]);
	ptr+=2;
    }

    ResponseTime_P(PSTR(",\"GWIOT\":{\"UID\":\"%s\"}}"), GWIOT.uid);
    MqttPublishTeleSensor();
    
  }
}

#ifdef USE_WEBSERVER
void GWIOTShow(void) {
  if (!GWIOTSerial) { return; }
  WSContentSend_PD(PSTR("{s}GWIOT UID{m}%s {e}"), GWIOT.uid);
}
#endif  // USE_WEBSERVER

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xsns123(uint32_t function) {
  bool result = false;

  switch (function) {
    case FUNC_INIT:
      GWIOTInit();
      break;
    case FUNC_EVERY_100_MSECOND:
      GWIOTScanForTag();
      break;
#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      GWIOTShow();
      break;
#endif  // USE_WEBSERVER
  }
  return result;
}

#endif  // USE_GWIOT
