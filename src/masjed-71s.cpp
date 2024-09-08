#include <Arduino.h>
#include <RTClib.h>
#include <Wire.h>
#include <PrayerTimes.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
// #include <umm_malloc/umm_heap_select.h>
#include <OneButton.h>
#include <PString.h>
#include <SPI.h>
#include <ESP8266mDNS.h>
#include <SoftwareSerial.h>
#include <JQ6500_Serial.h>

#include "config.h"

#if DEBUG == 1
#define log(xdata) Serial.print(xdata)
#define logln(xdata) Serial.println(xdata)
#else
#define log(xdata) (void)(0)
#define logln(xdata) (void)(0)
#endif

SoftwareSerial mySerial(-1, 16);
JQ6500_Serial mp3(mySerial);

AsyncWebServer server(80);
DNSServer dnsServer;

OneWire oneWire(tempSensorPin);
DallasTemperature sensors(&oneWire);

double times[sizeof(TimeName) / sizeof(char *)];
RTC_DS3231 rtc;
DateTime dt;
DateTime summerStart;
DateTime winterStart;

// OneButton btn(buttonPin, true, true);

char buffer[250];
PString tempChar(buffer, sizeof(buffer));
char buffer2[250];
PString tempChar2(buffer2, sizeof(buffer2));

int buzzerRepeat = 0, prayerNum;
bool isSleepNow, isUpdateNow, isSplashScreen = true, isDateHijri, isFlashPrayer, isFlashEqama,isLoadingValues, wifiStatus = true, isUploadUpdate, isRtcConnected, isTestMode, testSoundMode, azanSoundFinsish;
static int isRestartNow;

unsigned long buzzerTimer, oneSecTimer, halfSecTimer, splashTime, halfHourTimer, restartMillis, closeWiFiTimer;

void shiftOut(uint8_t dataPin2, uint8_t clockPin2, uint8_t bitOrder, uint8_t val)
{
  for (int i = 0; i < 8; i++)
  {
    digitalWrite(dataPin2, !!(val & (1 << i)));
    digitalWrite(clockPin2, HIGH);
    digitalWrite(clockPin2, LOW);
  }
}

void restart()
{
  logln("Restart Begin");
  isRestartNow = true;
  restartMillis = millis();
}

void uploadProgress(int progress)
{
  digitalWrite(latchPin, LOW);
  for (int i = 0; i < 37; i++)
  {
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
  }
  int fn = (progress % 10);
  int sn = ((progress / 10) % 10);

  shiftOut(dataPin, clockPin, LSBFIRST, segments[fn]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[sn]);
  // digitalWrite(latchPin, LOW);
  digitalWrite(latchPin, HIGH);
}

