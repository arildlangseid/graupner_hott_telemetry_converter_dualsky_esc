#include <Arduino.h>
#include "HoTTServer.h"

HoTTServer::HoTTServer(uint8_t rxPin, uint8_t txPin)
  : _serialPort(rxPin, txPin) {
	// define pin modes for tx, rx:
	pinMode(rxPin, INPUT);
	pinMode(txPin, OUTPUT);

	setWarning(HOTT_ALARM_NONE);

	setupEEPROM();
}

void HoTTServer::registerModule(uint8_t module) {
	bitSet(_registeredModules, module);
}

void HoTTServer::start() {
	digitalWrite(LED_BUILTIN, HIGH);
	_serialPort.begin(19200);
	while (_serialPort.available())
		digitalWrite(LED_BUILTIN, LOW);
	_serialPort.read();
}

/*
*     setupEEPROM()
*/
void HoTTServer::setupEEPROM(void) {
  //read the stored counter & eepromdata
  eeprom_read_block((void*)&sSettings, (void*)0, sizeof(sSettings));
  // if serial number is not set
  if (sSettings.serial != SERIAL_NUMBER) {
    // set default values in case eeprom is empty/garbage as in first try
		sSettings.numPoles=14;
		sSettings.battAlarmV=9.6;
    sSettings.serial=SERIAL_NUMBER;
  }
}

void HoTTServer::_sendData(uint8_t* data, uint8_t size) {
	// Checksum berechnen bei Binary Mode
	//if (size == 45)
	{
		uint8_t sum = 0;
		for (int i = 0; i < size - 1; i++) {
			sum += data[i];
		}
		data[size - 1] = sum;
	}

	// Daten senden
	for (int j = 0; j < size; j++) {
		_serialPort.write(data[j]);
		delayMicroseconds(HOTT_TX_DELAY);
	}
	while (_serialPort.available())
		_serialPort.read();
}

bool HoTTServer::_isModuleRegistered(uint8_t module) {
	if (bitRead(_registeredModules, module) == 0)
		return 0;
	else
		return 1;
}

void HoTTServer::clearLine(int line) {
	memset(&hottASCII.text[line * 21], ' ', 21);
}

void HoTTServer::clearAll() {
	memset(&hottASCII.text[0], ' ', 21 * 8);
}

/*
 * Prepares text for sending it to the HoTT display
 * returns the position of the last char printed
 */
int HoTTServer::hottPrint(int line, int col, char* text, boolean inv = false) {
	int i = 0;
	while (text[i] != '\0' && i < 21 - col) {
		hottASCII.text[(line * 21) + col + i] = inv ? text[i] | 0x80 : text[i];
		i++;
	}
	return i + col;
}

/* character set normal (inverted)
 32	(160)	: _!"#$%&´()*+,-./
 48	(176)	: 0123456789
 58	(186)	: :;<=>?@
 65	(193)	: ABCDEFGHIJKLMNOPQRSTUVWXYZ
 91	(220)	: [\]^_(degree)
 97	(225)	: abcdefghijklmnopqrstuvwxyz
123	(251)	:	arrows: left right up down
*/

