#include "Watchy.h"

#include "Screen.h"
#include "GetLocation.h"  // bad dependency

using namespace Watchy;

void _rtcConfig(String datetime);
void _bmaConfig();
uint16_t _readRegister(uint8_t address, uint8_t reg, uint8_t *data,
                       uint16_t len);
uint16_t _writeRegister(uint8_t address, uint8_t reg, uint8_t *data,
                        uint16_t len);

DS3232RTC Watchy::RTC(false);
GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> Watchy::display(
    GxEPD2_154_D67(CS, DC, RESET, BUSY));
RTC_DATA_ATTR Screen *Watchy::screen = nullptr;

RTC_DATA_ATTR BMA423 Watchy::sensor;
RTC_DATA_ATTR bool Watchy::WIFI_CONFIGURED;
RTC_DATA_ATTR bool Watchy::BLE_CONFIGURED;

String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

tmElements_t Watchy::currentTime;

void Watchy::init(String datetime) {
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();  // get wake up reason
  Wire.begin(SDA, SCL);                          // init i2c

  // sync ESP32 clocks to RTC
  if (Watchy::RTC.read(currentTime) == 0) {
    setenv("TZ", Watchy_GetLocation::currentLocation.timezone, 1);
    tzset();
    time_t t = makeTime(currentTime);
    setTime(t);
    timeval tv = {t, 0};
    settimeofday(&tv, nullptr);
  }

  switch (wakeup_reason) {
#ifdef ESP_RTC
    case ESP_SLEEP_WAKEUP_TIMER:  // ESP Internal RTC
      tmElements_t Watchy::currentTime;
      RTC.read(currentTime);
      currentTime.Minute++;
      tmElements_t tm;
      tm.Month = currentTime.Month;
      tm.Day = currentTime.Day;
      tm.Year = currentTime.Year;
      tm.Hour = currentTime.Hour;
      tm.Minute = currentTime.Minute;
      tm.Second = 0;
      time_t t = makeTime(tm);
      RTC.set(t);
      showWatchFace(true);  // partial updates on tick
      break;
#endif
    case ESP_SLEEP_WAKEUP_EXT0:  // RTC Alarm
      RTC.alarm(ALARM_2);        // resets the alarm flag in the RTC
      showWatchFace(true);       // partial updates on tick
      break;
    case ESP_SLEEP_WAKEUP_EXT1:  // button Press
      handleButtonPress();
      break;
    default:  // reset
#ifndef ESP_RTC
      _rtcConfig(datetime);
#endif
      _bmaConfig();
      showWatchFace(false);  // full update on reset
      break;
  }
  deepSleep();
}

void Watchy::deepSleep() {
  display.hibernate();
#ifndef ESP_RTC
  esp_sleep_enable_ext0_wakeup(RTC_PIN,
                               0);  // enable deep sleep wake on RTC interrupt
#endif
#ifdef ESP_RTC
  esp_sleep_enable_timer_wakeup(60000000);
#endif
  esp_sleep_enable_ext1_wakeup(
      BTN_PIN_MASK,
      ESP_EXT1_WAKEUP_ANY_HIGH);  // enable deep sleep wake on button press
  esp_deep_sleep_start();
}

void _rtcConfig(String datetime) {
  if (datetime != NULL) {
    const time_t FUDGE(
        30);  // fudge factor to allow for upload time, etc. (seconds, YMMV)
    tmElements_t tm;
    tm.Year = getValue(datetime, ':', 0).toInt() -
              YEAR_OFFSET;  // offset from 1970, since year is stored in uint8_t
    tm.Month = getValue(datetime, ':', 1).toInt();
    tm.Day = getValue(datetime, ':', 2).toInt();
    tm.Hour = getValue(datetime, ':', 3).toInt();
    tm.Minute = getValue(datetime, ':', 4).toInt();
    tm.Second = getValue(datetime, ':', 5).toInt();

    time_t t = makeTime(tm) + FUDGE;
    RTC.set(t);
  }
  // https://github.com/JChristensen/DS3232RTC
  RTC.squareWave(SQWAVE_NONE);  // disable square wave output
  RTC.setAlarm(ALM2_EVERY_MINUTE, 0, 0, 0,
               0);                    // alarm wakes up Watchy every minute
  RTC.alarmInterrupt(ALARM_2, true);  // enable alarm interrupt
}

