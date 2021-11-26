#define ONEWIRE_SEARCH 0

#include <TinyGPS++.h>
#include <SoftwareSerial.h>

#include <SPI.h>
#include <epd2in9_V2.h>
#include <epdif.h>
//
#include <epdpaint.h>
#include <fonts.h>
#include <imagedata.h>

#include <maidenhead.h>

#include <DS18B20.h>


#define COLORED     0
#define UNCOLORED   1

#define BLINK_DELAY 100
#define GPS_RX_PIN 2
#define GPS_TX_PIN 3
#define ONEWIRE_PIN 6
#define LED_PIN 5
#define MAX_TEMP_TRIES 100


// Create a TinyGPS++ object
TinyGPSPlus gps;

// Create a software serial port called "gpsSerial"
SoftwareSerial gpsSerial(GPS_RX_PIN, GPS_TX_PIN);

// temperature sensor
DS18B20 ds(ONEWIRE_PIN);

// GPS fields.
unsigned long lastGpsRead = 0;
unsigned long lastPaint = 0;
unsigned long lastFix = 0;
char loc[20] = "00.0000N, 000.0000W\0";
char alt[20] = "+000000 ft, 00 sats        \0";
char time[22] = "HH:MM:SS UTC         \0";
char date[18] = "00/00/0000       \0";
char NO_READING[11] = "NO READING\0";
char NO_GPS[7] = "NO GPS\0";
char STALE[6] = "STALE\0";
float curTemp = 0.0;

// epaper stuff
boolean recentGpsData = false;
unsigned char image[1024];
Paint paint(image, 0, 0);    // width should be the multiple of 8 
Epd display;
boolean displayOk = true;


// blinks for a few ms.
void blink(unsigned long millis) {
  boolean on = true;
  for (int i = 0; i < millis; i+= BLINK_DELAY) {
    digitalWrite(LED_PIN, on);
    safeDelay(BLINK_DELAY);
    on = !on;
  }
  digitalWrite(LED_PIN, LOW);
}

// consumes GPS for maximum of ms.
// may return earlier.
void pollGps(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start && gpsSerial.available()) {
    if (gps.encode(gpsSerial.read())) {
      recentGpsData = true;
      lastGpsRead = millis();
    }
  }
}

// tries to get the temperature.
// be warned: this guy takes about 1 second to run.
float pollTemp() {
  curTemp = 0.0; // small race here.
  for (int i = 0; i < MAX_TEMP_TRIES; i++) {
    if (ds.selectNext()) {
      curTemp = ds.getTempF();
      break;
    }
  }
  if (curTemp == 0.0) {
    Serial.println(F("Failed temp"));
  }
}

// busy loop that aggressively consumes gps data for a period of time.
// similar to pollGps(), but will never return sooner than ms.
void safeDelay(unsigned long ms) {
  unsigned long start = millis();
  do {
    pollGps(ms - (millis() - start));
  } while (millis() - start < ms);
}

void setup()
{
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  Serial.begin(9600);
  analogReference(EXTERNAL);
  gpsSerial.begin(9600);

  // initialize the display.
  if (display.Init() != 0) {
    displayOk = false;
  } else {
    updateFields();
    display.ClearFrameMemory(0xFF);
    display.DisplayFrame();
    repaintScreen();
  }
}

void loop()
{
  // populate gps stuff.
  safeDelay(1000);

  // populate temperature.
  // this call usually takes about 1s, so it has the effect of making the LED blink for
  // about a second.
  digitalWrite(LED_PIN, true);
  pollTemp();
  digitalWrite(LED_PIN, false);

  // maybe repaint.
  // give visual indication if it's stale.
  if (millis() - lastPaint > 10000) {
    // one last chance to get some gps data.
    if (!recentGpsData) {
      safeDelay(1000);
    }
    
    // if we still don't have data, give a visual indication.
    // we've lost our fix.
    if (!recentGpsData) {
      blink(1500);
    }
    
    // if we have gps data, update fields and toggle the recency flag
    // otherwise, indicate in the alt field.
    if (recentGpsData) {
      updateFields();
      recentGpsData = false;
    } else {
      strcpy(alt, STALE);
    }
    repaintScreen();
    lastPaint = millis();
  }
}

void repaintScreen() {
  if (displayOk) {
    paint.SetRotate(ROTATE_90);
    paint.SetWidth(24);
    paint.SetHeight(295);
    
    paint.Clear(UNCOLORED);
    paint.DrawStringAt(0, 0, loc, &Font20, COLORED);
    display.SetFrameMemory(paint.GetImage(), 100, 0, paint.GetWidth(), paint.GetHeight());

    paint.Clear(UNCOLORED);
    paint.DrawStringAt(0, 0, time, &Font24, COLORED);
    display.SetFrameMemory(paint.GetImage(), 68, 0, paint.GetWidth(), paint.GetHeight());

    paint.Clear(UNCOLORED);
    paint.DrawStringAt(0, 0, date, &Font20, COLORED);
    display.SetFrameMemory(paint.GetImage(), 34, 0, paint.GetWidth(), paint.GetHeight());

    paint.Clear(UNCOLORED);
    paint.DrawStringAt(0, 0, alt, &Font12, COLORED);
    display.SetFrameMemory(paint.GetImage(), 0, 0, paint.GetWidth(), paint.GetHeight());

    display.DisplayFrame();
  }
}

// returns the number of digits in a double, assuming 4 digits of precision.
// doesn't work if l is >= 1k.
int numDigitsDouble(double l) {
  if (l < 10.0) {
    return 6;
  } else if (l < 100.0) {
    return 7;
  } else {
    return 8;
  }
}