void HoTTServer::hottBuildAscii(byte button) {
	uint8_t key = (uint8_t)(button & 0x0F);  // get pressed button

	hottASCII.startByte = 0x7B;  // 0x7B
	//hottASCII.escape = 0xE0;     // 0xE0 D=GAM, E=EAM, A=GPS, 9=Vario
	hottASCII.escape = HOTT_AIRESC_SENSOR_ID;
	hottASCII.alarm = 0;

	// generate Textmenu
	clearAll();

	hottASCII.endByte = 0x7D;  // 0x7D

	// menu-indicator top-right
	hottPrint(0, 19, "<");
	// version
	hottPrint(0, 14, "V0.02");
	// header
	hottPrint(0, 0, "DUALSKYSUMMIT");
	// menu
	hottPrint(2, 1, "NUM MOTOR POLES");
	hottPrint(3, 1, "ALARM VOLT");
	hottPrint(3, 20, "V");
	hottPrint(4, 1,	"-not implemented yet");
	// credits :-)
	hottPrint(7, 1, "(C) ARILD LANGSEID");

	int buttonFeedBackLine = 5;

	switch (key) {
		case 0x07:
			// ESC
			if (numPolesEdit) {
				numPolesEdit = false;
				sSettings.numPoles = numPolesBackupValue;
			}
			if (battAlarmEdit) {
				battAlarmEdit = false;
				sSettings.battAlarmV = battAlarmBackupValue;
			}
			//hottPrint(buttonFeedBackLine, 10, "ESC");
			hottASCII.escape = 0x01;
			break;
		case 0x0D:
			// INC
			if (numPolesEdit && sSettings.numPoles < 40) {
				sSettings.numPoles += 2;
			} else if (battAlarmEdit && sSettings.battAlarmV < 25.2) {
				sSettings.battAlarmV += 0.1;
			} else if (curserPos < 3) curserPos++;
			hottPrint(buttonFeedBackLine, 10, "INC");
			break;
		case 0x0B:
			// DEC
			if (numPolesEdit && sSettings.numPoles > 2) {
				sSettings.numPoles -= 2;
			} else if (battAlarmEdit && sSettings.battAlarmV > 3.0) {
				sSettings.battAlarmV -= 0.1;
			} else if (curserPos > 2) curserPos--;
			hottPrint(buttonFeedBackLine, 10, "DEC");
			break;
		case 0x0E:
			// ENTER
			hottPrint(buttonFeedBackLine, 10, "ENTER");
			break;
		case 0x09:
			// SET
			if (numPolesEdit) {
				numPolesEdit = false;
				eeprom_write_block((const void*)&sSettings, (void*)0, sizeof(sSettings));
				break;
			}
			if (battAlarmEdit) {
				battAlarmEdit = false;
				eeprom_write_block((const void*)&sSettings, (void*)0, sizeof(sSettings));
				break;
			}
			if (curserPos == 2) {
				numPolesEdit = true;
				numPolesBackupValue = sSettings.numPoles;
			}
			if (curserPos == 3) {
				battAlarmEdit = true;
				battAlarmBackupValue = sSettings.battAlarmV;
			}

			hottPrint(buttonFeedBackLine, 10, "SET");

			break;
		case 0x0F:
			break;
		default:
			// NONE
			hottPrint(buttonFeedBackLine, 10, "Unhandeled");
			break;
	}

	char buffer[8];

	// print number of motor poles
	float numPolesFloat = sSettings.numPoles;
	dtostrf(numPolesFloat, 2, 0, buffer);
	hottPrint(2, 19, buffer, numPolesEdit);

	// print Voltage alarm level
	dtostrf(sSettings.battAlarmV, 4, 1, buffer);
	hottPrint(3, 16, buffer, battAlarmEdit);

	hottPrint(curserPos, 0, ">");
}

