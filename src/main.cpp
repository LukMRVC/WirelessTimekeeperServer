#include <Arduino.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>

#define RADIO_CHANNEL 16
#define DEVICE_ADDRESS 0x2F
#define DEST_DEVICE_ADDRESS 0x1F
#define FREQ 868.0

#define DEBOUNCE_DELAY 50

const short inputBtn1Pin = 4; // change!
const short inputBtn2Pin = 5;
const size_t inputBtn1Idx = 0;
const size_t inputBtn2Idx = 1;

// for buttons 1 and 2
short buttonState[2] = {HIGH, HIGH};
short lastButtonState[2] = {LOW, LOW};
unsigned long lastButtonChangeTime[2] = {0, 0};
unsigned long resultTime[2] = {0, 0};

// returns true if state changed
bool readButton(const short btnPin, const size_t btnIdx)
{
  const auto reading = digitalRead(btnPin);
  bool changed = false;
  if (reading != lastButtonState[btnIdx])
  {
    lastButtonChangeTime[btnIdx] = millis();
  }

  // check if the same reading time is bigger than the debounce delay
  if ((millis() - lastButtonChangeTime[btnIdx]) > DEBOUNCE_DELAY)
  {
    // if button state has changed
    if (reading != buttonState[btnIdx])
    {
      buttonState[btnIdx] = reading;
      changed = true;
    }
  }
  lastButtonState[btnIdx] = reading;
  return changed;
}

void setup()
{

  Serial.begin(9600);

  if (ELECHOUSE_cc1101.getCC1101())
  { // Check the CC1101 Spi connection.
    Serial.println(F("Connection OK"));
  }
  else
  {
    Serial.println(F("Connection Error"));
  }

  ELECHOUSE_cc1101.Init();           // must be set to initialize the cc1101!
  ELECHOUSE_cc1101.setCCMode(1);     // set config for internal transmission mode.
  ELECHOUSE_cc1101.setModulation(0); // set modulation mode. 0 = 2-FSK, 1 = GFSK, 2 = ASK/OOK, 3 = 4-FSK, 4 = MSK.
  ELECHOUSE_cc1101.setMHZ(FREQ);     // Here you can set your basic frequency. The lib calculates the frequency automatically (default = 433.92).The cc1101 can: 300-348 MHZ, 387-464MHZ and 779-928MHZ. Read More info from datasheet.
  ELECHOUSE_cc1101.setSyncMode(2);   // Combined sync-word qualifier mode. 0 = No preamble/sync. 1 = 16 sync word bits detected. 2 = 16/16 sync word bits detected. 3 = 30/32 sync word bits detected. 4 = No preamble/sync, carrier-sense above threshold. 5 = 15/16 + carrier-sense above threshold. 6 = 16/16 + carrier-sense above threshold. 7 = 30/32 + carrier-sense above threshold.
                                     // ELECHOUSE_cc1101.setPA(10);      // set TxPower. The following settings are possible depending on the frequency band.  (-30  -20  -15  -10  -6    0    5    7    10   11   12) Default is max!
  ELECHOUSE_cc1101.setCrc(1);        // 1 = CRC calculation in TX and CRC check in RX enabled. 0 = CRC disabled for TX and RX.
  ELECHOUSE_cc1101.setChannel(RADIO_CHANNEL);
  ELECHOUSE_cc1101.setAddr(DEVICE_ADDRESS);
  ELECHOUSE_cc1101.setAdrChk(1); // when addChk is enabled, every sent packet first byte must be the destination address
  Serial.println(F("Tx Mode"));
}

const short buffer_size = 32;
char transmission_buffer[buffer_size];
byte receiving_buffer[buffer_size];
int counter = 0;

unsigned long timers[2] = {0, 0};
unsigned long timer_stops[2] = {0, 0};

void loop()
{

  memset(transmission_buffer, 0, buffer_size);
  memset(receiving_buffer, 0, buffer_size);
  *transmission_buffer = DEST_DEVICE_ADDRESS;
  bool btnChangedState = readButton(inputBtn1Pin, inputBtn1Idx);

  if (btnChangedState && buttonState[inputBtn1Idx] == LOW)
  {
    sprintf(transmission_buffer + 1, "START");
    Serial.print(F("Sending radio data: "));
    Serial.println(transmission_buffer);
    ELECHOUSE_cc1101.SendData(transmission_buffer, buffer_size);
    if (timers[0] == 0 && timers[1] == 0)
    {
      Serial.println(F("Starting timer..."));
      timers[0] = millis();
      timers[1] = millis();
    }
  }

  if (timers[0] > 0 || timers[1] > 0)
  {
    if (ELECHOUSE_cc1101.CheckRxFifo(100))
    {
      if (ELECHOUSE_cc1101.CheckCRC())
      {
        int len = ELECHOUSE_cc1101.ReceiveData(receiving_buffer);
        receiving_buffer[len] = '\0';
        Serial.print(F("Received: "));
        Serial.println((char *)receiving_buffer + 1);
        auto cmpResult = strncmp((char *)receiving_buffer + 1, "END", 3);
        if (cmpResult == 0)
        {
          short timerIdx = -1;
          if ((char)receiving_buffer[4] == '1')
            timerIdx = 0;
          else if ((char)receiving_buffer[4] == '2')
            timerIdx = 1;

          if (timerIdx < 0)
          {
            Serial.println(F("Received END signal, but no timer INDEX"));
          }
          else
          {
            timer_stops[timerIdx] = millis();
            Serial.print(F("TIMER result: "));
            Serial.print(timer_stops[timerIdx] - timers[timerIdx]);
            Serial.println(" ms.");
            timer_stops[timerIdx] = 0;
          }
        }
      }
      else
      {
        Serial.println(F("Data received but CRC failed"));
      }
    }
  }

  /**
   * When stopwatching the time, the server time is always gonna be bigger than the end client,
   * So when a client is discovered, it sends back its millis() time.
   * Having our milis when discovering the client and its millis, I can calculate the difference in of the two.
   * Then, when stopwatching, the server remembers start time and the client will send the end time
   * we can simply substract those two and adjust it with the arduino_start_difference
   *
   * server_start_time = millis()
   * client_start_time = client_time()
   * arduino_start_difference = server_start_time - client_start_time
   *
   *
   * stopwatch_start = millis()
   *
   * target_1_time = get_target_time(1);
   * target_2_time = get_target_time(2);
   *
   * final_target_1_time = target_1_time - stopwatch_start - arduino_start_difference
   * final_target_2_time = target_2_time - stopwatch_start - arduino_start_difference
   */
}