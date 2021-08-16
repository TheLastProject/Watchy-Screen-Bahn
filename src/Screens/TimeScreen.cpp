#include "TimeScreen.h"

#include <stdlib.h>
#include <time.h>

#include "DIN_1451_Engschrift_Regular12pt7b.h"
#include "DIN_1451_Engschrift_Regular64pt7b.h"
#include "GetLocation.h"
#include "GetWeather.h"

using namespace Watchy;

void TimeScreen::show() {
  tm t;
  time_t tt = now();
  localtime_r(&tt, &t);

  Watchy::display.fillScreen(bgColor);

  int16_t  x1, y1, lasty;
  uint16_t w, h;
  String textstring;
  bool light = true;

  // ** DRAW **

  //drawbg
  Watchy::display.fillScreen(light ? GxEPD_WHITE : GxEPD_BLACK);
  //Watchy::display.fillRoundRect(2,2,196,196,8,light ? GxEPD_BLACK : GxEPD_WHITE);
  //Watchy::display.fillRoundRect(6,6,188,188,5,light ? GxEPD_WHITE : GxEPD_BLACK);

  Watchy::display.setFont(&DIN_1451_Engschrift_Regular64pt7b);
  Watchy::display.setTextColor(light ? GxEPD_BLACK : GxEPD_WHITE);
  Watchy::display.setTextWrap(false);

  //draw hours
  textstring = t.tm_hour;
  Watchy::display.getTextBounds(textstring, 0, 0, &x1, &y1, &w, &h);
  Watchy::display.setCursor(183-w, 100-5);
  Watchy::display.print(textstring);

  //draw minutes
  if (t.tm_min < 10) {
    textstring = "0";
  } else {
    textstring = "";
  }
  textstring += t.tm_min;
  Watchy::display.getTextBounds(textstring, 0, 0, &x1, &y1, &w, &h);
  Watchy::display.setCursor(183-w, 100+3+h);
  Watchy::display.print(textstring);

  // draw battery
  Watchy::display.fillRoundRect(16,16,34,12,4,light ? GxEPD_BLACK : GxEPD_WHITE);
  Watchy::display.fillRoundRect(49,20,3,4,2,light ? GxEPD_BLACK : GxEPD_WHITE);
  Watchy::display.fillRoundRect(18,18,30,8,3,light ? GxEPD_WHITE : GxEPD_BLACK);
  float batt = (getBatteryVoltage()-3.3)/0.9;
  if (batt > 0) {
    Watchy::display.fillRoundRect(20,20,26*batt,4,2,light ? GxEPD_BLACK : GxEPD_WHITE);
  }

  Watchy::display.setFont(&DIN_1451_Engschrift_Regular12pt7b);
  lasty = 200 - 16;

  //draw steps
  textstring = sensor.getCounter();
  textstring += " steps";
  Watchy::display.getTextBounds(textstring, 0, 0, &x1, &y1, &w, &h);
  Watchy::display.fillRoundRect(16,lasty-h-2,w + 7,h+4,2,light ? GxEPD_BLACK : GxEPD_WHITE);
  Watchy::display.setCursor(19, lasty-3);
  Watchy::display.setTextColor(light ? GxEPD_WHITE : GxEPD_BLACK);
  Watchy::display.print(textstring);
  Watchy::display.setTextColor(light ? GxEPD_BLACK : GxEPD_WHITE);
  lasty += -8-h;

  // draw year
  textstring = t.tm_year + 1900;
  Watchy::display.getTextBounds(textstring, 0, 0, &x1, &y1, &w, &h);
  Watchy::display.setCursor(16, lasty);
  Watchy::display.print(textstring);
  lasty += -20;

  // draw date
  textstring = monthShortStr(t.tm_mon);
  textstring += " ";
  textstring += t.tm_mday;
  Watchy::display.getTextBounds(textstring, 0, 0, &x1, &y1, &w, &h);
  Watchy::display.setCursor(16, lasty);
  Watchy::display.print(textstring);
  lasty += -20;

  // draw day
  textstring = dayStr(t.tm_wday);
  Watchy::display.getTextBounds(textstring, 0, 0, &x1, &y1, &w, &h);
  Watchy::display.setCursor(16, lasty);
  Watchy::display.print(textstring);
  lasty += -40;

  // weather things
  auto wd = Watchy_GetWeather::getWeather();

  // draw weather state
  if (wd.weatherConditionCode >= 801) {
    textstring = "Cloudy";
  } else if (wd.weatherConditionCode == 800) {
    textstring = "Clear";
  } else if (wd.weatherConditionCode == 781) {
    textstring = "Tornado";
  } else if (wd.weatherConditionCode == 771) {
    textstring = "Squall";
  } else if (wd.weatherConditionCode == 762) {
    textstring = "Ash";
  } else if (wd.weatherConditionCode == 761 || wd.weatherConditionCode == 731) {
    textstring = "Dust";
  } else if (wd.weatherConditionCode == 751) {
    textstring = "Sand";
  } else if (wd.weatherConditionCode == 741) {
    textstring = "Fog";
  } else if (wd.weatherConditionCode == 721) {
    textstring = "Haze";
  } else if (wd.weatherConditionCode == 711) {
    textstring = "Smoke";
  } else if (wd.weatherConditionCode == 701) {
    textstring = "Mist";
  } else if (wd.weatherConditionCode >= 600) {
    textstring = "Snow";
  } else if (wd.weatherConditionCode >= 500) {
    textstring = "Rain";
  } else if (wd.weatherConditionCode >= 300) {
    textstring = "Drizzle";
  } else if (wd.weatherConditionCode >= 200) {
    textstring = "Thunderstorm";
  } else {
    textstring = "";
  }
  display.getTextBounds(textstring, 0, 0, &x1, &y1, &w, &h);
  display.setCursor(16, lasty);
  display.print(textstring);
  lasty += -20;

  // draw temperature
  textstring = wd.temperature;
  textstring += strcmp(Watchy_GetWeather::TEMP_UNIT, "metric") == 0 ? "C" : "F";
  Watchy::display.getTextBounds(textstring, 0, 0, &x1, &y1, &w, &h);
  Watchy::display.setCursor(16, lasty);
  Watchy::display.print(textstring);
}