bool HoTTServer::processRequest() {
	bool requestProcessed = false;
	if (_serialPort.available() >= 2) {
		uint8_t requestMode = _serialPort.read();
		switch (requestMode) {
			case HOTT_TEXT_MODE_REQUEST_ID:
				{
					requestProcessed = true;
					delayMicroseconds(HOTT_SEND_DELAY);
					uint8_t requestID = _serialPort.read();


					if ((requestID & 0xf0) == (HOTT_TEXT_MODE_REQUEST_ELECTRIC_AIR & 0xf0) && _isModuleRegistered(HoTTServerEAM)) {
						//textData[45] = 'E'; textData[46] = 'A'; textData[47] = 'M';
						//_sendData(textData, 173);
					} else if ((requestID & 0xf0) == (HOTT_TEXT_MODE_REQUEST_GENERAL_AIR & 0xf0) && _isModuleRegistered(HoTTServerGAM)) {
						//textData[45] = 'G'; textData[46] = 'A'; textData[47] = 'M';
						//_sendData(textData, 173);
					} else if ((requestID & 0xf0) == (HOTT_TEXT_MODE_REQUEST_VARIO & 0xf0) && _isModuleRegistered(HoTTServerVario)) {
						//textData[45] = 'V'; textData[46] = 'a'; textData[47] = 'r'; textData[48] = 'i'; textData[49] = 'o';
						//_sendData(textData, 173);
					} else if ((requestID & 0xf0) == (HOTT_TEXT_MODE_REQUEST_GPS & 0xf0) && _isModuleRegistered(HoTTServerGPS)) {
						//textData[45] = 'G'; textData[46] = 'P'; textData[47] = 'S';
						//_sendData(textData, 173);
					} else if ((requestID & 0xf0) == (HOTT_TEXT_MODE_REQUEST_AIRESC & 0xf0) && _isModuleRegistered(HoTTServerESC)) {
						//textData[45] = 'A'; textData[46] = 'i'; textData[47] = 'r'; textData[48] = 'E'; textData[49] = 'S'; textData[50] = 'C';

						// Textmeldung Zeile 1
						/*
					for (uint8_t i = 0; i < 21; i++) {
						textData[3+i] = _message[i];
					}
					*/

						hottBuildAscii(requestID & 0x0f);

						_sendData(&hottASCII.startByte, 173);
					}

					//textData[2] = _warningID;
				}
				break;
			case HOTT_BINARY_MODE_REQUEST_ID:
				{
					requestProcessed = true;
					delayMicroseconds(HOTT_SEND_DELAY);
					uint8_t requestID = _serialPort.read();
					uint8_t telemetryData[] = {
						0x7C, /*  0	Start Byte */
						0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						0x7D, /* 43	End sign */
						0x00  /* 44	Checksum */
					};
					uint8_t OFFSET_STEIGRATE = 117;
					int steigrate_meter = 0;
					float rest = 0;
					int OFFSET_DEZIMETER = 40;
					int steigrate_dezimeter = 0;

					if (requestID == HOTT_ELECTRIC_AIR_MODULE_ID && _isModuleRegistered(HoTTServerEAM)) {
						/*
 0
 1
 2							Alarm
 3
 4, 5						Inverted Value 1 and 2
 6,  7,  8,  9, 10, 11, 12	Cell Voltage 1-7, LSB, in [2mV]/step
13, 14, 15, 16, 17, 18, 19	Cell Voltage 1-7, MSB, in [2mV]/step
20, 21						Battetry 1, LSB/MSB, in [100mV]/step, 50 == 5V
22, 23						Battetry 2, LSB/MSB, in [100mV]/step, 50 == 5V
24							Temp 1, Offset -20, 20 == 0, in [°C]
25							Temp 2, Offset -20, 20 == 0, in [°C]
26, 27						Altitude, Offset -500, 500 == 0, in [m]
28, 29						Current, LSB/MSB in [100mA]/step, 1 = 0.1A
30, 31						Drive Voltage, LSB/MSB
32, 33						Capacity, LSB/MSB, in [mAh]
34, 35						FIXME climbrate ? m2s
36							FIXME climbrate ? m3s
37, 38						RPM, LSB/MSB, [10rev]/step, 300 == 3000rpm
39							Electric minutes
40							Electric seconds
41							Speed, in [km/h]
42							Version Number
43							End sign
44							
 */

						telemetryData[1] = HOTT_ELECTRIC_AIR_MODULE_ID;

						// Warnings
						telemetryData[2] = _warningID;

						telemetryData[3] = HOTT_ELECTRIC_AIR_SENSOR_ID;

						// show inverted
						telemetryData[4] = _inverted1;
						telemetryData[5] = _inverted1;

						// cell voltages lsb
						telemetryData[6] = lowByte((uint16_t)(_cellVoltage1 * 100 / 2));
						telemetryData[7] = lowByte((uint16_t)(_cellVoltage2 * 100 / 2));
						telemetryData[8] = lowByte((uint16_t)(_cellVoltage3 * 100 / 2));
						telemetryData[9] = lowByte((uint16_t)(_cellVoltage4 * 100 / 2));
						telemetryData[10] = lowByte((uint16_t)(_cellVoltage5 * 100 / 2));
						telemetryData[11] = lowByte((uint16_t)(_cellVoltage6 * 100 / 2));
						telemetryData[12] = lowByte((uint16_t)(_cellVoltage7 * 100 / 2));
						// cell voltages msb
						telemetryData[13] = highByte((uint16_t)(_cellVoltage1 * 100 / 2));
						telemetryData[14] = highByte((uint16_t)(_cellVoltage2 * 100 / 2));
						telemetryData[15] = highByte((uint16_t)(_cellVoltage3 * 100 / 2));
						telemetryData[16] = highByte((uint16_t)(_cellVoltage4 * 100 / 2));
						telemetryData[17] = highByte((uint16_t)(_cellVoltage5 * 100 / 2));
						telemetryData[18] = highByte((uint16_t)(_cellVoltage6 * 100 / 2));
						telemetryData[19] = highByte((uint16_t)(_cellVoltage7 * 100 / 2));

						// battery voltages
						telemetryData[20] = lowByte((uint16_t)(_voltage1 * 10));
						telemetryData[21] = highByte((uint16_t)(_voltage1 * 10));
						telemetryData[22] = lowByte((uint16_t)(_voltage2 * 10));
						telemetryData[23] = highByte((uint16_t)(_voltage2 * 10));

						// temperatures
						telemetryData[24] = 20 + _temperature1;
						telemetryData[25] = 20 + _temperature2;

						// altitude
						telemetryData[26] = lowByte(_altitude + 500);
						telemetryData[27] = highByte(_altitude + 500);

						// current
						telemetryData[28] = lowByte((uint16_t)(_current * 10));
						telemetryData[29] = highByte((uint16_t)(_current * 10));

						// main voltage
						telemetryData[30] = lowByte((uint16_t)(_mainVoltage * 10));
						telemetryData[31] = highByte((uint16_t)(_mainVoltage * 10));

						// capacity
						telemetryData[32] = lowByte((uint16_t)(_capacity / 10));
						telemetryData[33] = highByte((uint16_t)(_capacity / 10));

						_sendData(telemetryData, 45);
					} else if (requestID == HOTT_GENERAL_AIR_MODULE_ID && _isModuleRegistered(HoTTServerGAM)) {
						/*
 0
 1
 2							Alarm
 3
 4,  5						Inverted Value 1 and 2
 6,  7,  8,  9, 10, 11		Voltage Cell 1-6, in [2mV]/step
12, 13						Battetry 1, LSB/MSB, in [100mV]/step, 50 == 5V
14, 15						Battetry 2, LSB/MSB, in [100mV]/step, 50 == 5V
16							Temp 1, Offset -20, in [°C]
17							Temp 2, Offset -20, in [°C]
18							Fuel percentage
19, 20						Fuel, LSB/MSB, in  [mL] (milliliter)
21, 22						RPM, LSB/MSB, [10rev]/step, 300 == 3000rpm
23, 24						Altitude, LSB/MSB, Offset -500, 500 = 0m
25, 26						Climb rate, FIXME, in m/1s
27							Climb rate, FIXME, in m/3s
28, 29						Current, LSB/MSB in 100mA steps, 1 = 0.1A
30, 31						Main voltage, LSB/MSB
32, 33						Capacity, LSB/MSB, in [mAh]
34, 35						Speed, in [km/h]
36							min. cell voltage
37							Cell number with min. voltage
38, 39						RPM2, LSB/MSB, [10rev]/step, 300 == 3000rpm
40							General error number
41							Pressure
42							Version Number
43							End sign
44							Checksum
*/

						telemetryData[1] = HOTT_GENERAL_AIR_MODULE_ID;

						// Warnings
						telemetryData[2] = _warningID;

						telemetryData[3] = HOTT_GENERAL_AIR_SENSOR_ID;

						// show inverted
						telemetryData[4] = _inverted1;
						telemetryData[5] = _inverted1;

						// cell voltages
						telemetryData[6] = _cellVoltage1 * 100 / 2;
						telemetryData[7] = _cellVoltage2 * 100 / 2;
						telemetryData[8] = _cellVoltage3 * 100 / 2;
						telemetryData[9] = _cellVoltage4 * 100 / 2;
						telemetryData[10] = _cellVoltage5 * 100 / 2;
						telemetryData[11] = _cellVoltage6 * 100 / 2;

						// battery voltages
						telemetryData[12] = lowByte((uint16_t)(_voltage1 * 10));
						telemetryData[13] = highByte((uint16_t)(_voltage1 * 10));
						telemetryData[14] = lowByte((uint16_t)(_voltage2 * 10));
						telemetryData[15] = highByte((uint16_t)(_voltage2 * 10));

						// temperatures
						telemetryData[16] = 20 + _temperature1;
						telemetryData[17] = 20 + _temperature2;

						// fuel percentage
						telemetryData[18] = _fuelPercentage;

						// fuel
						telemetryData[19] = lowByte(_fuel);
						telemetryData[20] = highByte(_fuel);

						// RPM1
						telemetryData[21] = lowByte((uint16_t)(_rpm1 / 10));
						telemetryData[22] = highByte((uint16_t)(_rpm1 / 10));

						// altitude
						telemetryData[23] = lowByte(_altitude + 500);
						telemetryData[24] = highByte(_altitude + 500);

						// climb rate m/s
						//höherwertiges Byte zum Bezugswert 117 (ca. 0 Meter) berechnen
						OFFSET_STEIGRATE = 117;
						steigrate_meter = _climbRate1s / 2.560 + OFFSET_STEIGRATE;
						//Rest berechnen, um das niederwertige Byte zu bestimmen
						rest = _climbRate1s - (2.560 * (steigrate_meter - OFFSET_STEIGRATE));
						//Bezugswert ist 40, denn Byte24=117 + Byte23=40 entsprechen 0.0 Metern)
						OFFSET_DEZIMETER = 40;
						if (_climbRate1s > 0) OFFSET_DEZIMETER = 50;
						steigrate_dezimeter = OFFSET_DEZIMETER + rest * 100;
						//da beim oben stehenden Algorithmus (echte "int"-Werte!)
						//im niederwertigen Byte23 auch Werte über 255 auftreten könnten,
						//muss hier geprüft werden
						if (steigrate_dezimeter > 0xff) {
							steigrate_dezimeter = steigrate_dezimeter - 0xff;
							steigrate_meter = steigrate_meter + 1;
						}
						telemetryData[25] = steigrate_dezimeter;
						telemetryData[26] = steigrate_meter;

						// climb rate m/3s
						telemetryData[27] = 120 + (uint8_t)_climbRate3s;

						// current
						telemetryData[28] = lowByte((uint16_t)(_current * 10));
						telemetryData[29] = highByte((uint16_t)(_current * 10));

						// main voltage
						telemetryData[30] = lowByte((uint16_t)(_mainVoltage * 10));
						telemetryData[31] = highByte((uint16_t)(_mainVoltage * 10));

						// capacity
						telemetryData[32] = lowByte((uint16_t)(_capacity / 10));
						telemetryData[33] = highByte((uint16_t)(_capacity / 10));

						// speed
						telemetryData[34] = lowByte(_speed);
						telemetryData[35] = highByte(_speed);

						// min. cell voltage and id
						telemetryData[36] = _minCellVoltage * 100 / 2;
						telemetryData[37] = _minCellVoltageCell;

						// RPM2
						telemetryData[38] = lowByte((uint16_t)(_rpm2 / 10));
						telemetryData[39] = highByte((uint16_t)(_rpm2 / 10));

						// pressure
						telemetryData[41] = _pressure * 10;

						_sendData(telemetryData, 45);
					} else if (requestID == HOTT_VARIO_MODULE_ID && _isModuleRegistered(HoTTServerVario)) {
						/*
 0 							Start Byte
 1							Module ID 
 2							Alarm
 3							Sensor ID
 4							Inverted Value 1
 5,  6  						Altitude, Offset -500, 500 == 0, in [m]
 7,  8						Max. altitude, Offset -500, 500 == 0, in [m]
 9, 10						Min. altitude, Offset -500, 500 == 0, in [m]
11, 12						Climb rate, in [m/s]
13, 14						Climb rate, in [m/3s]
15, 16						Climb rate, in [m/10s]
17, 18, 19, 20, 21, 22, 23	ASCII
24, 25, 26, 27, 28, 29, 30	ASCII
31, 32, 33, 34, 35, 36, 37	ASCII
38, 39, 40, 41				free
42							Version Number 
43							End sign
44							Checksum
*/
						telemetryData[1] = HOTT_VARIO_MODULE_ID;

						// Warnings
						telemetryData[2] = _warningID;

						telemetryData[3] = HOTT_VARIO_SENSOR_ID;

						// show inverted
						telemetryData[4] = _inverted1;

						// altitude
						telemetryData[5] = lowByte(_altitude + 500);
						telemetryData[6] = highByte(_altitude + 500);

						// max. altitude
						telemetryData[7] = lowByte(_max_altitude + 500);
						telemetryData[8] = highByte(_max_altitude + 500);

						// min. altitude
						telemetryData[9] = lowByte(_min_altitude + 500);
						telemetryData[10] = highByte(_min_altitude + 500);

						// climb m/1s
						//höherwertiges Byte zum Bezugswert 117 (ca. 0 Meter) berechnen
						OFFSET_STEIGRATE = 117;
						steigrate_meter = _climbRate1s / 2.560 + OFFSET_STEIGRATE;
						//Rest berechnen, um das niederwertige Byte zu bestimmen
						rest = _climbRate1s - (2.560 * (steigrate_meter - OFFSET_STEIGRATE));
						//Bezugswert ist 40, denn Byte24=117 + Byte23=40 entsprechen 0.0 Metern)
						OFFSET_DEZIMETER = 40;
						if (_climbRate1s > 0) OFFSET_DEZIMETER = 50;
						steigrate_dezimeter = OFFSET_DEZIMETER + rest * 100;
						//da beim oben stehenden Algorithmus (echte "int"-Werte!)
						//im niederwertigen Byte23 auch Werte über 255 auftreten könnten,
						//muss hier geprüft werden
						if (steigrate_dezimeter > 0xff) {
							steigrate_dezimeter = steigrate_dezimeter - 0xff;
							steigrate_meter = steigrate_meter + 1;
						}
						telemetryData[11] = steigrate_dezimeter;
						telemetryData[12] = steigrate_meter;

						// climb m/3s
						//höherwertiges Byte zum Bezugswert 117 (ca. 0 Meter) berechnen
						steigrate_meter = _climbRate3s / 2.560 + OFFSET_STEIGRATE;
						//Rest berechnen, um das niederwertige Byte zu bestimmen
						rest = _climbRate3s - (2.560 * (steigrate_meter - OFFSET_STEIGRATE));
						//Bezugswert ist 40, denn Byte24=117 + Byte23=40 entsprechen 0.0 Metern)
						OFFSET_DEZIMETER = 40;
						if (_climbRate3s > 0) OFFSET_DEZIMETER = 50;
						steigrate_dezimeter = OFFSET_DEZIMETER + rest * 100;
						//da beim oben stehenden Algorithmus (echte "int"-Werte!)
						//im niederwertigen Byte23 auch Werte über 255 auftreten könnten,
						//muss hier geprüft werden
						if (steigrate_dezimeter > 0xff) {
							steigrate_dezimeter = steigrate_dezimeter - 0xff;
							steigrate_meter = steigrate_meter + 1;
						}
						telemetryData[13] = steigrate_dezimeter;
						telemetryData[14] = steigrate_meter;

						// climb m/10s
						//höherwertiges Byte zum Bezugswert 117 (ca. 0 Meter) berechnen
						steigrate_meter = _climbRate10s / 2.560 + OFFSET_STEIGRATE;
						//Rest berechnen, um das niederwertige Byte zu bestimmen
						rest = _climbRate10s - (2.560 * (steigrate_meter - OFFSET_STEIGRATE));
						//Bezugswert ist 40, denn Byte24=117 + Byte23=40 entsprechen 0.0 Metern)
						OFFSET_DEZIMETER = 40;
						if (_climbRate10s > 0) OFFSET_DEZIMETER = 50;
						steigrate_dezimeter = OFFSET_DEZIMETER + rest * 100;
						//da beim oben stehenden Algorithmus (echte "int"-Werte!)
						//im niederwertigen Byte23 auch Werte über 255 auftreten könnten,
						//muss hier geprüft werden
						if (steigrate_dezimeter > 0xff) {
							steigrate_dezimeter = steigrate_dezimeter - 0xff;
							steigrate_meter = steigrate_meter + 1;
						}
						telemetryData[15] = steigrate_dezimeter;
						telemetryData[16] = steigrate_meter;

						// Textmeldung
						for (uint8_t i = 0; i < 21; i++) {
							telemetryData[17 + i] = _message[i];
						}

						_sendData(telemetryData, 45);
					} else if (requestID == HOTT_GPS_MODULE_ID && _isModuleRegistered(HoTTServerGPS)) {
						/*
 0					Start sign
 1					Module ID
 2					Warning ID / Alarm
 3 					Sensor ID
 4,  5				Inverted Value 1 and 2
 6 					Flight direction 
 7,  8				Ground speed, LSB/MSB
 9, 10, 11, 12, 13	Latitude
14, 15, 16, 17, 18	Longitude
19, 20				Distance, LSB/MSB, in [m]
21, 22				Altitude, Offset -500, 500 == 0, in [m]
23, 24				Climb rate, in [m/s], 1 = 0.01m/s
25					Climb rate, in [m/3s], 120 = 0
26					Number of satelites
27					GPS fix character
28					Home direction
29					angle x-direction
30					angle y-direction
31					angle z-direction
32, 33				gyro x
34, 35				gyro y
36, 37				gyro z
38					Vibrations
39					ASCII Free Character 4
40					ASCII Free Character 5
41					ASCII Free Character 6
42					Version Number
43					End sign
44					Checksum 
*/
						telemetryData[1] = HOTT_GPS_MODULE_ID;

						// Warnings
						telemetryData[2] = _warningID;

						telemetryData[3] = HOTT_GPS_SENSOR_ID;

						// show inverted
						telemetryData[4] = _inverted1;
						telemetryData[5] = _inverted1;

						// Flight direction
						telemetryData[6] = _flightDirection / 2;

						// speed
						telemetryData[7] = lowByte(_speed);
						telemetryData[8] = highByte(_speed);

						// distance
						telemetryData[19] = lowByte(_distance);
						telemetryData[20] = highByte(_distance);

						// altitude
						telemetryData[21] = lowByte(_altitude + 500);
						telemetryData[22] = highByte(_altitude + 500);

						// climb rate
						telemetryData[23] = 0x78;
						telemetryData[24] = 0x00;

						_sendData(telemetryData, 45);
					} else if (requestID == HOTT_AIRESC_MODULE_ID && _isModuleRegistered(HoTTServerESC)) {
						/*
 0		Start byte
 1		Module ID
 2		Alarm
 3		Sensor ID
 4,  5	Inverted Value 1 and 2
 6,  7	Input voltage, LSB/MSB, in [100mV]/step, 50 == 5V
 8,  9	min. input voltage, LSB/MSB, in [100mV]/step, 50 == 5V
10, 11	Capacity, LSB/MSB, in [mAh]
12		ESC temperature, in [°C]
13		ESC max. temperature, in [°C]
14, 15	Current, LSB/MSB in [100mA]/step, 1 = 0.1A
16, 17	max. current, LSB/MSB in [100mA]/step, 1 = 0.1A
18, 19	RPM, LSB/MSB, [10rev]/step, 300 == 3000rpm
20, 21	max. RPM, LSB/MSB, [10rev]/step, 300 == 3000rpm
22		Throttle, in [%]
23, 24	Speed, in [km/h]
25, 26	max. Speed, in [km/h]
27		BEC voltage
28		BEC min. voltage
29		BEC current
30, 31	BEC max. current
32		PWM
33		BEC temperature
34		BEC max. temperature
35		motor temperature
36		motor max. temperature
37, 38	Motor RPM, LSB/MSB, [10rev]/step, 300 == 3000rpm
39		motor timing
40		motor timing advanced
41		motor highest current
42		Version Number
43		End sign
44		Checksum
*/

						telemetryData[1] = HOTT_AIRESC_MODULE_ID;

						// Warnings
						telemetryData[2] = _warningID;

						telemetryData[3] = HOTT_AIRESC_SENSOR_ID;

						// show inverted
						telemetryData[4] = _inverted1;
						telemetryData[5] = _inverted1;

						// main voltage
						telemetryData[6] = lowByte((uint16_t)(_mainVoltage * 10));
						telemetryData[7] = highByte((uint16_t)(_mainVoltage * 10));

						// min. main voltage
						telemetryData[8] = lowByte((uint16_t)(_minMainVoltage * 10));
						telemetryData[9] = highByte((uint16_t)(_minMainVoltage * 10));

						// capacity
						telemetryData[10] = lowByte((uint16_t)(_capacity / 10));
						telemetryData[11] = highByte((uint16_t)(_capacity / 10));

						telemetryData[12] = _ESCTemperature;
						telemetryData[13] = _maxESCTemperature;

						// current
						telemetryData[14] = lowByte((uint16_t)(_current * 10));
						telemetryData[15] = highByte((uint16_t)(_current * 10));

						// max. current
						telemetryData[16] = lowByte((uint16_t)(_maxCurrent * 10));
						telemetryData[17] = highByte((uint16_t)(_maxCurrent * 10));

						// RPM1
						telemetryData[18] = lowByte((uint16_t)(_rpm1 / 10));
						telemetryData[19] = highByte((uint16_t)(_rpm1 / 10));

						telemetryData[20] = _throttle;

						// speed
						telemetryData[21] = lowByte(_speed);
						telemetryData[22] = highByte(_speed);

						// max speed
						telemetryData[23] = lowByte(_maxSpeed);
						telemetryData[24] = highByte(_maxSpeed);

						telemetryData[25] = (uint8_t)(_BECVoltage * 10);
						telemetryData[26] = (uint8_t)(_minBECVoltage * 10);

						// BEC current
						telemetryData[27] = _BECCurrent * 10;
						telemetryData[28] = lowByte((uint16_t)(_maxBECCurrent * 10));
						telemetryData[29] = highByte((uint16_t)(_maxBECCurrent * 10));

						telemetryData[30] = _PWM;

						telemetryData[30] = _BECTemperature;
						telemetryData[31] = _maxBECTemperature;

						telemetryData[32] = _motorTemperature;
						telemetryData[33] = _maxMotorTemperature;

						// Motor RPM
						telemetryData[34] = lowByte((uint16_t)(_motorRpm / 10));
						telemetryData[35] = highByte((uint16_t)(_motorRpm / 10));

						telemetryData[36] = _motorTiming;
						telemetryData[37] = _motorAdvancedTiming;

						_sendData(telemetryData, 45);
					}
				}
				break;
		}
	}
	return requestProcessed;
}

