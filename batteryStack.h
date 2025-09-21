#ifndef MAX_PYLON_BATTERIES_SUPPORTED
#define MAX_PYLON_BATTERIES_SUPPORTED 16
#endif

#ifndef BATTERYSTACK_H
#define BATTERYSTACK_H

// This struct represents a single Pylontech battery.
struct pylonBattery {
  bool isPresent;     // Indicates if the battery is present (not "Absent")
  long soc;           // State of Charge in %
  long voltage;       // Measured in mW
  long current;       // Measured in mA (negative if discharging)
  long tempr;         // Temperature in milli-degrees Celsius
  long cellTempLow;   // Lowest cell temperature (mC)
  long cellTempHigh;  // Highest cell temperature (mC)
  long cellVoltLow;   // Lowest cell voltage (mV)
  long cellVoltHigh;  // Highest cell voltage (mV)
  char baseState[9];  
  char voltageState[9];
  char currentState[9];
  char tempState[9];
  char time[20];
  char b_v_st[9];
  char b_t_st[9];

  bool isCharging()    const { return strcmp(baseState, "Charge")   == 0; }
  bool isDischarging() const { return strcmp(baseState, "Dischg")   == 0; }
  bool isIdle()        const { return strcmp(baseState, "Idle")     == 0; }
  bool isBalancing()   const { return strcmp(baseState, "Balance")  == 0; }

  // Determines whether the battery is in a "normal" state.
  bool isNormal() const
  {
    // If none of the basic states apply, it's not normal.
    if (!isCharging() && !isDischarging() && !isIdle() && !isBalancing()) {
      return false;
    }
    // Also check that voltage/current/temperature states are "Normal."
    return  strcmp(voltageState, "Normal") == 0 &&
            strcmp(currentState, "Normal") == 0 &&
            strcmp(tempState,    "Normal") == 0 &&
            strcmp(b_v_st,       "Normal") == 0 &&
            strcmp(b_t_st,       "Normal") == 0;
  }
};

// This struct represents a stack (group) of Pylontech batteries.
struct batteryStack {
  int batteryCount;   // Number of present batteries
  int soc;            // State of Charge in %
  int temp;           // Overall temperature in milli-degrees Celsius
  long currentDC;     // Measured current for the whole stack in mA
  long avgVoltage;    // Average voltage across batteries in mV
  char baseState[9];  // e.g., "Charge", "Dischg", "Idle", "Alarm!", etc.

   // Array de batería: reservado hasta el máximo soportado (16).
  pylonBattery batts[MAX_PYLON_BATTERIES_SUPPORTED];

  // Returns true if all present batteries are in "normal" state.
  bool isNormal() const
  {
    for(int ix = 0; ix < MAX_PYLON_BATTERIES_SUPPORTED; ix++)
    {
      if (batts[ix].isPresent && !batts[ix].isNormal()) {
        return false;
      }
    }
    return true;
  }

  // Calculates DC power in watts (approx) = (mA/1000) * (mV/1000)
  long getPowerDC() const
  {
    return (long)(((double)currentDC / 1000.0) * ((double)avgVoltage / 1000.0));
  }

  // Power in watts when charging (currentDC > 0).
  float powerIN() const
  {
    if (currentDC > 0) {
      return (float)(((double)currentDC / 1000.0) * ((double)avgVoltage / 1000.0));
    } else {
      return 0;
    }
  }
  
  // Power in watts when discharging (currentDC < 0).
  float powerOUT() const
  {
    if (currentDC < 0) {
      return (float)(-1.0 * ((double)currentDC / 1000.0) * ((double)avgVoltage / 1000.0));
    } else {
      return 0;
    }
  }

  // Estimated AC-side power, accounting for inverter losses.
  long getEstPowerAc() const
  {
    double powerDCf = (double)getPowerDC();
    if(powerDCf == 0)
    {
      return 0;
    }
    else if(powerDCf < 0)
    {
      // Discharging
      if(powerDCf < -1000)      return (long)(powerDCf * 0.94);
      else if(powerDCf < -600)  return (long)(powerDCf * 0.90);
      else                      return (long)(powerDCf * 0.87);
    }
    else
    {
      // Charging
      if(powerDCf > 1000)       return (long)(powerDCf * 1.06);
      else if(powerDCf > 600)   return (long)(powerDCf * 1.10);
      else                      return (long)(powerDCf * 1.13);
    }
    return 0;
  }
};

#endif // BATTERYSTACK_H