// returns the number of digits for a double that gets converted to an integer.
// it's used for altitude, so assume a max of 5 digits.
int numDigitsInt(double d) {
  if (d < 10.0) {
    return 1;
  } else if (d < 100.0) {
    return 2;
  } else if (d < 1000.0) {
    return 3;
  } else if (d < 10000.0) {
    return 4;
  } else {
    return 5; // it's for altitude. this is as big as it gets.
  }
}

// update the memory fields for the various sensor objects.
// there is some clumsy pointer manipulation here that could 
// be cleaned up quite a bit by someone who knows C.
void updateFields()
{
  // copy the location. 
  int bufPos = 0;
  if (gps.location.isValid()) {
    dtostrf(abs(gps.location.lat()), -1, 4, loc + bufPos);
    bufPos += numDigitsDouble(abs(gps.location.lat()));
    loc[bufPos++] = gps.location.lat() < 0.0 ? 'S' : 'N';
    loc[bufPos++] = ',';
    loc[bufPos++] = ' ';
    dtostrf(abs(gps.location.lng()), -1, 4, loc + bufPos);
    bufPos += numDigitsDouble(abs(gps.location.lng()));
    loc[bufPos++] = gps.location.lng() < 0 ? 'W' : 'E';
    loc[bufPos++] = '\0';
  } else {
    strcpy(loc, NO_READING);
  }

  // copy the time.
  // hour
  if (gps.time.isUpdated()) {
    bufPos = 0;
    if (gps.time.hour() < 10) {
      time[bufPos++] = '0';
      dtostrf(gps.time.hour(), -1, 0, time + bufPos);
      bufPos += 1;
    } else {
      dtostrf(gps.time.hour(), -1, 0, time + bufPos);
      bufPos += 2;
    }
    time[bufPos++] = ':';
    // minute
    if (gps.time.minute() < 10) {
      time[bufPos++] = '0';
      dtostrf(gps.time.minute(), -1, 0, time + bufPos);
      bufPos += 1;
    } else {
      dtostrf(gps.time.minute(), -1, 0, time + bufPos);
      bufPos += 2;
    }
    time[bufPos++] = ':';
    // second
    if (gps.time.second() < 10) {
      time[bufPos++] = '0';
      dtostrf(gps.time.second(), -1, 0, time + bufPos);
      bufPos += 1;
    } else {
      dtostrf(gps.time.second(), -1, 0, time + bufPos);
      bufPos += 2;
    }
    time[bufPos++] = ' ';
    time[bufPos++] = 'U';
    time[bufPos++] = 'T';
    time[bufPos++] = 'C';
    time[bufPos++] = ' ';
    // temperature
    dtostrf(curTemp, -1, 0, time + bufPos);
    bufPos += numDigitsInt(curTemp);
    time[bufPos++] = 'F';
    time[bufPos++] = '\0';
  } else if (!gps.time.isValid()) {
    strcpy(time, NO_GPS);
  } else if (gps.time.age() > 30000) {
    strcpy(time, NO_READING);
  }

  // copy the date.
  if (gps.date.isUpdated()) {
    bufPos = 0;
    // month
    if (gps.date.month() < 10) {
      date[bufPos++] = '0';
      dtostrf(gps.date.month(), -1, 0, date + bufPos);
      bufPos += 1;
    } else {
      dtostrf(gps.date.month(), -1, 0, date + bufPos);
      bufPos += 2;
    }
    date[bufPos++] = '/';
    // day
    if (gps.date.day() < 10) {
      date[bufPos++] = '0';
      dtostrf(gps.date.day(), -1, 0, date + bufPos);
      bufPos += 1;
    } else {
      dtostrf(gps.date.day(), -1, 0, date + bufPos);
      bufPos += 2;
    }
    date[bufPos++] = '/';
    // year.
    dtostrf(gps.date.year(), -1, 0, date + bufPos);
    bufPos += 4;
    // add the grid
    date[bufPos++] = ' ';
    char *grid = get_mh(gps.location.lat(), gps.location.lng(), 6);
    date[bufPos++] = grid[0];
    date[bufPos++] = grid[1];
    date[bufPos++] = grid[2];
    date[bufPos++] = grid[3];
    date[bufPos++] = grid[4];
    date[bufPos++] = grid[5];
    date[bufPos++] = '\0';
  } else if (gps.date.age() > 3600000) {
    strcpy(date, NO_READING);
  }

  // alt + sats
  if (!gps.altitude.isValid()) {
    strcpy(alt, NO_GPS);
  } else if (gps.altitude.isUpdated()) {
    bufPos = 0;
    if (gps.altitude.feet() < 0.0) {
      alt[bufPos++] = '-';
    }
    dtostrf(abs(gps.altitude.feet()), -1, 0, alt + bufPos);
    bufPos += numDigitsInt(abs(gps.altitude.feet()));
    alt[bufPos++] = ' ';
    alt[bufPos++] = 'f';
    alt[bufPos++] = 't';
    alt[bufPos++] = ',';
    alt[bufPos++] = ' ';
  
    dtostrf(gps.satellites.value(), -1, 0, alt + bufPos);
    bufPos += numDigitsInt(gps.satellites.value());
    alt[bufPos++] = ' ';
    alt[bufPos++] = 's';
    alt[bufPos++] = 'a';
    alt[bufPos++] = 't';
    alt[bufPos++] = 's';
    alt[bufPos++] = ' ';
    alt[bufPos++] = '\0';
  } else {
    strcpy(alt, NO_READING);
  }
//    lastFix = gps.time.age();
}
