# Porting a watchface from Watchy to Watchy-Screen

## Code changes

We'll use 7_SEG from the `Watchy/examples/WatchFaces` directory as an example. The basic idea is that the subclass of `Watchy` becomes a subclass of `Screen` the and `drawWatchFace()` method becomes the `show()` method.

First create a new platformio project with platform espressif32, board pico32, and framework arduino. If you're doing it in VS Code, go to platformio home, "+ New Project" pick a name, select "ESP32 Pico Kit (Espressif)" for the board. Default location is fine. Your `platformio.ini` should look like:

``` ini
; PlatformIO Project Configuration File
;
;   Build options: build flags, source filter
;   Upload options: custom upload port, speed and extra flags
;   Library options: dependencies, extra library storages
;   Advanced options: extra scripting
;
; Please visit documentation for the other options and examples
; https://docs.platformio.org/page/projectconf.html

[env:pico32]
platform = espressif32
board = pico32
framework = arduino
```

We want to maximize program space and don't use much SPIFFS so add

``` ini
board_build.partitions = min_spiffs.csv
```

I suggest updating the serial speeds for faster uploading and faster monitoring. Add the following two lines:

``` ini
monitor_speed = 115200
upload_speed = 921600
```

You'll need to add the libraries we depend on. You can use the PIO library manager, but Watchy-Screen is custom and you'll have to edit `platformio.ini` by hand to add it so I just add them all by adding

``` ini
lib_deps =
        adafruit/Adafruit GFX Library@^1.10.10
        arduino-libraries/Arduino_JSON@^0.1.0
        jchristensen/DS3232RTC@0.0.0-alpha+sha.261ca7d0e6
        zinggjm/GxEPD2@^1.3.4
        adafruit/Adafruit BusIO@^1.7.5
        https://github.com/charles-haynes/Watchy-Screen.git#v3.1.3
```


after `framework` but there may be newer versions since this was written, so check for updates.

Now copy your `.h` `.ino` `.c` and `.cpp` files to the `src` directory in your new project. In this case that means

* 7_SEG.ino
* DSEG7_Classic_Bold_25.h
* DSEG7_Classic_Regular_15.h
* DSEG7_Classic_Regular_39.h
* Seven_Segment10pt7b.h
* Watchy_7_SEG.cpp
* Watchy_7_SEG.h
* icons.h

### `main.cpp`

rename the `.ino` file to `main.cpp` (overwrite any existing `main.cpp`) In `main.cpp` add an import for watchy `#import <Watchy.h>` instantiate your screen (don't call it `watchy` though, I usually name it after the class in this case `watchy7SEG`), and change main instead of calling `watchy.init()` first set the current screen then call `Watchy::init()` (`Watchy::` not `watchy.` because Watchy is now a namespace.) So `main.cpp` should look like:

``` c++
#include <Watchy.h>

#include "Watchy_7_SEG.h"

Watchy7SEG watchy7SEG;

void setup() {
  // only set it if not already set so that changing
  // screens persists over deep sleep
  if (Watchy::screen == nullptr) {
    Watchy::screen = &watch7SEG;
  }
  Watchy::init();
}

void loop() {}
```

### `Watchy_7_SEG.h`

Instead a subclass of the `Watchy` class, it should be a subclass of `Screen`. Import `"Screen.h"` instead of `<Watchy.h>` and change `class Watchy7SEG : public Watchy{` to `class Watchy7SEG : public Screen {` (change `Watchy` to `Screen`) and change `void drawWatchFace();` to `void show() override;` (change your `drawWatchFace` method to a `show` method). That's it for the header file.

### `Watchy_7_SEG.cpp`

We need to copy `DSEG7_Classic_Bold_53.h` from Watchy into our project, since Watchy-Screen doesn't include it. Also add `#include "DSEG7_Classic_Bold_53.h"` to the includes.

Add a `using namespace Watchy;` right after the includes. This is bad style, but when porting will make things easier to get working. We'll fix it in a bit.

Change `drawWatchFace` to `show`

That only leaves weather. `Watchy-Screens` moves weather into its own file. So add `#include <GetWeather.h>` to your includes, add a `using namespace Watchy_GetWeather;` after your includes (we'll fix that too in a minute) and change `getWeatherData` to `getWeather` - Watchy Screen changed the name.

### That's it! Almost.

That's the minimal code changes. You should be able to build and upload the watch face, and it should display properly.
But `using namespace Blah;` is bad coding practice. So we will clean that up. Delete the line `using namespace Watchy_GetWeather;` and rebuild. You should see three complaints about names `not declared in this scope` add `Watchy_GetWeather::` to all of them. Next do the same thing with `using namespace Watchy;` This time there will be quite a few more errors. Most of them related to `display` and `currentTime` - add `Watchy::` to all of them.

Done.