void httpProcessUpdate(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
{
  uint32_t free_space = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
  if (!index)
  {
    isUploadUpdate = true;
    logln("Updating ...");
    log("free_space : ");
    logln(free_space);
    Update.runAsync(true);
    isUpdateNow = true;
    if (!Update.begin(free_space))
    {
      Update.printError(Serial);
    }
    Serial.printf("UploadStart: %s\n", filename.c_str());
  }

  if (Update.write(data, len) != len)
  {
    Update.printError(Serial);
  }
  else
  {
    int h = ((float)Update.progress() / 385024) * 100;
    if (h != prayerNum)
    {
      prayerNum = h;
      uploadProgress(h);
      logln(Update.progress());
      logln(h);
    }
  }

  if (final)
  {
    if (!Update.end(true))
    {
      Update.printError(Serial);
      logln("Update Error");
      isUploadUpdate = false;
      request->send(200, "text/plain", "UpdateError");
    }
    else
    {
      restart();
      isUploadUpdate = false;
      logln("Update complete");
      request->send(200, "text/plain", "UpdateOk");
    }
  }
}

const char *key = "Abonagy@2";
uint8_t keyLength = strlen(key);
void encrypt(Configration &config)
{
  logln(">> encrypt");

  // Convert configuration data to byte array
  uint8_t *data = (uint8_t *)&config;
  size_t dataLength = sizeof(config);

  // XOR each byte of data with corresponding byte of key
  for (size_t i = 0; i < dataLength; i++)
  {
    data[i] ^= key[i % keyLength];
  }
}
void decrypt(Configration &config)
{
  logln(">> decrypt");

  // Convert configuration data to byte array
  uint8_t *data = (uint8_t *)&config;
  size_t dataLength = sizeof(config);

  // XOR each byte of data with corresponding byte of key
  for (size_t i = 0; i < dataLength; i++)
  {
    data[i] ^= key[i % keyLength];
  }
}
const char *getValue(const char *data, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = strlen(data) - 1;

  // logln("data");
  // logln(data);
  for (int i = 0; i <= maxIndex && found <= index; i++)
  {
    // logln(data[i]);
    if ((data[i] == '-') || (i == maxIndex))
    {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  tempChar2.begin();
  if (found > index)
  {
    for (int z = strIndex[0]; z < strIndex[1]; z++)
    {
      tempChar2.print(data[z]);
    }
  }
  else
  {
    tempChar2 = "";
  }
  // logln(tempChar2);
  return tempChar2;
}
const char *blur(const char *text)
{
  int length = strlen(text);
  // log("Input Data ");
  // logln(text);
  // log("Lenght ");
  // logln(lenght);

  // // First Stage change char by next 5 char
  // char data[length];
  // for (int i = 0; i < length; i++)
  // {
  //   char c = text[i] + 5;
  //   data[i] = c;
  // }
  // data[length] = '\0'; // Add the null terminator
  // logln("Data");
  // logln(data);

  // Second Stage Convert char to int + 37
  tempChar2.begin();
  for (int x = 0; x < length; x++)
  {
    int asci = (int)text[x];
    asci = asci + 37;
    tempChar2.print(asci);
    if (x != (length - 1))
      tempChar2.print('-');
  }
  // log("Blur Data : ");
  // logln(tempChar2);
  const char *datas = tempChar2;
  return datas;
}
const char *unBlur(const char *text)
{
  int lenght = strlen(text);
  int commaCount = 0;
  // for (unsigned int x = 0; x < text.length(); x++)
  for (int x = 0; x < lenght; x++)
  {
    if (text[x] == '-')
      commaCount++;
  }
  // log("Comma Count = ");
  // logln(commaCount);

  tempChar.begin();
  for (int x = 0; x <= commaCount; x++)
  {
    const char *split = getValue(text, x);
    char ch = (char)(atoi(split) - 37);
    // logln(split);
    tempChar.print(ch);
  }
  log("unBlur Data : ");
  logln(tempChar);
  const char *data = tempChar;
  return data;
}

void intiLittleFS()
{
  logln(">> intiLittleFS");

  (LittleFS.begin()) ? logln("LittleFS start Success") : logln("LittleFS start Faield");
}
bool getConfig()
{
  logln(">> getConfig");

  File configFile = LittleFS.open("/config-1.bin", "r");
  if (configFile)
  {
    if (configFile.read((uint8_t *)&config, sizeof(config)))
    {
      configFile.close();
      decrypt(config);

      logln("< Data >");
      log("calcMethod > ");
      logln(config.calcMethod);
      log("wayMethod > ");
      logln(config.wayMethod);
      log("latitude > ");
      logln(config.latitude);
      log("longitude > ");
      logln(config.longitude);
      log("dst > ");
      logln(config.dst);
      log("hijriDayCorection > ");
      logln(config.hijriDayCorection);
      log("eqamaFajr > ");
      logln(config.eqamaFajr);
      log("eqamaShrouq > ");
      logln(config.eqamaShrouq);
      log("eqamaZohr > ");
      logln(config.eqamaZohr);
      log("eqamaAsr > ");
      logln(config.eqamaAsr);
      log("eqamaMagrb > ");
      logln(config.eqamaMagrb);
      log("eqamaEsha > ");
      logln(config.eqamaEsha);
      log("sleepGomaaTime > ");
      logln(config.sleepGomaaTime);
      log("sleepPrayerTime > ");
      logln(config.sleepPrayerTime);
      log("enableBuzzer > ");
      logln(config.enableBuzzer);
      log("countery > ");
      logln(config.countery);
      log("government > ");
      logln(config.government);
      log("password > ");
      logln(config.wifiPassword);
      log("summerTime > ");
      logln(config.summerTime);
      log("sleepBeforFajr > ");
      logln(config.sleepBeforFajr);
      log("sleepAfterEsha > ");
      logln(config.sleepAfterEsha);
      log("azanSound > ");
      logln(config.azanSound);
      log("eqamaSound > ");
      logln(config.eqamaSound);
      log("soundLevel > ");
      logln(config.soundLevel);

      return true;
    }
    else
    {
      logln("Failed to Read config file");
      return false;
    }
  }
  else
  {
    Serial.println("Failed to open config file");
    return false;
  }
}
bool setConfig()
{
  logln(">> setConfig");

  File configFile = LittleFS.open("/config-1.bin", "w");
  if (configFile)
  {
    Configration tmpConfig;
    tmpConfig = config;
    encrypt(tmpConfig);
    if (configFile.write((uint8_t *)&tmpConfig, sizeof(tmpConfig)))
    {
      logln("setConfig Success");
      return true;
    }
    else
    {
      logln("Failed to Write data to File");
      return false;
    }
    configFile.close();
  }
  else
  {
    logln("Failed to open config file for writing");
    return false;
  }
}
void ListOfFiles()
{
  logln(">> ListOfFiles");

  Dir dir = LittleFS.openDir("/");

  while (dir.next())
  {
    log(dir.fileName());
    log(" >> SIZE: ");
    logln(dir.fileSize());
  }
}

void resetDefault()
{
  logln(">> resetDefault");
  config.calcMethod = 0;
  config.wayMethod = 0;
  config.latitude = 50;
  config.longitude = 50;
  config.dst = 0;
  config.hijriDayCorection = 0;
  config.eqamaFajr = 0;
  config.eqamaShrouq = 0;
  config.eqamaZohr = 0;
  config.eqamaAsr = 0;
  config.eqamaMagrb = 0;
  config.eqamaEsha = 0;
  config.sleepGomaaTime = 0;
  config.sleepPrayerTime = 0;
  config.enableBuzzer = 0;
  config.countery = 0;
  config.government = 0;
  config.wifiPassword = 12881288;
  config.summerTime = 0;
  config.sleepAfterEsha = 0;
  config.sleepBeforFajr = 0;
  config.azanSound = 0;
  config.eqamaSound = 0;
  config.soundLevel = 0;

  (LittleFS.format()) ? logln("Format LittleFS Success") : logln("Format LittleFS Faield");
  (setConfig()) ? logln("Reset Default Success") : logln("Reset Default Faield");
  if (getConfig())
    logln("Reset And Load Default Success");
}
void resetAndRestart()
{
  logln(">> resetAndRestart");
  resetDefault();
  restart();
}

int lastFridayInApril(int year)
{
  DateTime lastDayInApril(year, 4, 30);
  int dayOfWeek = lastDayInApril.dayOfTheWeek(); // 0 = Sunday, 1 = Monday, ..., 6 = Saturday
  int lastFriday = 30 - ((dayOfWeek - 5 + 7) % 7);
  // log("last Friday In April ");
  // logln(lastFriday);
  return lastFriday;
}
int lastFridayInOctober(int year)
{
  DateTime lastDayInOctober(year, 10, 31);
  int dayOfWeek = lastDayInOctober.dayOfTheWeek(); // 0 = Sunday, 1 = Monday, ..., 6 = Saturday
  int lastThursday = 31 - ((dayOfWeek - 5 + 7) % 7);
  // log("last Friday In October ");
  // logln(lastThursday);
  return lastThursday;
}
bool isSummerTime(DateTime now)
{
  logln(">> isSummerTime");

  summerStart = DateTime(rtc.now().year(), 4, lastFridayInApril(rtc.now().year()));
  winterStart = DateTime(rtc.now().year(), 10, lastFridayInOctober(rtc.now().year())) - TimeSpan(0, 1, 0, 0);

  if (config.summerTime == 0)
    return false;

  if ((now > summerStart) && (now < winterStart))
  {
    logln("Summer");
    return true;
  }
  else
  {
    logln("Winter");
    return false;
  }
}

long Days(uint16_t Y, uint8_t M, uint8_t D)
{
  if (M < 3)
  {
    Y -= 1;
    M += 12;
  }
  Y = Y - 2000;
  long ndays = floor(365.25 * Y) + floor(30.6001 * (M + 1)) + floor(Y / 100) +
               floor(Y / 400) + D + 196;
  // long ndays= d1 + d2 - A + B + D + 196;
  return ndays;
}
long DaysHijri(uint16_t Y, uint8_t M, uint8_t D)
{
  Y = Y - 1420;
  long hari = floor(29.5 * M - 28.999) + floor(354.367068 * Y) + D;

  log("hari ");
  logln(hari);

  return hari;
}
void Hijri()
{
  logln(">> Hijri");

  if (dt.year() < 2023)
  {
    // var.hDay = 1;
    // var.hMonth = 1;
    // var.hYear = 1400;
    logln("year < 2023 Can't get Hijri");
    return;
  }

  if (isRtcConnected == false)
    return;

  var.day = dt.day();

  long nday = Days(dt.year(), dt.month(), dt.day()) + config.hijriDayCorection;
  long Y = floor(nday / 354.367068) + 1420;
  long M = 1;
  long D = 1;

  logln("Start While");
  while (DaysHijri(Y, M, 1) <= nday)
  {
    Y++;
  };
  Y--;
  logln(dt.year());
  while (DaysHijri(Y, M, 1) <= nday)
  {
    M++;
  };
  M--;
  logln(dt.month());
  D = 1 + nday - DaysHijri(Y, M, 1);
  if (M == 13)
  {
    M = 12;
    D += 29;
  };
  logln("End While");

  // var.hYear = Y;
  // var.hMonth = M;
  // var.hDay = D;

  data.dayA = (D % 10);
  data.dayB = ((D / 10) % 10);

  data.monA = (M % 10);
  data.monB = ((M / 10) % 10);

  data.yeaA = (Y % 10);
  data.yeaB = ((Y / 10) % 10);
  data.yeaC = ((Y / 100) % 10);
  data.yeaD = ((Y / 1000) % 10);

  log(Y);
  log("-");
  log(M);
  log("-");
  logln(D);
}

void setMethod(CalculationMethod prayerMethod, double fajr, double isha)
{
  set_calc_method(prayerMethod);
  set_fajr_angle(fajr);
  set_isha_angle(isha);
}
void getPrayerTimes()
{
  logln(">> getPrayerTimes");
  // set_high_lats_adjust_method(None);
  set_high_lats_adjust_method(AngleBased);

  switch (config.calcMethod)
  {
  case 0:
    setMethod(Egypt, 19.5, 17.5);
    break;
  case 1:
    setMethod(Makkah, 18.5, 17);
    break;
  case 2:
    setMethod(Custom, 18, 17.5);
    break;
  case 3:
    setMethod(MWL, 18, 17);
    break;
  case 4:
    setMethod(Karachi, 18, 18);
    break;
  }

  (config.wayMethod == 0) ? set_asr_method(Shafii) : set_asr_method(Hanafi);

  int dst = config.dst;
  if (isSummerTime(dt))
    dst = config.dst + 1; // For Summer Time Add 1 Hour to dst

  get_prayer_times(dt.year(), dt.month(), dt.day(), config.latitude, config.longitude, dst, times);

  byte hours, minutes;
  get_float_time_parts(times[0], hours, minutes);
  data.fajH = (hours % 10);
  data.fajA = (minutes % 10);
  data.fajB = ((minutes / 10) % 10);
  log("Fajr ");
  log(hours);
  logln(minutes);

  get_float_time_parts(times[1], hours, minutes);
  data.shrH = (hours % 10);
  data.shrA = (minutes % 10);
  data.shrB = ((minutes / 10) % 10);
  log("Shro ");
  log(hours);
  logln(minutes);

  get_float_time_parts(times[2], hours, minutes);
  if (hours > 12)
    hours = hours - 12;
  data.zohHA = (hours % 10);
  data.zohHB = ((hours / 10) % 10);
  data.zohMA = (minutes % 10);
  data.zohMB = ((minutes / 10) % 10);
  log("Dohr ");
  log(hours);
  logln(minutes);

  get_float_time_parts(times[3], hours, minutes);
  if (hours > 12)
    hours = hours - 12;
  data.asrH = (hours % 10);
  data.asrMA = (minutes % 10);
  data.asrMB = ((minutes / 10) % 10);
  log("Asrr ");
  log(hours);
  logln(minutes);

  get_float_time_parts(times[4], hours, minutes);
  if (hours > 12)
    hours = hours - 12;
  data.magH = (hours % 10);
  data.magMA = (minutes % 10);
  data.magMB = ((minutes / 10) % 10);
  log("Magr ");
  log(hours);
  logln(minutes);

  get_float_time_parts(times[6], hours, minutes);
  if (hours > 12)
    hours = hours - 12;
  data.eshHA = (hours % 10);
  data.eshHB = ((hours / 10) % 10);
  data.eshMA = (minutes % 10);
  data.eshMB = ((minutes / 10) % 10);
  log("Esha ");
  log(hours);
  logln(minutes);
}

void getTemp()
{
  logln(">> getTemp");
  sensors.requestTemperatures(); // Request temperature readings

  // Check if the sensor is detected
  if (sensors.getDeviceCount() > 0)
  {
    float temperatureC = sensors.getTempCByIndex(0); // Get temperature in Celsius

    if (temperatureC != DEVICE_DISCONNECTED_C)
    {
      // Convert temperature to an integer value
      int temperatureInt = static_cast<int>(temperatureC);
      data.tmpA = (temperatureInt % 10);
      data.tmpB = ((temperatureInt / 10) % 10);
      log("Temperature: ");
      logln(temperatureInt);
    }
    else
    {
      logln("Error: Unable to read temperature!");
      data.tmpA = 0;
      data.tmpB = 0;
    }
  }
  else
  {
    logln("No DS18B20 sensor detected!");
    data.tmpA = 8;
    data.tmpB = 8;
  }
}

void getDateTime()
{
  logln(">> getDateTime");

  if (isRtcConnected)
  {
    (isSummerTime(rtc.now())) ? dt = rtc.now() + TimeSpan(0, 1, 0, 0) : dt = rtc.now();

    if (dt.second() % 5 == 0) // Change between Hijri and milady Every 5 Seconds
      isDateHijri = !isDateHijri;

    data.minA = (dt.minute() % 10);
    data.minB = ((dt.minute() / 10) % 10);

    int h = dt.hour();
    if (h > 12)
      h = dt.hour() - 12;
    if (h == 0)
      h = 12;

    data.horA = (h % 10);
    data.horB = ((h / 10) % 10);

    data.secA = (dt.second() % 10);
    data.secB = ((dt.second() / 10) % 10);

    if (isDateHijri)
    {
      // if (isDateHijri && var.day != dt.day()) // get Hijri data when day start
      Hijri();
      // data.dayA = (var.hDay % 10);
      // data.dayB = ((var.hDay / 10) % 10);

      // data.monA = (var.hMonth % 10);
      // data.monB = ((var.hMonth / 10) % 10);

      // data.yeaA = (var.hYear % 10);
      // data.yeaB = ((var.hYear / 10) % 10);
      // data.yeaC = ((var.hYear / 100) % 10);
      // data.yeaD = ((var.hYear / 1000) % 10);
    }
    else
    {
      data.dayA = (dt.day() % 10);
      data.dayB = ((dt.day() / 10) % 10);

      data.monA = (dt.month() % 10);
      data.monB = ((dt.month() / 10) % 10);

      data.yeaA = (dt.year() % 10);
      data.yeaB = ((dt.year() / 10) % 10);
      data.yeaC = ((dt.year() / 100) % 10);
      data.yeaD = ((dt.year() / 1000) % 10);
    }
  }
  else
  {
    rtc.adjust(DateTime(2024, 2, 1, 1, 0, 0));
  }
}

void sleeping()
{
  logln("Sleeping ...");
  isSleepNow = false;

  digitalWrite(latchPin, LOW);
  for (int i = 0; i < 37; i++)
  {
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
  }
  // shiftOut(dataPin, clockPin, LSBFIRST, segments[11]);
  // shiftOut(dataPin, clockPin, LSBFIRST, segments[11]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[11]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[11]);
  // digitalWrite(latchPin, LOW);
  digitalWrite(latchPin, HIGH);
}

void startMP3()
{
  mySerial.begin(9600);
  mp3.reset();
  mp3.setVolume(config.soundLevel);
  // mp3.setVolume(25);
  // mp3.setLoopMode(MP3_LOOP_ONE_STOP); // Deafult
  // mp3.setEqualizer(MP3_EQ_BASS);
  mp3.playFileByIndexNumber(8);
}
void playAzan()
{
  if (config.azanSound >= 1 && config.azanSound <= 8)
    mp3.playFileByIndexNumber(config.azanSound);
  // mp3.playFileByIndexNumber(2);
  azanSoundFinsish = true;
}
void playEqama()
{
  if (config.eqamaSound >= 9 && config.eqamaSound <= 11)
    mp3.playFileByIndexNumber(config.eqamaSound);
  // mp3.playFileByIndexNumber(10);
  azanSoundFinsish = true;
}

void testMode()
{
  logln("testMode");
  static int count = 0;
  log("Count = ");
  logln(count);
  if (count > 9)
  {
    count = 0;
    isTestMode = false;
    return;
  }

  digitalWrite(latchPin, LOW);
  for (int i = 0; i < 39; i++)
  {
    shiftOut(dataPin, clockPin, LSBFIRST, segments[count]);
  }
  digitalWrite(latchPin, HIGH);

  count++;
}

void Checking()
{
  logln(">> Checking");
  byte PrayH = 0, PrayM = 0, EqamaTime = 0;
  int TimeNow, PrayTime = 0;
  int sleepAfterEsha = 0, sleepBeforeFajr = 0;
  for (byte i = 0; i < 7; i++)
  {
    if (!(i == 5 || i == 7))
    {
      //=== Get Prayer Times ================================
      get_float_time_parts(times[i], PrayH, PrayM);

      //=== get eqama time ==================================
      switch (i)
      {
      case 0:
        EqamaTime = config.eqamaFajr;
        sleepBeforeFajr = (PrayH * 60) + PrayM + config.sleepBeforFajr;
        break;
      case 1:
        EqamaTime = config.eqamaShrouq;
        break;
      case 2:
        EqamaTime = config.eqamaZohr;
        break;
      case 3:
        EqamaTime = config.eqamaAsr;
        break;
      case 4:
        EqamaTime = config.eqamaMagrb;
        break;
      case 6:
        EqamaTime = config.eqamaEsha;
        sleepAfterEsha = (PrayH * 60) + PrayM + config.sleepAfterEsha;
        break;
      }

      TimeNow = dt.hour() * 60 + dt.minute();
      PrayTime = PrayH * 60 + PrayM;

      //=== Power Saving Mode After Esha To Fajr ============
      if (config.sleepAfterEsha > 0 && config.sleepBeforFajr > 0)
      {
        if (TimeNow > sleepAfterEsha && TimeNow < sleepBeforeFajr)
        {
          isSleepNow = true;
        }
      }

      //=== Azan Last Minute ================================
      if (PrayTime - TimeNow == 1)
        azanSoundFinsish = false;
      //=== Azan Now ========================================
      if ((TimeNow == PrayTime) || (TimeNow == PrayTime + 1))
      {
        if (!azanSoundFinsish)
          playAzan();

        logln("Azan Now");
        // log("i = ");
        // logln(i);
        isFlashPrayer = !isFlashPrayer;
        prayerNum = i;
        if (dt.second() >= 58)
          isFlashPrayer = false;
        break;
      }
      //=== Gomaa Sleeping ==================================
      else if ((i == 2) && (config.sleepGomaaTime > 0) && (dt.dayOfTheWeek() == 5))
      {
        azanSoundFinsish = false;
        isFlashPrayer = false;
        if ((TimeNow >= (PrayTime + 2)) && (TimeNow < (PrayTime + config.sleepGomaaTime + 2)))
        {
          logln("Sleep On Gomaa");
          // sleeping();
          isSleepNow = true;
          break;
        }
      }
      //=== Eqama Waiting ===================================
      else if (((TimeNow - PrayTime) >= 2) && (TimeNow < (PrayTime + EqamaTime - 1)))
      {
        azanSoundFinsish = false;
        isFlashPrayer = false;
        // if ((i == 2) && (config.dayOfWeek == 5))
        //   continue;
        if ((i == 2) && (dt.dayOfTheWeek() == 5))
          break;

        log("Num >> ");
        log(i);
        logln(" Eqama Waiting");
        int baky = (PrayTime + EqamaTime) - TimeNow;
        data.eqaA = (baky % 10);
        data.eqaB = ((baky / 10) % 10);
        log("Baky = ");
        logln(baky);
        break;
      }
      //=== Eqama Last Minute ===============================
      else if (TimeNow == ((PrayTime + EqamaTime) - 1))
      {
        azanSoundFinsish = false;
        isFlashPrayer = false;
        if ((i == 2) && (dt.dayOfTheWeek() == 5))
          break;
        logln("Eqama Last Minute");
        isFlashEqama = true;
        int baky = 60 - dt.second();
        data.eqaA = (baky % 10);
        data.eqaB = ((baky / 10) % 10);
        log("Baky = ");
        logln(baky);
        break;
      }
      //=== Eqama Now =======================================
      else if (TimeNow == (PrayTime + EqamaTime))
      {
        isFlashPrayer = false;

        if ((i == 2) && (dt.dayOfTheWeek() == 5))
          break;

        if (!azanSoundFinsish)
          playEqama();

        logln("Eqama Now");
        isFlashEqama = true;
        
        data.eqaA = 11;
        data.eqaB = 11;
        break;
      }
      //=== Prayer Sleeping =================================
      else if ((TimeNow > (PrayTime + EqamaTime)) && (TimeNow < (PrayTime + EqamaTime + config.sleepPrayerTime)) && (i != 1))
      {
        isFlashPrayer = false;
        azanSoundFinsish = false;
        if ((i == 2) && (dt.dayOfTheWeek() == 5))
          break;
        logln("Prayer Sleeping");
        // sleeping();
        isSleepNow = true;
        break;
      }
      data.eqaA = 10;
      data.eqaB = 10;
    }
  }
}

void show()
{
  logln(">> Show");

  digitalWrite(latchPin, LOW);
  //=== Minetes ===============================================
  shiftOut(dataPin, clockPin, LSBFIRST, segments[data.minA]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[data.minB]);

  //=== Hour ===============================================
  shiftOut(dataPin, clockPin, LSBFIRST, segments[data.horA]);
  data.horB == 0 ? shiftOut(dataPin, clockPin, LSBFIRST, segments[10]) : shiftOut(dataPin, clockPin, LSBFIRST, segments[data.horB]);

  //=== Second ===============================================
  shiftOut(dataPin, clockPin, LSBFIRST, segments[data.secA]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[data.secB]);

  //=== Day ===============================================
  shiftOut(dataPin, clockPin, LSBFIRST, segments[data.dayA]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[data.dayB]);

  //=== Month ===============================================
  shiftOut(dataPin, clockPin, LSBFIRST, segments[data.monA]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[data.monB]);

  //=== Year ===============================================
  shiftOut(dataPin, clockPin, LSBFIRST, segments[data.yeaA]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[data.yeaB]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[data.yeaC]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[data.yeaD]);

  //=== Fajr ===============================================
  if ((isFlashPrayer) && (prayerNum == 0))
  {
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    isFlashPrayer = false;
  }
  else
  {
    shiftOut(dataPin, clockPin, LSBFIRST, segments[data.fajA]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[data.fajB]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[data.fajH]);
  }

  //=== Shrook ===============================================
  if ((isFlashPrayer) && (prayerNum == 1))
  {
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    isFlashPrayer = false;
  }
  else
  {
    shiftOut(dataPin, clockPin, LSBFIRST, segments[data.shrA]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[data.shrB]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[data.shrH]);
  }

  //=== Zohr ===============================================
  if ((isFlashPrayer) && (prayerNum == 2))
  {
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    isFlashPrayer = false;
  }
  else
  {
    shiftOut(dataPin, clockPin, LSBFIRST, segments[data.zohMA]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[data.zohMB]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[data.zohHA]);
    data.zohHB == 0 ? shiftOut(dataPin, clockPin, LSBFIRST, segments[10]) : shiftOut(dataPin, clockPin, LSBFIRST, segments[data.zohHB]);
  }

  //=== Asr ===============================================
  if ((isFlashPrayer) && (prayerNum == 3))
  {
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    isFlashPrayer = false;
  }
  else
  {
    shiftOut(dataPin, clockPin, LSBFIRST, segments[data.asrMA]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[data.asrMB]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[data.asrH]);
  }

  //=== Magrib ===============================================
  if ((isFlashPrayer) && (prayerNum == 4))
  {
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    isFlashPrayer = false;
  }
  else
  {
    shiftOut(dataPin, clockPin, LSBFIRST, segments[data.magMA]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[data.magMB]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[data.magH]);
  }

  //=== Esha ===============================================
  if ((isFlashPrayer) && (prayerNum == 6))
  {
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    isFlashPrayer = false;
  }
  else
  {
    shiftOut(dataPin, clockPin, LSBFIRST, segments[data.eshMA]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[data.eshMB]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[data.eshHA]);
    data.eshHB == 0 ? shiftOut(dataPin, clockPin, LSBFIRST, segments[10]) : shiftOut(dataPin, clockPin, LSBFIRST, segments[data.eshHB]);
  }

  //=== Eqama ===============================================
  if (isFlashEqama)
  {
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    isFlashEqama = false;
  }
  else
  {
    shiftOut(dataPin, clockPin, LSBFIRST, segments[data.eqaA]);
    data.eqaB == 0 ? shiftOut(dataPin, clockPin, LSBFIRST, segments[10]) : shiftOut(dataPin, clockPin, LSBFIRST, segments[data.eqaB]);
  }

  //=== Temp ===============================================
  shiftOut(dataPin, clockPin, LSBFIRST, segments[data.tmpA]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[data.tmpB]);

  digitalWrite(latchPin, HIGH);
}

/*
void show()
{
  logln(">> Show");
  // Checking();
  int minA, minB, horA, horB, secA, secB, dayA, dayB, monA, monB, yeaA, yeaB, yeaC, yeaD,
      fajH, fajA, fajB, shrH, shrA, shrB, zohHA, zohHB, zohMA, zohMB, asrH, asrMA, asrMB,
      magH, magMA, magMB, eshHA, eshHB, eshMA, eshMB, eqaA, eqaB, tmpA, tmpB = 0;

  minA = (dt.minute() % 10);
  minB = ((dt.minute() / 10) % 10);

  int h = dt.hour();
  if (h > 12)
    h = dt.hour() - 12;
  if (h == 0)
    h = 12;

  horA = (h % 10);
  horB = ((h / 10) % 10);

  secA = (dt.second() % 10);
  secB = ((dt.second() / 10) % 10);

  if (isDateHijri)
  {
    dayA = (var.hDay % 10);
    dayB = ((var.hDay / 10) % 10);

    monA = (var.hMonth % 10);
    monB = ((var.hMonth / 10) % 10);

    yeaA = (var.hYear % 10);
    yeaB = ((var.hYear / 10) % 10);
    yeaC = ((var.hYear / 100) % 10);
    yeaD = ((var.hYear / 1000) % 10);
  }
  else
  {
    dayA = (dt.day() % 10);
    dayB = ((dt.day() / 10) % 10);

    monA = (dt.month() % 10);
    monB = ((dt.month() / 10) % 10);

    yeaA = (dt.year() % 10);
    yeaB = ((dt.year() / 10) % 10);
    yeaC = ((dt.year() / 100) % 10);
    yeaD = ((dt.year() / 1000) % 10);
  }

  fajH = (var.fajrClock % 10);
  fajA = (var.fajrMinutes % 10);
  fajB = ((var.fajrMinutes / 10) % 10);

  shrH = (var.shroukClock % 10);
  shrA = (var.shroukMinutes % 10);
  shrB = ((var.shroukMinutes / 10) % 10);

  zohHA = (var.zohrClock % 10);
  zohHB = ((var.zohrClock / 10) % 10);
  zohMA = (var.zohrMinutes % 10);
  zohMB = ((var.zohrMinutes / 10) % 10);

  asrH = (var.asrClock % 10);
  asrMA = (var.asrMinutes % 10);
  asrMB = ((var.asrMinutes / 10) % 10);

  magH = (var.magrebClock % 10);
  magMA = (var.magrebMinutes % 10);
  magMB = ((var.magrebMinutes / 10) % 10);

  eshHA = (var.eshaClock % 10);
  eshHB = ((var.eshaClock / 10) % 10);
  eshMA = (var.eshaMinutes % 10);
  eshMB = ((var.eshaMinutes / 10) % 10);

  eqaA = (var.eqamaAfter % 10);
  eqaB = ((var.eqamaAfter / 10) % 10);

  tmpA = (var.temp % 10);
  tmpB = ((var.temp / 10) % 10);

  digitalWrite(latchPin, LOW);
  //=== Minetes ===============================================
  shiftOut(dataPin, clockPin, LSBFIRST, segments[minA]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[minB]);

  //=== Hour ===============================================
  shiftOut(dataPin, clockPin, LSBFIRST, segments[horA]);
  horB == 0 ? shiftOut(dataPin, clockPin, LSBFIRST, segments[10]) : shiftOut(dataPin, clockPin, LSBFIRST, segments[horB]);

  //=== Second ===============================================
  shiftOut(dataPin, clockPin, LSBFIRST, segments[secA]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[secB]);

  //=== Day ===============================================
  shiftOut(dataPin, clockPin, LSBFIRST, segments[dayA]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[dayB]);

  //=== Month ===============================================
  shiftOut(dataPin, clockPin, LSBFIRST, segments[monA]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[monB]);

  //=== Year ===============================================
  shiftOut(dataPin, clockPin, LSBFIRST, segments[yeaA]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[yeaB]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[yeaC]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[yeaD]);

  //=== Fajr ===============================================
  if ((isFlashPrayer) && (prayerNum == 0))
  {
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
  }
  else
  {
    shiftOut(dataPin, clockPin, LSBFIRST, segments[fajA]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[fajB]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[fajH]);
  }

  //=== Shrook ===============================================
  shiftOut(dataPin, clockPin, LSBFIRST, segments[shrA]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[shrB]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[shrH]);

  //=== Zohr ===============================================
  if ((isFlashPrayer) && (prayerNum == 2))
  {
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
  }
  else
  {
    shiftOut(dataPin, clockPin, LSBFIRST, segments[zohMA]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[zohMB]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[zohHA]);
    zohHB == 0 ? shiftOut(dataPin, clockPin, LSBFIRST, segments[10]) : shiftOut(dataPin, clockPin, LSBFIRST, segments[zohHB]);
  }

  //=== Asr ===============================================
  if ((isFlashPrayer) && (prayerNum == 3))
  {
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
  }
  else
  {
    shiftOut(dataPin, clockPin, LSBFIRST, segments[asrMA]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[asrMB]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[asrH]);
  }

  //=== Magrib ===============================================
  if ((isFlashPrayer) && (prayerNum == 4))
  {
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
  }
  else
  {
    shiftOut(dataPin, clockPin, LSBFIRST, segments[magMA]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[magMB]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[magH]);
  }

  //=== Esha ===============================================
  if ((isFlashPrayer) && (prayerNum == 6))
  {
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
  }
  else
  {
    shiftOut(dataPin, clockPin, LSBFIRST, segments[eshMA]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[eshMB]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[eshHA]);
    eshHB == 0 ? shiftOut(dataPin, clockPin, LSBFIRST, segments[10]) : shiftOut(dataPin, clockPin, LSBFIRST, segments[eshHB]);
  }

  //=== Eqama ===============================================
  if (((eqaA == 0) && (eqaB == 0)) && (isFlashEqama == false))
  {
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
  }
  else if ((eqaA == 9) && (eqaB == 9))
  // else if (config.eqamaAfter == 99)
  {
    logln("Equal 99");
    if (isFlashEqama)
    {
      logln("true");
      shiftOut(dataPin, clockPin, LSBFIRST, segments[0]);
      shiftOut(dataPin, clockPin, LSBFIRST, segments[0]);
    }
    else
    {
      logln("false");
      shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
      shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
    }
  }
  else
  {
    shiftOut(dataPin, clockPin, LSBFIRST, segments[eqaA]);
    eqaB == 0 ? shiftOut(dataPin, clockPin, LSBFIRST, segments[10]) : shiftOut(dataPin, clockPin, LSBFIRST, segments[eqaB]);
    var.eqamaAfter = 0;
  }

  //=== Temp ===============================================
  shiftOut(dataPin, clockPin, LSBFIRST, segments[tmpA]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[tmpB]);
  // digitalWrite(latchPin, LOW);
  // delay(100);
  digitalWrite(latchPin, HIGH);
}
*/

/*
void show()
{
  logln(">> Show");

  int h = dt.hour();
  if (h > 12)
    h = dt.hour() - 12;
  if (h == 0)
    h = 12;

  int nums[14] = {dt.minute() % 10, dt.minute() / 10 % 10,
                  h % 10, (h / 10 % 10 == 0) ? 10 : h / 10 % 10,
                  dt.second() % 10, dt.second() / 10 % 10,
                  isDateHijri ? config.hDay % 10 : dt.day() % 10,
                  isDateHijri ? config.hDay / 10 % 10 : dt.day() / 10 % 10,
                  isDateHijri ? config.hMonth % 10 : dt.month() % 10,
                  isDateHijri ? config.hMonth / 10 % 10 : dt.month() / 10 % 10,
                  isDateHijri ? config.hYear % 10 : dt.year() % 10,
                  isDateHijri ? config.hYear / 10 % 10 : dt.year() / 10 % 10,
                  isDateHijri ? config.hYear / 100 % 10 : dt.year() / 100 % 10,
                  isDateHijri ? config.hYear / 1000 % 10 : dt.year() / 1000 % 10};

  int times[20] = {
      // Fajr
      ((isFlashPrayer) && (prayerNum == 0)) ? 0 : config.fajrMinutes % 10,
      ((isFlashPrayer) && (prayerNum == 0)) ? 0 : config.fajrMinutes / 10 % 10,
      ((isFlashPrayer) && (prayerNum == 0)) ? 0 : config.fajrClock % 10,
      config.shroukMinutes % 10,
      config.shroukMinutes / 10 % 10,
      config.shroukClock % 10,
      ((isFlashPrayer) && (prayerNum == 2)) ? 0 : config.zohrMinutes % 10,
      ((isFlashPrayer) && (prayerNum == 2)) ? 0 : config.zohrMinutes / 10 % 10,
      ((isFlashPrayer) && (prayerNum == 2)) ? 0 : config.zohrClock % 10,
      ((isFlashPrayer) && (prayerNum == 2)) ? 0 : (config.zohrClock / 10 % 10 == 0) ? 10
                                                                                   : config.zohrClock / 10 % 10,
      ((isFlashPrayer) && (prayerNum == 3)) ? 0 : config.asrMinutes % 10,
      ((isFlashPrayer) && (prayerNum == 3)) ? 0 : config.asrMinutes / 10 % 10,
      ((isFlashPrayer) && (prayerNum == 3)) ? 0 : config.asrClock % 10,
      ((isFlashPrayer) && (prayerNum == 4)) ? 0 : config.magrebMinutes % 10,
      ((isFlashPrayer) && (prayerNum == 4)) ? 0 : config.magrebMinutes / 10 % 10,
      ((isFlashPrayer) && (prayerNum == 4)) ? 0 : config.magrebClock % 10,
      ((isFlashPrayer) && (prayerNum == 6)) ? 0 : config.eshaMinutes % 10,
      ((isFlashPrayer) && (prayerNum == 6)) ? 0 : config.eshaMinutes / 10 % 10,
      ((isFlashPrayer) && (prayerNum == 6)) ? 0 : config.eshaClock % 10,
      ((isFlashPrayer) && (prayerNum == 6)) ? 0 : (config.eshaClock / 10 % 10 == 0) ? 10
                                                                                   : config.eshaClock / 10 % 10};

  digitalWrite(latchPin, LOW);
  for (int i = 0; i < 14; i++)
  {
    shiftOut(dataPin, clockPin, LSBFIRST, segments[nums[i]]);
  }
  for (int i = 0; i < 20; i++)
  {
    shiftOut(dataPin, clockPin, LSBFIRST, segments[times[i]]);
  }
  shiftOut(dataPin, clockPin, LSBFIRST, segments[(config.eqamaAfter % 10 == 0) ? 10 : config.eqamaAfter % 10]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[(config.eqamaAfter / 10 % 10 == 0) ? 10 : config.eqamaAfter / 10 % 10]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[config.temp % 10]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[config.temp / 10 % 10]);

  digitalWrite(latchPin, HIGH);
}
*/

void splashScreen()
{
  logln(">> splashScreen");
  // OFF All Segment
  digitalWrite(latchPin, LOW);
  for (int i = 0; i < 39; i++)
  {
    // digitalWrite(latchPin, LOW);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
  }
  // digitalWrite(latchPin, LOW);
  digitalWrite(latchPin, HIGH);

  // Loading Segments
  for (int i = 0; i < 39; i++)
  {
    digitalWrite(latchPin, LOW);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[11]);
    // digitalWrite(latchPin, LOW);
    digitalWrite(latchPin, HIGH);
    delay(60);
  }

  // Show Mobile Number
  digitalWrite(latchPin, LOW);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[0]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[1]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[0]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[0]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[3]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[3]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[7]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[0]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[0]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[0]);
  shiftOut(dataPin, clockPin, LSBFIRST, segments[3]);
  for (int i = 0; i < 24; i++)
  {
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
  }
  // digitalWrite(latchPin, LOW);
  digitalWrite(latchPin, HIGH);
  // Delaying 6 Sec.
  splashTime = millis() + 6000;
}

void initAccessPoint()
{
  logln(">> initAccessPoint");

  WiFi.mode(WIFI_AP);
  WiFi.persistent(false); // Solve possible wifi init errors
  WiFi.encryptionType(AUTH_WPA_WPA2_PSK);
  WiFi.hostname(DEVICE_NAME); // Set hostname for access point (optional)
  delay(500);                 // Without delay I've seen the IP address blank

  (WiFi.softAPConfig(mainIP, mainIP, IPAddress(255, 255, 255, 0)))
      ? logln("AccessPoint configure Done")
      : logln("AccessPoint configure Failed");

  tempChar2.begin();
  tempChar2.print(config.wifiPassword);

  boolean result;
  // (config.wifiPassword == 12881288)
  //     ? result = WiFi.softAP(DEVICE_NAME, "12881288")
  //     : result = WiFi.softAP(DEVICE_NAME, tempChar2);
  result = WiFi.softAP(DEVICE_NAME, "12881288");
  (result) ? logln("Access Point Start Success") : logln("Access Point Start Failed");

  delay(150); // Allow some time for AP configuration to complete

  // MDNS Allow to Call server by name Like "Basit-Masjed-8888.local"
  (MDNS.begin(DEVICE_NAME)) ? logln("MDNS Begin Success") : logln("MDNS Begin Failed");
  MDNS.addService("http", "tcp", 80);

  // DNS Server
  dnsServer.setTTL(3600); // Set the lease time to 600 seconds (10 minutes)
  // dnsServer.setTTL(0);  //  Captive portals will usually use a TTL of 0 to avoid DNS cache poisoning.

  // IPAddress dnsIP(8, 8, 8, 8); // Google's DNS server
  // dnsServer.setDNS(dnsIP);
  // Setup the DNS server redirecting all the domains to the apIP
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  (dnsServer.start(53, "*", mainIP))
      ? logln("dnsServer Start Success")
      : logln("dnsServer Start Faield");
}

void serverInit()
{
  logln(">> serverInit");

  // Android captive portal. Maybe not needed. Might be handled by notFound handler.
  server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request)
            { const char *html = R"(<html><head><title>Basit Apps</title><meta charset="UTF-8"> </head><body style="text-align:center;font-size:60px;"><br><br><p>تم الاتصال بنجاح</p><p>افتح البرنامج</p></body></html>)";
              request->send(200, "text/html", html); });
  // Microsoft captive portal. Maybe not needed. Might be handled by notFound handler.
  server.on("/fwlink", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              const char *html = R"(<html><head><title>Basit Apps</title><meta charset="UTF-8"> </head><body style="text-align:center;font-size:60px;"><br><br><p>تم الاتصال بنجاح</p><p>افتح البرنامج</p><p>Microsoft</p></body></html>)";
              request->send(200, "text/html", html); });

  server.onNotFound([](AsyncWebServerRequest *request)
                    { 
                      const char *html = R"(<html><head><title>Basit Apps</title><meta charset="UTF-8"> </head><body style="text-align:center;font-size:60px;"><br><br><p>تم الاتصال بنجاح</p><p>افتح البرنامج</p><p>404</p></body></html>)";
                      request->send(404, "text/html", html); });

  // Serve the HTML page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/html", "<html><body><h1>Welcome Let's Begine The GAME</h1></body></html>"); });

  server.on("/checkUpdate", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              //                           Type,Model,Version
              request->send(200, "text/plain", "update,M,71s,5,good");
              log("CheckUpdate "); });

  server.on("/mac", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              request->send(200, "text/plain", WiFi.macAddress());
              log("mac "); });

  server.on("/getData", HTTP_GET, [](AsyncWebServerRequest *request)
            {
                tempChar.begin();
                tempChar.print("Data,");
                tempChar.print(config.hijriDayCorection);
                tempChar.print(",");
                tempChar.print(config.eqamaFajr);
                tempChar.print(",");
                tempChar.print(config.eqamaShrouq);
                tempChar.print(",");
                tempChar.print(config.eqamaZohr);
                tempChar.print(",");
                tempChar.print(config.eqamaAsr);
                tempChar.print(",");
                tempChar.print(config.eqamaMagrb);
                tempChar.print(",");
                tempChar.print(config.eqamaEsha);
                tempChar.print(",");
                tempChar.print(config.countery);
                tempChar.print(",");
                tempChar.print(config.government);
                tempChar.print(",");
                tempChar.print(config.latitude);
                tempChar.print(",");
                tempChar.print(config.longitude);
                tempChar.print(",");
                tempChar.print(config.calcMethod);
                tempChar.print(",");
                tempChar.print(config.wayMethod);
                tempChar.print(",");
                tempChar.print(config.dst);
                tempChar.print(",");
                tempChar.print(config.sleepPrayerTime);
                tempChar.print(",");
                tempChar.print(config.sleepGomaaTime);
                tempChar.print(",");
                tempChar.print(config.enableBuzzer);
                tempChar.print(",");
                tempChar.print(config.wifiPassword);
                tempChar.print(",");
                tempChar.print(config.summerTime);
                tempChar.print(",");
                tempChar.print(config.sleepAfterEsha);
                tempChar.print(",");
                tempChar.print(config.sleepBeforFajr);
                tempChar.print(",");
                tempChar.print(config.azanSound);
                tempChar.print(",");
                tempChar.print(config.eqamaSound);
                tempChar.print(",");
                tempChar.print(config.soundLevel);
                log("GetData ");
                logln(tempChar);
                request->send(200, "text/plain", blur(tempChar)); });

  server.on("/setDateTime", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              logln("Date Time Setting");
              const char *PARAM_year = "y";
              const char *PARAM_month = "m";
              const char *PARAM_day = "d";
              const char *PARAM_hour = "h";
              const char *PARAM_minute = "mi";
              const char *PARAM_second = "s";
              const char *PARAM_hijriCorrec = "hjc";
              const char *PARAM_summerTime = "st";
              const char *PARAM_dst = "dst";

              int y = 2023, MM = 2, d = 1, h = 0, m = 0, s = 0;
              if (request->hasParam(PARAM_year))
                y = atoi(unBlur(request->getParam(PARAM_year)->value().c_str()));
              if (request->hasParam(PARAM_month))
                MM = atoi(unBlur(request->getParam(PARAM_month)->value().c_str()));
              if (request->hasParam(PARAM_day))
                d = atoi(unBlur(request->getParam(PARAM_day)->value().c_str()));
              if (request->hasParam(PARAM_hour))
                h = atoi(unBlur(request->getParam(PARAM_hour)->value().c_str()));
              if (request->hasParam(PARAM_minute))
                m = atoi(unBlur(request->getParam(PARAM_minute)->value().c_str()));
              if (request->hasParam(PARAM_second))
                s = atoi(unBlur(request->getParam(PARAM_second)->value().c_str()));
              if (request->hasParam(PARAM_hijriCorrec))
                config.hijriDayCorection = atoi(unBlur(request->getParam(PARAM_hijriCorrec)->value().c_str()));
              if (request->hasParam(PARAM_summerTime))
                config.summerTime = atoi(unBlur(request->getParam(PARAM_summerTime)->value().c_str()));
              if (request->hasParam(PARAM_dst))
                config.dst = atoi(unBlur(request->getParam(PARAM_dst)->value().c_str()));

              (setConfig()) ? request->send(200, "text/plain", blur("dateTimeOk")) : request->send(200, "text/plain", blur("Error"));

              DateTime dtTmp = DateTime(y, MM, d, h, m, s);

              log("Income Data ");
              log(dtTmp.year());
              log("-");
              log(dtTmp.month());
              log("-");
              log(dtTmp.day());
              log(" ");
              log(dtTmp.hour());
              log(":");
              log(dtTmp.minute());
              log(":");
              log(dtTmp.second());
              log(" ");
              log(config.hijriDayCorection);
              log(" ");
              logln(config.summerTime);
              log(" ");
              logln(config.dst);

              if (isSummerTime(dtTmp))
                dtTmp = dtTmp - TimeSpan(0, 1, 0, 0);

              rtc.adjust(dtTmp);

              getDateTime();
              getPrayerTimes();
              Hijri();
              
              logln("Date Time Saved"); });

  server.on("/setEqama", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              const char *PARAM_fajr = "f";
              const char *PARAM_shrouq = "s";
              const char *PARAM_zohr = "z";
              const char *PARAM_asr = "a";
              const char *PARAM_magreb = "m";
              const char *PARAM_esha = "e";

              if (request->hasParam(PARAM_fajr))
                config.eqamaFajr = atoi(unBlur(request->getParam(PARAM_fajr)->value().c_str()));
              if (request->hasParam(PARAM_shrouq))
                config.eqamaShrouq = atoi(unBlur(request->getParam(PARAM_shrouq)->value().c_str()));
              if (request->hasParam(PARAM_zohr))
                config.eqamaZohr = atoi(unBlur(request->getParam(PARAM_zohr)->value().c_str()));
              if (request->hasParam(PARAM_asr))
                config.eqamaAsr = atoi(unBlur(request->getParam(PARAM_asr)->value().c_str()));
              if (request->hasParam(PARAM_magreb))
                config.eqamaMagrb = atoi(unBlur(request->getParam(PARAM_magreb)->value().c_str()));
              if (request->hasParam(PARAM_esha))
                config.eqamaEsha = atoi(unBlur(request->getParam(PARAM_esha)->value().c_str()));

              (setConfig()) ? request->send(200, "text/plain", blur("eqamaOk")) : request->send(200, "text/plain", blur("Error"));

              getConfig();

              logln(config.eqamaFajr);
              logln(config.eqamaShrouq);
              logln(config.eqamaZohr);
              logln(config.eqamaAsr);
              logln(config.eqamaMagrb);
              logln(config.eqamaEsha);

              logln("Eqama Time Saved"); });

  server.on("/setLocation", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              const char *PARAM_countery = "c";
              const char *PARAM_government = "g";
              const char *PARAM_latitude = "lat";
              const char *PARAM_longitude = "lon";

              if (request->hasParam(PARAM_countery))
                config.countery = atoi(unBlur(request->getParam(PARAM_countery)->value().c_str()));
              if (request->hasParam(PARAM_government))
                config.government = atoi(unBlur(request->getParam(PARAM_government)->value().c_str()));
              if (request->hasParam(PARAM_latitude))
                config.latitude = atof(unBlur(request->getParam(PARAM_latitude)->value().c_str()));
              if (request->hasParam(PARAM_longitude))
                config.longitude = atof(unBlur(request->getParam(PARAM_longitude)->value().c_str()));

              (setConfig()) ? request->send(200, "text/plain", blur("locationOk")) : request->send(200, "text/plain", blur("Error"));

              getConfig();

              logln(config.countery);
              logln(config.government);
              logln(config.latitude);
              logln(config.longitude);

              getPrayerTimes();

              logln("Location Saved"); });

  server.on("/setSettings", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              const char *PARAM_calcMethod = "cm";
              const char *PARAM_wayMethod = "wm";
              const char *PARAM_sleepPrayerTime = "spt";
              const char *PARAM_sleepGomaaTime = "sgt";
              const char *PARAM_sleepAfterEsha = "sae";
              const char *PARAM_sleepBeforeFajr = "sbf";
              const char *PARAM_enableBuzzer = "buz";
              const char *PARAM_AZAN = "azs";
              const char *PARAM_EQAMA = "eqs";
              const char *PARAM_SOUND_LEVEL = "sl";

              if (request->hasParam(PARAM_calcMethod))
                config.calcMethod = atoi(unBlur(request->getParam(PARAM_calcMethod)->value().c_str()));
              if (request->hasParam(PARAM_wayMethod))
                config.wayMethod = atoi(unBlur(request->getParam(PARAM_wayMethod)->value().c_str()));
              if (request->hasParam(PARAM_sleepPrayerTime))
                config.sleepPrayerTime = atoi(unBlur(request->getParam(PARAM_sleepPrayerTime)->value().c_str()));
              if (request->hasParam(PARAM_sleepGomaaTime))
                config.sleepGomaaTime = atoi(unBlur(request->getParam(PARAM_sleepGomaaTime)->value().c_str()));
              if (request->hasParam(PARAM_sleepAfterEsha))
                config.sleepAfterEsha = atoi(unBlur(request->getParam(PARAM_sleepAfterEsha)->value().c_str()));
              if (request->hasParam(PARAM_sleepBeforeFajr))
                config.sleepBeforFajr = atoi(unBlur(request->getParam(PARAM_sleepBeforeFajr)->value().c_str()));
              if (request->hasParam(PARAM_enableBuzzer))
                config.enableBuzzer = atoi(unBlur(request->getParam(PARAM_enableBuzzer)->value().c_str()));
              if (request->hasParam(PARAM_AZAN))
                config.azanSound = atoi(unBlur(request->getParam(PARAM_AZAN)->value().c_str()));
              if (request->hasParam(PARAM_EQAMA))
                config.eqamaSound = atoi(unBlur(request->getParam(PARAM_EQAMA)->value().c_str()));
              if (request->hasParam(PARAM_SOUND_LEVEL))
                config.soundLevel = atoi(unBlur(request->getParam(PARAM_SOUND_LEVEL)->value().c_str()));

              (setConfig()) ? request->send(200, "text/plain", blur("settingsOk")) : request->send(200, "text/plain", blur("Error"));

              getConfig();

              logln(config.calcMethod);
              logln(config.wayMethod);
              logln(config.sleepPrayerTime);
              logln(config.sleepGomaaTime);
              logln(config.sleepAfterEsha);
              logln(config.sleepBeforFajr);
              logln(config.enableBuzzer);
              logln(config.azanSound);
              logln(config.eqamaSound);
              logln(config.soundLevel);

              mp3.setVolume(config.soundLevel);

              // testSoundMode = true;

              // mp3.reset();
              // mp3.setVolume(config.soundLevel);
              // mp3.playFileByIndexNumber(7);

              getPrayerTimes();

              logln("Settings Saved"); });

  server.on("/setPassword", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              const char *PARAM_password = "p";

              if (request->hasParam(PARAM_password))
                config.wifiPassword = atoi(unBlur(request->getParam(PARAM_password)->value().c_str()));

              (setConfig()) ? request->send(200, "text/plain", blur("passwordOk")) : request->send(200, "text/plain", blur("Error"));

              getConfig();

              logln(config.wifiPassword);

              logln("Password Saved"); });

  server.on("/testMode", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              request->send(200, "text/plain", blur("testModeOk"));

              isTestMode = true;

              logln("Test Mode Active"); });

  server.on("/resetDefault", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              request->send(200, "text/plain", blur("resetDefaultOk"));

              resetDefault();

              logln("Test Mode Active"); });

  server.on("/updateMain", HTTP_POST, [](AsyncWebServerRequest *request)
            {
              isSleepNow = true;
              logln("update Request");
              request->send(200); }, httpProcessUpdate);

  server.begin();
}

