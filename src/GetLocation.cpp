#include "GetLocation.h"

#include <Arduino_JSON.h>
#include <IPAddress.h>

#include "Watchy.h"  // for connectWiFi

namespace Watchy_GetLocation {

constexpr const float DEFAULT_LOCATION_LATITUDE = -37.8136;
constexpr const float DEFAULT_LOCATION_LONGDITUDE = 144.9631;
const int LOCATION_UPDATE_INTERVAL = 5 * 60 * 1000;  // 5 minutes in millis


const char* TZDB_UDP_HOST = "timezoned.rop.nl";
const int TZDB_UDP_PORT = 2342;

RTC_DATA_ATTR time_t lastGetLocationTS;  // can use this to throttle
RTC_DATA_ATTR location currentLocation = {
    DEFAULT_LOCATION_LATITUDE,      // lat
    DEFAULT_LOCATION_LONGDITUDE,    // lon
    "AEST-10AEDT,M10.1.0,M4.1.0/3"  // timezone
};

// ropg maintains a UDP server that translates Olson Timzones to Posix
// https://github.com/ropg/ezTime#timezonedropnl
// alternatively we could maintain our own map, but it would be at least
// a few kb. Too large for something we need so rarely.
// this function, given a tz name in "Olson" format (like "Australia/Melbourne")
// returns the tz in "Posix" format (like "AEST-10AEDT,M10.1.0,M4.1.0/3"), which
// is what settz wants.
char *getPosixTZforOlson(const char *olson, char *buf, int bufLen) {
  WiFiUDP udp;
  udp.begin(TZDB_UDP_PORT);
  IPAddress remoteIP;
  if (!udp.beginPacket(TZDB_UDP_HOST, TZDB_UDP_PORT)) {
    return nullptr;
  }
  udp.write(reinterpret_cast<const uint8_t *>(olson), strlen(olson));
  udp.endPacket();
  auto startTS = millis();
  const unsigned long timeout = 5000;  // five seconds
  while (!udp.parsePacket()) {
    yield();
    if (millis() - startTS > timeout) {
      udp.stop();
      return nullptr;
    }
  }
  auto len = udp.read(buf, bufLen);
  udp.stop();
  // in the happy path the returned string is
  // "OK olsonTZstring posixTZstring"
  // for errors it is
  // "ERROR error message text"
  if (len <= 0 || len >= bufLen) {
    return nullptr;
  }
  buf[len] = '\0';
  if (!strncmp(buf, "ERROR ", 6)) {
    LOGE("%s", &buf[6]);
    return nullptr;
  }
  if (strncmp(buf, "OK ", 3)) {
    LOGE("unknown reply %s", buf);
    return nullptr;
  }
  const char *posix = strchr(&buf[3], ' ');
  if (!posix) {
    LOGE("missing TZ %s", posix);
    return nullptr;
  }
  posix++;
  return strcpy(buf, posix);  // possibly overlapping copy, but strcpy is smart.
}

const location *getLocation() {
  // http://ip-api.com/json?fields=57792
  // {"status":"success","lat":-27.4649,"lon":153.028,"timezone":"Australia/Brisbane","query":"202.144.174.72"}
  if (now() - lastGetLocationTS < LOCATION_UPDATE_INTERVAL) { // too soon
    return &currentLocation;
  }
  if (!Watchy::connectWiFi()) {
    LOGE("connectWiFi failed");
    return &currentLocation;
  }

  // WiFi is connected Use Weather API for live data
  HTTPClient http;
  http.setConnectTimeout(5000);  // 5 second max timeout
  const char *locationQueryURL = "http://ip-api.com/json?fields=57792";
  if (!http.begin(locationQueryURL)) {
    LOGE("http.begin failed");
  }
  int httpResponseCode = http.GET();
  if (httpResponseCode == 200) {
    String payload = http.getString();
    JSONVar responseObject = JSON.parse(payload);
    location loc;
    loc.lat = double(responseObject["lat"]);
    loc.lon = double(responseObject["lon"]);
    auto olsonTZ = (const char *)responseObject["timezone"];
    if (getPosixTZforOlson(olsonTZ, loc.timezone, sizeof(loc.timezone))) {
      currentLocation = loc;
      lastGetLocationTS = now();
    } else {
      LOGE("getPosixTZForOlson failed");
    }
  } else {
    LOGE("http error %d", httpResponseCode);
    // http error
  }
  http.end();
  // turn off radios
  WiFi.mode(WIFI_OFF);
  btStop();
  return &currentLocation;
}
}  // namespace Watchy_GetLocation
