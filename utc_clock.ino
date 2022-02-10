 #define ONEWIRE_SEARCH 0

#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <SPI.h>
#include <DS18B20.h>
#include <maidenhead.h>

#include <epd2in9_V2.h>
#include <epdif.h>
#include <epdpaint.h>
#include <fonts.h>
#include <imagedata.h>

#define COLORED           0
#define UNCOLORED         1
#define GPS_TX_ARD_RX     3 
#define GPS_RX_ARD_TX     2
#define ONEWIRE_PIN       6
#define MAX_TEMP_ATTEMPTS    100
#define GPS_LOCK_TIMEOUT  10000
#define GPS_STATE_FRESH   1
#define GPS_STATE_STALE   2
#define GPS_STATE_LOST    3


// GPS stuff.
TinyGPSPlus gps;
SoftwareSerial gpsSerial(GPS_TX_ARD_RX, GPS_RX_ARD_TX);
unsigned long gpsState = GPS_STATE_LOST;
unsigned long gpsLastRead = 0;

// temperature stuff.
DS18B20 ds(ONEWIRE_PIN);
float curTemp = 0.0;

// Epaper stuff.
unsigned char image[1024];
Paint paint(image, 0, 0);    // width should be the multiple of 8 
Epd display;
boolean epdOk = false;

// display fields
char loc[20] = "00.0000N, 000.0000W\0";
char alt[20] = "+000000 ft, 00 sats        \0";
char time[22] = "HH:MM:SS UTC         \0";
char date[18] = "00/00/0000       \0";
char NO_READING[11] = "NO READING\0";
char NO_GPS[7] = "NO GPS\0";
char STALE[6] = "STALE\0";

void setup() {
  Serial.begin(9600);
  analogReference(EXTERNAL);
  gpsSerial.begin(9600);
  Serial.println(F("Serial devices initialized"));

  // initialize waveshare display
  if (display.Init() != 0) {
    Serial.println(F("EPD initialization failed"));
  } else {
    Serial.println(F("EPD ok"));
    epdOk = true;
    updateValues();
    display.ClearFrameMemory(0xFF);
    display.DisplayFrame();
    repaintScreen();
  }
}

void loop() {
  Serial.println(F("Loop"));
  
  // reset fresh check and poll gps
  gpsState = GPS_STATE_STALE;
  safeDelay(9000);

  // die if GPS never warmed up.
  if (millis() > 5000 && gps.charsProcessed() < 10) {
    Serial.println(F("No GPS detected: check wiring."));
    // TODO: turn on both lights.
    while(true);
  }

  unsigned long loopStart = millis();

  // see if we've lost our gps lock
  if (loopStart - gpsLastRead > GPS_LOCK_TIMEOUT) {
    gpsState = GPS_STATE_LOST;
  }

  pollTemp();

  updateValues();
  repaintScreen();
}