void HoTTServer::setWarning(HOTTAlarm_e warningID) {
	_warningID = warningID;
}
void HoTTServer::setInverted(uint8_t invertedID, uint8_t inverted) {
	invertedID = constrain(invertedID, 0, 1);

	switch (invertedID) {
		case HOTT_PRIMARY_INVERTED:
			_inverted1 = inverted;
			break;
		case HOTT_SECONDARY_INVERTED:
			_inverted2 = inverted;
			break;
	}
}

void HoTTServer::setCapacity(uint16_t capacity) {
	_capacity = capacity;
}
void HoTTServer::setCurrent(HOTTCurrent_e sensorID, float current) {
	switch (sensorID) {
		case HOTT_MAIN_CURRENT:
			current = constrain(current, 0.0, 999.9);

			_current = current;
			_maxCurrent = max(_maxCurrent, current);
			break;
		case HOTT_BEC_CURRENT:
			current = constrain(current, 0.0, 25.5);

			_BECCurrent = current;
			_maxBECCurrent = max(_maxBECCurrent, current);
			break;
		case HOTT_MAX_MOTOR_CURRENT:
			current = constrain(current, 0.0, 25.5);

			_maxMotorCurrent = (uint8_t)current;
			break;
	}
}
void HoTTServer::setVoltage(HOTTVoltage_e sensorID, float voltage) {
	switch (sensorID) {
		case HOTT_PRIMARY_VOLTAGE:
			_voltage1 = voltage;
			break;
		case HOTT_SECONDARY_VOLTAGE:
			_voltage2 = voltage;
			break;
		case HOTT_MAIN_VOLTAGE:
			_mainVoltage = voltage;
			_minMainVoltage = min(_minMainVoltage, voltage);
			break;
		case HOTT_BEC_VOLTAGE:
			_BECVoltage = voltage;
			_minBECVoltage = min(_minMainVoltage, voltage);
			break;
	}
}
void HoTTServer::setCellVoltage(uint8_t cellNumber, float voltage) {
	voltage = constrain(voltage, 0.0, 5.1);

	switch (cellNumber) {
		case 1:
			_cellVoltage1 = voltage;
			break;
		case 2:
			_cellVoltage2 = voltage;
			break;
		case 3:
			_cellVoltage3 = voltage;
			break;
		case 4:
			_cellVoltage4 = voltage;
			break;
		case 5:
			_cellVoltage5 = voltage;
			break;
		case 6:
			_cellVoltage6 = voltage;
			break;
		case 7:
			_cellVoltage7 = voltage;
			break;
	}

	if (_minCellVoltage == 0.0) {
		_minCellVoltage = voltage;
		_minCellVoltageCell = cellNumber;
	}
	if (voltage < _minCellVoltage) {
		_minCellVoltage = voltage;
		_minCellVoltageCell = cellNumber;
	}
}
void HoTTServer::setAltitude(int16_t altitude) {
	_altitude = constrain(altitude, -500, 9999);

	_min_altitude = min(_altitude, _min_altitude);
	_max_altitude = max(_altitude, _max_altitude);
}
void HoTTServer::setClimbRate(HOTTClimbRate_e climbRateID, float climbRate) {
	climbRate = constrain(climbRate, -296.9, 327.5);

	switch (climbRateID) {
		case HOTT_CLIMBRATE_1S:
			_climbRate1s = climbRate;
			break;
		case HOTT_CLIMBRATE_3S:
			_climbRate3s = climbRate;
			break;
		case HOTT_CLIMBRATE_10S:
			_climbRate10s = climbRate;
			break;
	}
}
void HoTTServer::setFuelPercentage(uint8_t percent) {
	percent = constrain(percent, 0, 100);

	_fuelPercentage = percent;
}
void HoTTServer::setFuel(uint16_t fuel) {
	_fuel = fuel;
}
void HoTTServer::setTemperature(HOTTTemperature_e temperatureID, int8_t temperature) {
	temperature = constrain(temperature, -20, 235);

	switch (temperatureID) {
		case HOTT_PRIMARY_TEMPERATURE:
			_temperature1 = temperature;
			break;
		case HOTT_SECONDARY_TEMPERATURE:
			_temperature2 = temperature;
			break;
		case HOTT_ESC_TEMPERATURE:
			_ESCTemperature = temperature;
			_maxESCTemperature = max(_maxESCTemperature, temperature);
			break;
		case HOTT_BEC_TEMPERATURE:
			_BECTemperature = temperature;
			_maxBECTemperature = max(_maxBECTemperature, temperature);
			break;
		case HOTT_MOTOR_TEMPERATURE:
			_motorTemperature = temperature;
			_maxMotorTemperature = max(_maxMotorTemperature, temperature);
			break;
	}
}
void HoTTServer::setRPM(HOTTRPM_e rpmID, uint16_t rpm) {
	rpm = constrain(rpm * 100 / (sSettings.numPoles/2), 0, 65535);

	switch (rpmID) {
		case HOTT_PRIMARY_RPM:
			_rpm1 = rpm;
			_maxRpm1 = max(_maxRpm1, rpm);
			break;
		case HOTT_SECONDARY_RPM:
			_rpm2 = rpm;
			_maxRpm2 = max(_maxRpm2, rpm);
			break;
		case HOTT_MOTOR_RPM:
			_motorRpm = rpm;
			break;
	}
}
void HoTTServer::setSpeed(uint16_t speed) {
	_speed = speed;
	_maxSpeed = max(_maxSpeed, speed);
}
void HoTTServer::setPressure(float pressure) {
	pressure = constrain(pressure, 0, 25.5);

	_pressure = pressure;
}
void HoTTServer::setDistance(uint16_t distance) {
	_distance = distance;
}
void HoTTServer::setFlightDirection(uint8_t direction) {
	_flightDirection = constrain(_flightDirection, 0, 360);

	_flightDirection = direction;
}
void HoTTServer::setMessage(char message[], uint8_t length) {
	length = constrain(length, 0, 21);

	for (uint8_t i = 0; i < length; i++) {
		_message[i] = message[i];
	}
	for (uint8_t i = length; i < 21; i++) {
		_message[i] = 0;
	}
}
void HoTTServer::setThrottle(uint8_t throttle) {
	throttle = constrain(throttle, 0, 100);

	_throttle = throttle;
}
void HoTTServer::setPWM(uint8_t PWM) {
	_PWM = PWM;
}
void HoTTServer::setMotorTimimg(uint8_t timing) {
	_motorTiming = timing;
}
void HoTTServer::setMotorAdvancedTimimg(uint8_t timing) {
	_motorAdvancedTiming = timing;
}
void HoTTServer::setAngle(HOTTAxis_e axisID, uint8_t angle) {
	angle = constrain(angle, 0, 360);

	switch (axisID) {
		case HOTT_X_AXIS:
			_angleX = angle;
			break;
		case HOTT_Y_AXIS:
			_angleY = angle;
			break;
		case HOTT_Z_AXIS:
			_angleZ = angle;
			break;
	}
}
void HoTTServer::setGyro(HOTTAxis_e axisID, uint8_t gyro) {
	gyro = constrain(gyro, 0, 360);

	switch (axisID) {
		case HOTT_X_AXIS:
			_gyroX = gyro;
			break;
		case HOTT_Y_AXIS:
			_gyroY = gyro;
			break;
		case HOTT_Z_AXIS:
			_gyroZ = gyro;
			break;
	}
}
