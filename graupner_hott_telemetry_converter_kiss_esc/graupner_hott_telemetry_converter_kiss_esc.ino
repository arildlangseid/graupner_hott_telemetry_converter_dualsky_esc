#include "HoTTServer.h"

//#define DEBUG_PRINT

// KISS Telemetry in port
HardwareSerial &dsSerial = Serial1;

/*
*
*	HoTT Stuff below
*
*/
static unsigned long timeHoTT_update = 0;

#define HOTT_RX 10
#define HOTT_TX 11
//#define HOTT_RX 13
//#define HOTT_TX 5

HoTTServer server(HOTT_RX, HOTT_TX);  // rx, tx

/*
*
* setup()
*
*/
void setup() {
  // put your setup code here, to run once:
#ifdef DEBUG_PRINT
  Serial.begin(115200);  // open serial0 for serial monitor
  delay(2000);
  Serial.println("Start.......");
#endif

  /*
  *
  *	DualSky/KISS Stuff below
  *
  */
  dsSerial.begin(115200);  // open dsSerial for ESC communication


  /*
  *
  *	HoTT Stuff below
  *
  */
  server.registerModule(HoTTServerESC);
  server.start();
}


/*
*
* loop()
*
*/
#ifdef DEBUG_PRINT
static uint16_t capacityCounter = 0;
#endif
void loop() {
  receiveTelemtrie();

  if (unsigned long timeNow = millis() - timeHoTT_update > 200) {
    timeHoTT_update = timeNow;
#ifdef DEBUG_PRINT
    capacityCounter++;
    server.setCapacity(capacityCounter);
#endif
    server.processRequest();
  }
}

// get the Telemetrie from the ESC
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//static uint8_t SerialBuf[10];
#define SERIAL_BUF_LEN 20
#define KISS_PROTOCOL_LEN 10
#define KISS_FRAME_MIN 27000
#define KISS_FRAME_MAX 32000
static uint8_t SerialBuf[SERIAL_BUF_LEN];
static uint8_t TestBuf[KISS_PROTOCOL_LEN];
void receiveTelemtrie() {
  int counter = 0;
  unsigned long time_start = micros();
  unsigned long time_last = time_start;
  unsigned long time_now = time_start;
  unsigned long time_since_last = 0;
  unsigned long time_timeout = time_start + 35000;
  unsigned long max_time = 0;
  while (counter < SERIAL_BUF_LEN && time_now < time_timeout) {
    if (dsSerial.available()) {
      time_now = micros();
      time_since_last = time_now - time_last;
      uint8_t data = dsSerial.read();
      SerialBuf[counter] = data;
      if (max_time < time_since_last) max_time = time_since_last;
#ifdef DEBUG_PRINT
//      Serial.println(max_time);
#endif
//      delayMicroseconds(50);
      counter++;
    }
    if (counter > KISS_PROTOCOL_LEN && max_time > KISS_FRAME_MIN && max_time < KISS_FRAME_MAX) {
      counter--;
/*
      Serial.print("Break: ");
      Serial.print(counter);
      Serial.print(", ");
      Serial.print(max_time);
      Serial.println("");
*/
      break;
    }
    time_last = time_now;
  }

  if (counter >= KISS_PROTOCOL_LEN && max_time > KISS_FRAME_MIN && max_time < KISS_FRAME_MAX) {
    for (int i = 0; i<KISS_PROTOCOL_LEN; i++) {
      TestBuf[i] = SerialBuf[counter - KISS_PROTOCOL_LEN + i];
#ifdef DEBUG_PRINT
      Serial.print(TestBuf[i]); Serial.print(", ");
#endif
    }
#ifdef DEBUG_PRINT
    Serial.print(" : "); Serial.print(max_time); Serial.print(" : "); Serial.print(micros()-time_start);
#endif
  } else {
#ifdef DEBUG_PRINT
    Serial.print("Return: ");
    Serial.print(counter);
    Serial.print(", ");
    Serial.print(max_time);
    Serial.println("");
#endif
    return;
  }

  uint8_t crc8 = get_crc8(TestBuf, 9);  // get the 8 bit CRC
  if (crc8 != 0 && crc8 == TestBuf[9]) {
#ifdef DEBUG_PRINT
    Serial.print(", Good");
//    Serial.println("");
#endif
    int16_t volt = (TestBuf[1] << 8) | TestBuf[2];
    int16_t current = (TestBuf[3] << 8) | TestBuf[4];
    int16_t rpm = (TestBuf[7] << 8) | TestBuf[8];
    float voltage = (float)((float)volt / 100);
    server.setTemperature(HOTT_ESC_TEMPERATURE, 20 + TestBuf[0]);
    server.setVoltage(HOTT_MAIN_VOLTAGE, voltage);
    server.setCurrent(HOTT_MAIN_CURRENT, (float)((float)current / 100));
    server.setCapacity((TestBuf[5] << 8) | TestBuf[6]);
    server.setRPM(HOTT_PRIMARY_RPM, rpm);

#ifdef DEBUG_PRINT
  Serial.print(", ");
  Serial.print(server.getBattAlarmV());
  Serial.print(", ");
  Serial.println(voltage);
#endif
    if ( voltage < server.getBattAlarmV() ) {
      server.setWarning(HOTT_ALARM_SENSOR1_VOLTAGE_MIN);
#ifdef DEBUG_PRINT
      Serial.println("******** ALARM *********");
#endif
    } else {
      server.setWarning(HOTT_ALARM_NONE);
#ifdef DEBUG_PRINT
      Serial.println("******** OK OK OK OK OK  *********");
#endif
    }

  } else {
#ifdef DEBUG_PRINT
    Serial.print(", Failed, ");
    Serial.print(counter);
    Serial.print(", ");
    Serial.print(max_time);
    Serial.println("");
#endif
  }

  return;
}

// 8-Bit CRC
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
uint8_t update_crc8(uint8_t crc, uint8_t crc_seed) {
  uint8_t crc_u, i;
  crc_u = crc;
  crc_u ^= crc_seed;
  for (i = 0; i < 8; i++) crc_u = (crc_u & 0x80) ? 0x7 ^ (crc_u << 1) : (crc_u << 1);
  return (crc_u);
}

uint8_t get_crc8(uint8_t *Buf, uint8_t BufLen) {
  uint8_t crc = 0, i;
  for (i = 0; i < BufLen; i++) crc = update_crc8(Buf[i], crc);
  return (crc);
}
