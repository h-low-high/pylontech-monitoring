#ifndef MAX_PYLON_BATTERIES_SUPPORTED
#define MAX_PYLON_BATTERIES_SUPPORTED 16
#endif

#ifndef BATTERYSTACK_H
#define BATTERYSTACK_H

// Maximum history entries (72 hours * 4 entries per hour = 288)
#define MAX_BALANCE_HISTORY_ENTRIES 288

// Structure to store balance history entry
struct balanceHistoryEntry
{
  unsigned long timestamp; // Unix timestamp
  uint8_t batteryId;       // Battery ID (1-16)
  int16_t balanceMv;       // Balance difference in mV
  uint8_t socPercent;      // State of charge in %
  bool isValid;            // Entry validity flag
};

// Structure to manage daily balance history
struct balanceHistory
{
  balanceHistoryEntry entries[MAX_BALANCE_HISTORY_ENTRIES];
  uint8_t currentIndex;       // Current write position (circular buffer)
  uint8_t entryCount;         // Total valid entries
  unsigned long lastSaveTime; // Last save timestamp

  // Initialize the history buffer
  void init()
  {
    currentIndex = 0;
    entryCount = 0;
    lastSaveTime = 0;
    for (int i = 0; i < MAX_BALANCE_HISTORY_ENTRIES; i++)
    {
      entries[i].isValid = false;
    }
  }

  // Add new entry to history
  void addEntry(uint8_t batteryId, int16_t balanceMv, uint8_t socPercent, unsigned long timestamp)
  {
    entries[currentIndex].timestamp = timestamp;
    entries[currentIndex].batteryId = batteryId;
    entries[currentIndex].balanceMv = balanceMv;
    entries[currentIndex].socPercent = socPercent;
    entries[currentIndex].isValid = true;

    currentIndex = (currentIndex + 1) % MAX_BALANCE_HISTORY_ENTRIES;
    if (entryCount < MAX_BALANCE_HISTORY_ENTRIES)
    {
      entryCount++;
    }
  }

  // Get entry by index (0 = oldest, entryCount-1 = newest)
  balanceHistoryEntry *getEntry(uint8_t index)
  {
    if (index >= entryCount)
      return nullptr;

    uint8_t actualIndex;
    if (entryCount < MAX_BALANCE_HISTORY_ENTRIES)
    {
      actualIndex = index;
    }
    else
    {
      actualIndex = (currentIndex + index) % MAX_BALANCE_HISTORY_ENTRIES;
    }

    return entries[actualIndex].isValid ? &entries[actualIndex] : nullptr;
  }
};

// This struct represents a single Pylontech battery.
struct pylonBattery
{
  bool isPresent;    // Indicates if the battery is present (not "Absent")
  long soc;          // State of Charge in %
  long voltage;      // Measured in mW
  long current;      // Measured in mA (negative if discharging)
  long tempr;        // Temperature in milli-degrees Celsius
  long cellTempLow;  // Lowest cell temperature (mC)
  long cellTempHigh; // Highest cell temperature (mC)
  long cellVoltLow;  // Lowest cell voltage (mV)
  long cellVoltHigh; // Highest cell voltage (mV)
  char baseState[9];
  char voltageState[9];
  char currentState[9];
  char tempState[9];
  char time[20];
  char b_v_st[9];
  char b_t_st[9];

  bool isCharging() const { return strcmp(baseState, "Charge") == 0; }
  bool isDischarging() const { return strcmp(baseState, "Dischg") == 0; }
  bool isIdle() const { return strcmp(baseState, "Idle") == 0; }
  bool isBalancing() const { return strcmp(baseState, "Balance") == 0; }

  // Determines whether the battery is in a "normal" state.
  bool isNormal() const
  {
    // If none of the basic states apply, it's not normal.
    if (!isCharging() && !isDischarging() && !isIdle() && !isBalancing())
    {
      return false;
    }
    // Also check that voltage/current/temperature states are "Normal."
    return strcmp(voltageState, "Normal") == 0 &&
           strcmp(currentState, "Normal") == 0 &&
           strcmp(tempState, "Normal") == 0 &&
           strcmp(b_v_st, "Normal") == 0 &&
           strcmp(b_t_st, "Normal") == 0;
  }
};

// This struct represents a stack (group) of Pylontech batteries.
struct batteryStack
{
  int batteryCount;  // Number of present batteries
  int soc;           // State of Charge in %
  int temp;          // Overall temperature in milli-degrees Celsius
  long currentDC;    // Measured current for the whole stack in mA
  long avgVoltage;   // Average voltage across batteries in mV
  char baseState[9]; // e.g., "Charge", "Dischg", "Idle", "Alarm!", etc.

  // Array de batería: reservado hasta el máximo soportado (16).
  pylonBattery batts[MAX_PYLON_BATTERIES_SUPPORTED];

