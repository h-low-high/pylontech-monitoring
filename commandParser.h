#ifndef COMMANDPARSER_H
#define COMMANDPARSER_H

#include "batteryStack.h"

// Reduced timeout from 1000ms to 300ms
void requestBatteryData(batteryStack &battStack) {
  Serial2.print("^P003FDC\r");
  Serial.println(">> Enviado: ^P003FDC");

  unsigned long start = millis();
  String response = "";
  unsigned long timeout = 300; // Reduced from 1000ms to 300ms

  // Early exit when response is complete
  bool responseComplete = false;
  
  while (millis() - start < timeout && !responseComplete) {
    if (Serial2.available()) {
      while (Serial2.available()) {
        char c = Serial2.read();
        response += c;
        
        // Check if we have a complete response
        if (response.length() > 10 && c == '\r') {
          responseComplete = true;
          break;
        }
      }
    }
    // Small yield to allow ESP32 background tasks to run
    yield();
  }

  if (response.startsWith("^D")) {
    Serial.println("<< Respuesta recibida:");
    Serial.println(response);

    if (response.length() > 70) {
      int ix = 0;
      pylonBattery &bat = battStack.batts[ix];
      bat.isPresent = true;

      bat.voltage = response.substring(52, 57).toInt();
      bat.current = response.substring(57, 62).toInt();
      bat.soc     = response.substring(62, 65).toInt();
      strncpy(bat.baseState, "Idle", sizeof(bat.baseState));

      battStack.avgVoltage = bat.voltage;
      battStack.currentDC  = bat.current;
      battStack.soc        = bat.soc;

      Serial.print("SOC: "); Serial.println(battStack.soc);
      Serial.print("Voltaje: "); Serial.println(battStack.avgVoltage);
      Serial.print("Corriente: "); Serial.println(battStack.currentDC);
    }
  } else {
    Serial.println("⚠ No se recibió respuesta válida del BMS.");
  }
}

#endif // COMMANDPARSER_H