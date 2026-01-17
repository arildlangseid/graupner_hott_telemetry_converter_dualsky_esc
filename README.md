# graupner_hott_telemetry_converter_kiss_esc
Arduino converter DualSky/KISS Summit ESC to Graupner HoTT Telemetry

Using Arduino Leonardo (atmega32u) to convert KISS Telemetry from DualSky Summit 60 Slim ESC to Graupner HoTT

Telemetry-cable from Summit ESC is connected to HardwareSerial Rx (Serial1)
Graupner HoTT Receiver is connected to SoftwareSerial Rx pin D10. Tx pin D11 is connected to D10 via 2k resistor. See HoTTServer.h

Credits and big thanks goes to:
https://github.com/chriszero/ArduHottSensor
and
https://github.com/Made4RC/HoTTServer