void closeAllWifiServices()
{
  WiFi.disconnect(true);              // Disconnect from any connected network
  WiFi.mode(WIFI_OFF);                // Turn off Wi-Fi mode
  WiFi.softAPdisconnect(true);        // Disable WiFi access point mode
  WiFi.setSleepMode(WIFI_NONE_SLEEP); // Disable WiFi sleep mode
  server.end();
  MDNS.end();
  dnsServer.stop();
}

/*
void getMemory()
{
  log("Memory >> ");
  logln(ESP.getFreeHeap());
  log("Total >> ");
  logln(ESP.getFlashChipSize());
  HeapSelectIram ephemeral;
  Serial.printf("IRAM free: %6d bytes\r\n", ESP.getFreeHeap());
  {
    HeapSelectDram ephemeral;
    Serial.printf("DRAM free: %6d bytes\r\n", ESP.getFreeHeap());
  }

}*/

/*
void loading()
{
  logln(">> loading");
  isLoadingValues = !isLoadingValues;

  log("Loading ");
  logln(isLoadingValues);

  digitalWrite(latchPin, LOW);
  for (int i = 0; i < 35; i++)
  {
    shiftOut(dataPin, clockPin, LSBFIRST, segments[10]);
  }
  if (isLoadingValues)
  {
    shiftOut(dataPin, clockPin, LSBFIRST, segments[12]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[11]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[12]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[11]);
  }
  else
  {
    shiftOut(dataPin, clockPin, LSBFIRST, segments[11]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[12]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[11]);
    shiftOut(dataPin, clockPin, LSBFIRST, segments[12]);
  }
  // digitalWrite(latchPin, LOW);
  digitalWrite(latchPin, HIGH);
}
*/