void updateValues() {
  int bufPos = 0; // pointer.

  // gps data
  if (gpsState == GPS_STATE_FRESH && gps.location.isValid()) {
    dtostrf(abs(gps.location.lat()), -1, 4, loc + bufPos);
    bufPos += numDigitsDouble(abs(gps.location.lat()));
    loc[bufPos++] = gps.location.lat() < 0.0 ? 'S' : 'N';
    loc[bufPos++] = ',';
    loc[bufPos++] = ' ';
    dtostrf(abs(gps.location.lng()), 01, 4, loc + bufPos);
    bufPos += numDigitsDouble(abs(gps.location.lng()));
    loc[bufPos++] = gps.location.lng() < 0 ? 'W' : 'E';
    loc[bufPos++] = '\0';
  } else {
    strcpy(loc, NO_READING);
  }

  // time and temperature.
  if (gpsState == GPS_STATE_FRESH && gps.time.isUpdated()) {
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
  } else if (gps.time.age() > 30000) {
    strcpy(time, NO_READING);
  }

  // date
  if (gpsState == GPS_STATE_FRESH && gps.date.isUpdated()) {
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

  // altitude + sats
  if (gpsState == GPS_STATE_FRESH && gps.altitude.isValid()) {
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
  
}

void repaintScreen() {
  if (epdOk) {
    paint.SetRotate(ROTATE_90);
    paint.SetWidth(24);
    paint.SetHeight(295);

    // location.
    paint.Clear(UNCOLORED);
    paint.DrawStringAt(0, 0, loc, &Font20, COLORED);
    display.SetFrameMemory(paint.GetImage(), 100, 0, paint.GetWidth(), paint.GetHeight());

    // time.
    paint.Clear(UNCOLORED);
    paint.DrawStringAt(0, 0, time, &Font24, COLORED);
    display.SetFrameMemory(paint.GetImage(), 68, 0, paint.GetWidth(), paint.GetHeight());

    // date
    paint.Clear(UNCOLORED);
    paint.DrawStringAt(0, 0, date, &Font20, COLORED);
    display.SetFrameMemory(paint.GetImage(), 34, 0, paint.GetWidth(), paint.GetHeight());

    // altitude
    paint.Clear(UNCOLORED);
    paint.DrawStringAt(0, 0, alt, &Font12, COLORED);
    display.SetFrameMemory(paint.GetImage(), 0, 0, paint.GetWidth(), paint.GetHeight());

    display.DisplayFrame();
  }
}

// tries to get the temperature.
// be warned: this guy takes about 1 second to run.
float pollTemp() {
  curTemp = 0.0; // small race here.
  for (int i = 0; i < MAX_TEMP_ATTEMPTS; i++) {
    if (ds.selectNext()) {
      curTemp = ds.getTempF();
      break;
    }
  }
  if (curTemp == 0.0) {
    Serial.println(F("Failed temp"));
  }
}


void safeDelay(unsigned long ms) {
  unsigned long start = millis();
  do {
    _pollGps(ms - millis() + start);
  } while (millis() - start < ms);
}

void _pollGps(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms && gpsSerial.available()) {
    if (gps.encode(gpsSerial.read())) {
      gpsState = GPS_STATE_FRESH;
      gpsLastRead = millis();
    }
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

void _dumpGps() {
  Serial.print(F("LAT : ")); Serial.println(gps.location.lat(), 6); // Latitude in degrees (double)
  Serial.print(F("LON : "));Serial.println(gps.location.lng(), 6); // Longitude in degrees (double)
  Serial.print(F("RLAT: "));Serial.print(gps.location.rawLat().negative ? "-" : "+");
  Serial.println(gps.location.rawLat().deg); // Raw latitude in whole degrees
//  Serial.println(gps.location.rawLat().billionths);// ... and billionths (u16/u32)
  Serial.print(F("RLON: "));Serial.print(gps.location.rawLng().negative ? "-" : "+");
  Serial.println(gps.location.rawLng().deg); // Raw longitude in whole degrees
//  Serial.println(gps.location.rawLng().billionths);// ... and billionths (u16/u32)
  Serial.print(F("DATE: "));Serial.println(gps.date.value()); // Raw date in DDMMYY format (u32)
//  Serial.println(gps.date.year()); // Year (2000+) (u16)
//  Serial.println(gps.date.month()); // Month (1-12) (u8)
//  Serial.println(gps.date.day()); // Day (1-31) (u8)
  Serial.print(F("TIME: "));Serial.println(gps.time.value()); // Raw time in HHMMSSCC format (u32)
//  Serial.println(gps.time.hour()); // Hour (0-23) (u8)
//  Serial.println(gps.time.minute()); // Minute (0-59) (u8)
//  Serial.println(gps.time.second()); // Second (0-59) (u8)
//  Serial.println(gps.time.centisecond()); // 100ths of a second (0-99) (u8)
//  Serial.println(gps.speed.value()); // Raw speed in 100ths of a knot (i32)
//  Serial.println(gps.speed.knots()); // Speed in knots (double)
//  Serial.println(gps.speed.mph()); // Speed in miles per hour (double)
//  Serial.println(gps.speed.mps()); // Speed in meters per second (double)
//  Serial.println(gps.speed.kmph()); // Speed in kilometers per hour (double)
//  Serial.println(gps.course.value()); // Raw course in 100ths of a degree (i32)
//  Serial.println(gps.course.deg()); // Course in degrees (double)
//  Serial.println(gps.altitude.value()); // Raw altitude in centimeters (i32)
//  Serial.println(gps.altitude.meters()); // Altitude in meters (double)
//  Serial.println(gps.altitude.miles()); // Altitude in miles (double)
//  Serial.println(gps.altitude.kilometers()); // Altitude in kilometers (double)
  Serial.print(F("ALT: "));Serial.println(gps.altitude.feet()); // Altitude in feet (double)
  Serial.print(F("SATS: "));Serial.println(gps.satellites.value()); // Number of satellites in use (u32)
  Serial.print(F("PREC: "));Serial.println(gps.hdop.value()); // Horizontal Dim. of Precision (100ths-i32)
  Serial.println();
}