  // Balance history tracking
  balanceHistory history;

  // Initialize the battery stack including history
  void init()
  {
    history.init();
  }

  // Record balance data for all batteries
  void recordBalanceHistory(unsigned long currentTime)
  {
    for (int i = 0; i < MAX_PYLON_BATTERIES_SUPPORTED; i++)
    {
      if (batts[i].isPresent)
      {
        int16_t balanceMv = (int16_t)(batts[i].cellVoltHigh - batts[i].cellVoltLow);
        uint8_t socPercent = (uint8_t)batts[i].soc;
        history.addEntry(i + 1, balanceMv, socPercent, currentTime);
      }
    }
  }

  // Check if it's time to record (every 15 minutes)
  bool shouldRecordHistory(unsigned long currentTime)
  {
    const unsigned long RECORD_INTERVAL = 15 * 60 * 1000; // 15 minutes for production

    // Force first record after 30 seconds of startup
    if (history.lastSaveTime == 0 && currentTime > 30000)
    {
      return true;
    }

    return (currentTime - history.lastSaveTime) >= RECORD_INTERVAL;
  }

  // Update last save time
  void updateLastSaveTime(unsigned long currentTime)
  {
    history.lastSaveTime = currentTime;
  }

  // Save balance history to LittleFS
  bool saveBalanceHistory()
  {
#ifdef ESP8266
    if (!LittleFS.begin())
    {
      return false;
    }

    File file = LittleFS.open("/balance_history.dat", "w");
    if (!file)
    {
      return false;
    }

    // Write header with metadata
    file.write((uint8_t *)&history.currentIndex, sizeof(history.currentIndex));
    file.write((uint8_t *)&history.entryCount, sizeof(history.entryCount));
    file.write((uint8_t *)&history.lastSaveTime, sizeof(history.lastSaveTime));

    // Write all entries
    for (int i = 0; i < MAX_BALANCE_HISTORY_ENTRIES; i++)
    {
      file.write((uint8_t *)&history.entries[i], sizeof(balanceHistoryEntry));
    }

    file.close();
    return true;
#else
    return false; // Not implemented for ESP32 yet
#endif
  }

  // Load balance history from LittleFS
  bool loadBalanceHistory()
  {
#ifdef ESP8266
    if (!LittleFS.begin())
    {
      return false;
    }

    if (!LittleFS.exists("/balance_history.dat"))
    {
      // No history file exists, initialize empty
      history.init();
      return true;
    }

    File file = LittleFS.open("/balance_history.dat", "r");
    if (!file)
    {
      return false;
    }

    // Read header with metadata
    file.read((uint8_t *)&history.currentIndex, sizeof(history.currentIndex));
    file.read((uint8_t *)&history.entryCount, sizeof(history.entryCount));
    file.read((uint8_t *)&history.lastSaveTime, sizeof(history.lastSaveTime));

    // Read all entries
    for (int i = 0; i < MAX_BALANCE_HISTORY_ENTRIES; i++)
    {
      file.read((uint8_t *)&history.entries[i], sizeof(balanceHistoryEntry));
    }

    file.close();
    return true;
#else
    history.init();
    return false; // Not implemented for ESP32 yet
#endif
  }

  // Returns true if all present batteries are in "normal" state.
  bool isNormal() const
  {
    for (int ix = 0; ix < MAX_PYLON_BATTERIES_SUPPORTED; ix++)
    {
      if (batts[ix].isPresent && !batts[ix].isNormal())
      {
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
    if (currentDC > 0)
    {
      return (float)(((double)currentDC / 1000.0) * ((double)avgVoltage / 1000.0));
    }
    else
    {
      return 0;
    }
  }

  // Power in watts when discharging (currentDC < 0).
  float powerOUT() const
  {
    if (currentDC < 0)
    {
      return (float)(-1.0 * ((double)currentDC / 1000.0) * ((double)avgVoltage / 1000.0));
    }
    else
    {
      return 0;
    }
  }

  // Estimated AC-side power, accounting for inverter losses.
  long getEstPowerAc() const
  {
    double powerDCf = (double)getPowerDC();
    if (powerDCf == 0)
    {
      return 0;
    }
    else if (powerDCf < 0)
    {
      // Discharging
      if (powerDCf < -1000)
        return (long)(powerDCf * 0.94);
      else if (powerDCf < -600)
        return (long)(powerDCf * 0.90);
      else
        return (long)(powerDCf * 0.87);
    }
    else
    {
      // Charging
      if (powerDCf > 1000)
        return (long)(powerDCf * 1.06);
      else if (powerDCf > 600)
        return (long)(powerDCf * 1.10);
      else
        return (long)(powerDCf * 1.13);
    }
    return 0;
  }
};

#endif // BATTERYSTACK_H
