
#define DEBUG 1   // 1 to Enable Debug Mod 0 to Disable
#define VERSION 4 // Firmware Version Number

#define dataPin 12
#define latchPin 13
#define clockPin 14

#define buzzerPin 5
// #define buttonPin 2
#define tempSensorPin 4

#define mainIP IPAddress(192, 168, 1, 177) // Gateway IP Address
char DEVICE_NAME[50];

// Common Cathod
int segments[] = {
    B11111110, // 0
    B10011000, // 1
    B11101101, // 2
    B10111101, // 3
    B10011011, // 4
    B10110111, // 5
    B11110111, // 6
    B10011100, // 7
    B11111111, // 8
    B10111111, // 9
    B10000000, // 10 OFF ALL
    B10000001, // 11 -
    B10100000, // 12 _
    B11100111  // 13 E
};
// Common Anode
// int Segments[] = {
//     // E D C B A F G
//     B11111110, // 0
//     B10011000, // 1
//     B11101101, // 2
//     B10111101, // 3
//     B10011011, // 4
//     B10110111, // 5
//     B11110111, // 6
//     B10011100, // 7
//     B11111111, // 8
//     B10111111, // 9
//     B10000000, // 10 OFF ALL
//     B10000001, // 11 -
//     B10100000, // 12 _
//     B11100111  // 13 E
// };

typedef struct Configration
{
    int calcMethod;
    int wayMethod;
    float latitude;
    float longitude;
    int dst;
    int hijriDayCorection;
    int eqamaFajr;
    int eqamaShrouq;
    int eqamaZohr;
    int eqamaAsr;
    int eqamaMagrb;
    int eqamaEsha;
    int enableBuzzer = 1;
    int sleepGomaaTime;
    int sleepPrayerTime;
    int countery;
    int government;
    int wifiPassword;
    int summerTime = 0;
    int sleepAfterEsha = 0;
    int sleepBeforFajr = 0;
    int azanSound ;
    int eqamaSound;
    int soundLevel;
} Configration;
Configration config;

struct var
{
    int day = 0;
} var;

struct Data
{
    int minA, minB, horA, horB, secA, secB, dayA, dayB, monA, monB, yeaA, yeaB, yeaC, yeaD, fajH, fajA, fajB, shrH, shrA, shrB, zohHA, zohHB, zohMA, zohMB, asrH, asrMA, asrMB, magH, magMA, magMB, eshHA, eshHB, eshMA, eshMB, eqaA = 10, eqaB = 10, tmpA = 10, tmpB = 10;
} data;