void debounce(uint8_t pin, int state) {
  // wait for changed state + 40ms
  // or 400ms
  unsigned long timeout = millis() + 400;
  while (millis() < timeout) {
    if (digitalRead(pin) != state) {
      delay(min(timeout - millis(), 40ul));
      break;
    }
    yield();
  }
}

bool Watchy::pollButtonsAndDispatch()  // returns true if button was pressed
{
  if (digitalRead(MENU_BTN_PIN) == 1) {
    debounce(MENU_BTN_PIN, 1);
    screen->menu();
    return true;
  }
  if (digitalRead(BACK_BTN_PIN) == 1) {
    debounce(BACK_BTN_PIN, 1);
    screen->back();
    return true;
  }
  if (digitalRead(UP_BTN_PIN) == 1) {
    debounce(UP_BTN_PIN, 1);
    screen->up();
    return true;
  }
  if (digitalRead(DOWN_BTN_PIN) == 1) {
    debounce(DOWN_BTN_PIN, 1);
    screen->down();
    return true;
  }
  return false;
}

void fastEventLoop() {
  // TODO need a way for handlers to say they're done with the ui
  const int timeout = 5000;
  long timeoutMillis = millis() + timeout;
  pinMode(MENU_BTN_PIN, INPUT);
  pinMode(BACK_BTN_PIN, INPUT);
  pinMode(UP_BTN_PIN, INPUT);
  pinMode(DOWN_BTN_PIN, INPUT);
  while (millis() < timeoutMillis) {
    if (pollButtonsAndDispatch()) {
      timeoutMillis = millis() + timeout;
    }
    yield();
  }
}

void Watchy::handleButtonPress() {
  uint64_t wakeupBit = esp_sleep_get_ext1_wakeup_status();
  switch (wakeupBit & BTN_PIN_MASK) {
    case MENU_BTN_MASK: screen->menu(); break;
    case BACK_BTN_MASK: screen->back(); break;
    case UP_BTN_MASK:   screen->up();   break;
    case DOWN_BTN_MASK: screen->down(); break;
    default:                            break;
  }
  fastEventLoop();
}

void Watchy::showWatchFace(bool partialRefresh, Screen *s) {
  display.init(
      0, false);  //_initial_refresh to false to prevent full update on init
  display.setFullWindow();
  display.setTextColor((s->bgColor == GxEPD_WHITE ? GxEPD_BLACK : GxEPD_WHITE));
  display.setCursor(0,0);
  s->show();
  display.display(partialRefresh);  // partial refresh
}

// setScreen is used to set a new screen on the display
void Watchy::setScreen(Screen *s) {
  if (s == nullptr) {
    return;
  }
  screen = s;
  showWatchFace(true);
}

float Watchy::getBatteryVoltage() {
  return analogRead(ADC_PIN) / 4096.0 * 7.23;
}

uint16_t _readRegister(uint8_t address, uint8_t reg, uint8_t *data,
                       uint16_t len) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)address, (uint8_t)len);
  uint8_t i = 0;
  while (Wire.available()) {
    data[i++] = Wire.read();
  }
  return 0;
}

uint16_t _writeRegister(uint8_t address, uint8_t reg, uint8_t *data,
                        uint16_t len) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(data, len);
  return (0 != Wire.endTransmission());
}