void setup()
{
  if (DEBUG == 1)
    Serial.begin(115200);
  logln("Setup Start");

  ESP.wdtDisable();
  // system_update_cpu_freq(160);
  // system_update_cpu_freq(16);

  // aes_init(); // generate random IV, should be called only once? causes crash if repeated...

  sprintf(DEVICE_NAME, "Basit-Masjed-%u", ESP.getChipId());

  logln("");
  log("Startup reason:");
  logln(ESP.getResetReason());

  closeAllWifiServices();

  pinMode(dataPin, OUTPUT);
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);

  splashScreen();

  // LittleFS.format();
  intiLittleFS();
  ListOfFiles();

  if (!getConfig())
    resetDefault();
  
  startMP3();
  
  Wire.begin(3, 0);
  delay(200);

  if (!rtc.begin())
  {
    logln("RTC DisConnect");
    isRtcConnected = false;
  }
  else
  {
    logln("RTC Connected");
    isRtcConnected = true;
  }
  dt = rtc.now();
  if (dt.year() < 2023 || dt.year() > 2100)
    rtc.adjust(DateTime(2023, 2, 1, 1, 0, 0));
  getDateTime();

  if (rtc.lostPower())
    logln("RTC Lost Power");

  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  // rtc.adjust(DateTime(2000, 9, 10, 15, 36, 50));
  // rtc.adjust(DateTime(2022, 2, 11, 12, 7, 55)); // azan last Minute
  // rtc.adjust(DateTime(2022, 2, 11, 12, 19, 55)); // eqama last minute

  getPrayerTimes();

  sensors.begin();
  sensors.setWaitForConversion(false);
  sensors.requestTemperatures();

  getTemp();

  initAccessPoint();
  serverInit();

  getTemp();

  // btn.setPressTicks(3000);
  // btn.attachLongPressStart(resetAndRestart);
  closeWiFiTimer = millis();
}

