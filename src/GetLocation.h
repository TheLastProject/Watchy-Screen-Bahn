#pragma once

#include <IPAddress.h>

namespace Watchy_GetLocation {

typedef struct {
  float lat;
  float lon;
  char timezone[128]; // POSIX tz spec https://developer.ibm.com/articles/au-aix-posix/
  char city[30]; // even if we get a longer name from the API, we'll truncate it to fit the sceen width
} location;

extern location currentLocation;

extern const location* getLocation();
}  // namespace Watchy_GetLocation