void _bmaConfig() {
  if (sensor.begin(_readRegister, _writeRegister, delay) == false) {
    // fail to init BMA
    return;
  }

  // Accel parameter structure
  Acfg cfg;
  /*!
      Output data rate in Hz, Optional parameters:
          - BMA4_OUTPUT_DATA_RATE_0_78HZ
          - BMA4_OUTPUT_DATA_RATE_1_56HZ
          - BMA4_OUTPUT_DATA_RATE_3_12HZ
          - BMA4_OUTPUT_DATA_RATE_6_25HZ
          - BMA4_OUTPUT_DATA_RATE_12_5HZ
          - BMA4_OUTPUT_DATA_RATE_25HZ
          - BMA4_OUTPUT_DATA_RATE_50HZ
          - BMA4_OUTPUT_DATA_RATE_100HZ
          - BMA4_OUTPUT_DATA_RATE_200HZ
          - BMA4_OUTPUT_DATA_RATE_400HZ
          - BMA4_OUTPUT_DATA_RATE_800HZ
          - BMA4_OUTPUT_DATA_RATE_1600HZ
  */
  cfg.odr = BMA4_OUTPUT_DATA_RATE_100HZ;
  /*!
      G-range, Optional parameters:
          - BMA4_ACCEL_RANGE_2G
          - BMA4_ACCEL_RANGE_4G
          - BMA4_ACCEL_RANGE_8G
          - BMA4_ACCEL_RANGE_16G
  */
  cfg.range = BMA4_ACCEL_RANGE_2G;
  /*!
      Bandwidth parameter, determines filter configuration, Optional parameters:
          - BMA4_ACCEL_OSR4_AVG1
          - BMA4_ACCEL_OSR2_AVG2
          - BMA4_ACCEL_NORMAL_AVG4
          - BMA4_ACCEL_CIC_AVG8
          - BMA4_ACCEL_RES_AVG16
          - BMA4_ACCEL_RES_AVG32
          - BMA4_ACCEL_RES_AVG64
          - BMA4_ACCEL_RES_AVG128
  */
  cfg.bandwidth = BMA4_ACCEL_NORMAL_AVG4;

  /*! Filter performance mode , Optional parameters:
      - BMA4_CIC_AVG_MODE
      - BMA4_CONTINUOUS_MODE
  */
  cfg.perf_mode = BMA4_CONTINUOUS_MODE;

  // Configure the BMA423 accelerometer
  sensor.setAccelConfig(cfg);

  // Enable BMA423 accelerometer
  // Warning : Need to use feature, you must first enable the accelerometer
  // Warning : Need to use feature, you must first enable the accelerometer
  sensor.enableAccel();

  struct bma4_int_pin_config config;
  config.edge_ctrl = BMA4_LEVEL_TRIGGER;
  config.lvl = BMA4_ACTIVE_HIGH;
  config.od = BMA4_PUSH_PULL;
  config.output_en = BMA4_OUTPUT_ENABLE;
  config.input_en = BMA4_INPUT_DISABLE;
  // The correct trigger interrupt needs to be configured as needed
  sensor.setINTPinConfig(config, BMA4_INTR1_MAP);

  struct bma423_axes_remap remap_data;
  remap_data.x_axis = 1;
  remap_data.x_axis_sign = 0xFF;
  remap_data.y_axis = 0;
  remap_data.y_axis_sign = 0xFF;
  remap_data.z_axis = 2;
  remap_data.z_axis_sign = 0xFF;
  // Need to raise the wrist function, need to set the correct axis
  sensor.setRemapAxes(&remap_data);

  // Enable BMA423 isStepCounter feature
  sensor.enableFeature(BMA423_STEP_CNTR, true);
  // Enable BMA423 isTilt feature
  sensor.enableFeature(BMA423_TILT, true);
  // Enable BMA423 isDoubleClick feature
  sensor.enableFeature(BMA423_WAKEUP, true);

  // Reset steps
  sensor.resetStepCounter();

  // Turn on feature interrupt
  sensor.enableStepCountInterrupt();
  sensor.enableTiltInterrupt();
  // It corresponds to isDoubleClick interrupt
  sensor.enableWakeupInterrupt();
}

bool Watchy::connectWiFi() {
#if !defined(WIFI_SSID) || !defined(WIFI_PASSWORD)
  if (WL_CONNECT_FAILED == WiFi.begin()) {
    // WiFi not setup, you can also use hard coded credentials with
    // WiFi.begin(SSID,PASS); by defining WIFI_SSID and WIFI_PASSWORD
#else
  if (WL_CONNECT_FAILED == WiFi.begin() && WL_CONNECT_FAILED == WiFi.begin(WIFI_SSID,WIFI_PASSWORD)) {
    // WiFi not setup
#endif
    WIFI_CONFIGURED = false;
  } else {
    if (WL_CONNECTED ==
        WiFi.waitForConnectResult()) {  // attempt to connect for 10s
      WIFI_CONFIGURED = true;
    } else {  // connection failed, time out
      WIFI_CONFIGURED = false;
      // turn off radios
      WiFi.mode(WIFI_OFF);
      btStop();
    }
  }
  return WIFI_CONFIGURED;
}