void loop()
{
  // btn.tick();

  if (wifiStatus)
  {
    dnsServer.processNextRequest();
    MDNS.update();
    // Process DHCP requests
    // dhcpServer.loop();
  }

  if (testSoundMode)
  {
    logln("Test Sound Mode Start");
    log("Sound Level ");
    logln(config.soundLevel);

    mp3.setVolume(config.soundLevel);
    // mp3.playFileByIndexNumber(8);
    testSoundMode = false;
  }

  //=== Splash Screen For First Time Start =========
  if (isSplashScreen && splashTime < millis())
  {
    logln("===End Splash===");
    isSplashScreen = false;
  }

  //=== Stop Access Point After 10 Minutes =========
  if ((closeWiFiTimer + (10 * 60) * 1000) < millis() && wifiStatus && (!isUploadUpdate) && DEBUG == 0)
  {
    closeAllWifiServices();
    wifiStatus = false;
  }

  //=== Half Second Timer ===========================
  if ((millis() - halfSecTimer > 500) && !isRestartNow && !isSplashScreen && !isUploadUpdate && !isTestMode && !isUpdateNow)
  {
    show();
    halfSecTimer = millis();
  }

  //=== One Second Timer ===========================
  if ((millis() - oneSecTimer > 1000) && !isRestartNow && !isSplashScreen && !isUploadUpdate && !isUpdateNow)
  {
    if (!isUpdateNow)
    {
      logln("=== One Second Timer ===");

      getDateTime();

      if (isTestMode) // Test All 7 segments
      {
        logln("Test Mode ");
        // Change Count Every 5 Seconds
        if (dt.second() % 5 == 0)
          testMode();
      }
      else
      {
        if ((dt.second() == 1) || (dt.second() == 30))
        {
          getPrayerTimes();
          getTemp();
        }

        Checking();

        // getMemory();

        log("Date ");
        log(dt.day());
        log("-");
        log(dt.month());
        log("-");
        log(dt.year());
        // log("  ");
        // log(dt.hour());
        // log(":");
        // log(dt.minute());
        // log(":");
        // logln(dt.second());
        // log("Hijr ");
        // log(var.hDay);
        // log("-");
        // log(var.hMonth);
        // log("-");
        // logln(var.hYear);
      }
      oneSecTimer = millis();
    }
  }

  //=== IF Restart ===========================
  if (isRestartNow && (millis() > restartMillis + 3000))
  {
    logln("Restart Now");
    ESP.restart();
  }
}