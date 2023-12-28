/* original design by niq_ro: https://github.com/tehniq3/
 * v.1 - test for have a stable version
 * v.2.0 - changed almost all.. e.g from FastLED to Adafruit_NeoMatrix libraries
 * v.2.1 - added weather info (scrolling) from openweathermap
 * v.2.1.a - compute lengh of string + extract more info from opemweathermap data
 * v.2.2 - sunset/sunrise time: not used anymore data from SolaCalculator library, used opemweathermap data
 * v.2.3 - more minutes between weather info + deep night sleep (no info)
 * v.2.4 - added date
 * v.2.5 - added smoth transition for clock
 * v.2.6 - after extra-info show correct clock
 * v.2.7 - if no clear weather data received, show just simple info (date)
 * v.2.8 - added binary clock decimal as at https://en.wikipedia.org/wiki/Binary-coded_decimal
 * v.2.9 - ported to RPi Pico W (removed WiFimanager library)
 * v.2.9a - check "stack protector = enable" when upload
 * v.2.10 - added HW watchdog as at https://github.com/tehniq3/HW_watchdog/
 * v.2.11 - removed long delays and used  small delays, millis(), while, etc
 * v.2.12 - replaced pwm with digitalWrite(pin, millis()%2), changed pin for display from GP14 in GP15, DST pin from GP12to GP14 + added internal led as seconds beat
 * v.2.13 - show wifi status with red light in upper-left side, show weather connexion status
 * v.2.13a - prepare to reset system at 4:55 before weather info display again (no implement ok)
 * v.2.14 - changed ESP8266WiFi with WiFi (ESP32) libraries 
 * v.2.15 - return to SolarCalculator library for control the brigthness, hide older weather info than x hours
 * v.2.16 - reset system at 4:55 before weather info display again + added hour of update in extrainfo + show as purple if to much time without info
 * v.2.17 - correct day variable (zi2 instead zi) for sunset/sunrise + cleand the sketch
 * v.2.18 - split request in 2 requests: hour / weather as at https://github.com/tehniq3/NTPclock_RP2040_ESP8266_01/blob/main/NTP_weatherstation_RP2040_ESP8266_i2c_1602_v3_6/NTP_weatherstation_RP2040_ESP8266_i2c_1602_v3_6.ino
            check hour/year at first check (after power on / reset), no ok, retry ans retry..
 * v.2.18a - small changes at variables and timmings            
*/

#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>  // https://github.com/adafruit/Adafruit_NeoMatrix
#include <Adafruit_NeoPixel.h>  
#ifndef PSTR
 #define PSTR // Make Arduino Due happy
#endif
/*
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
*/
#include <WiFi.h>
#include <WiFiMulti.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <WebServer.h>

//#include <Wire.h>
#include <SolarCalculator.h> //  https://github.com/jpb10/SolarCalculator
#include <ArduinoJson.h>

#define DSTpin 14 // GPIO14 = D6, see https://randomnerdtutorials.com/esp8266-pinout-reference-gpios/
#define PIN 15 // D5 = GPIO14  https://randomnerdtutorials.com/esp8266-pinout-reference-gpios/

#define initial 21
#define piviem  20

// Replace with your network credentials
const char *ssid     =  "reteanet";
const char *password =  "parola1234!";

// MATRIX DECLARATION:
// Parameter 1 = width of the matrix
// Parameter 2 = height of the matrix
// Parameter 3 = pin number (most are valid)
// Parameter 4 = matrix layout flags, add together as needed:
//   NEO_MATRIX_TOP, NEO_MATRIX_BOTTOM, NEO_MATRIX_LEFT, NEO_MATRIX_RIGHT:
//     Position of the FIRST LED in the matrix; pick two, e.g.
//     NEO_MATRIX_TOP + NEO_MATRIX_LEFT for the top-left corner.
//   NEO_MATRIX_ROWS, NEO_MATRIX_COLUMNS: LEDs are arranged in horizontal
//     rows or in vertical columns, respectively; pick one or the other.
//   NEO_MATRIX_PROGRESSIVE, NEO_MATRIX_ZIGZAG: all rows/columns proceed
//     in the same order, or alternate lines reverse direction; pick one.
//   See example below for these values in action.
// Parameter 5 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_GRBW    Pixels are wired for GRBW bitstream (RGB+W NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)

Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(32, 8, PIN,
  NEO_MATRIX_TOP     + NEO_MATRIX_LEFT +
  NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
  NEO_GRB            + NEO_KHZ800);   // https://www.adafruit.com/product/2294

const uint16_t colors[] = {
  matrix.Color(255, 0, 0),    // red
  matrix.Color(0, 255, 0),    // green
  matrix.Color(255, 255, 0),  // yellow
  matrix.Color(0, 255, 255),  // bleu (light blue) 
  matrix.Color(255, 0, 255),  // mouve
  matrix.Color(0, 255, 255),  // blue
  matrix.Color(255, 255, 255) // white 
  };

// WS2812b day / night brightness.
#define NIGHTBRIGHTNESS 2      // Brightness level from 0 (off) to 255 (full brightness)
#define DAYBRIGHTNESS 6

const long timezoneOffset = 2; // ? hours
const char          *ntpServer  = "pool.ntp.org"; // change it to local NTP server if needed
const unsigned long updateDelay = 60000; //900000;         // update time every 15 min
const unsigned long retryDelay  = 5000; //300000;         // retry 5 min later if time query failed

//weather variables
WiFiClient client;
char servername[]="api.openweathermap.org";              // remote weather server we will connect to
String result;
String APIKEY = "ca55295c4a4b9a681dce2688e0751dde"; // https://home.openweathermap.org/api_keys                            
String CityID = "680332"; // Craiova city                       
int  fetchweatherflag = 0; 
String weatherDescription ="";
String weatherLocation = "";
String Country;
float temperatura=0.0;
int tempint, temprest;
float tempmin, tempmax;
int umiditate;
int presiune;
/*
unsigned long tpafisare;
unsigned long tpinfo = 60000;  // 15000 for test in video
*/
unsigned long tpvreme;
unsigned long tpinterogare1 = 20*60000;  // 20 minute

unsigned long tpceas;
unsigned long tpinterogare0 = 500;

unsigned long tpsoare;
unsigned long tpinterogare2 = 2*60000;   // 2 minute

unsigned long lastUpdatedTime = updateDelay * -1;
unsigned int  second_prev = 0;
bool          colon_switch = false;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer);

byte DST = 1;
byte DST0;
bool updated;
byte a = 0;

String text = "NTP clock (classic and binary-decimal) with weather data using Raspberry Pi Pico W (Arduino IDE) by niq_ro on 8x32 addressable led display !";
int ltext = 6*71;  // lengh of string
int x;

int ora = 20;
int minut = 24;
int secundar = 0;
int zi, zi2, luna, an;

byte aratadata = 1;
byte aratadata0 = 3;
byte culoare = 1;

int vant, directie, nori;
unsigned long unixora, unixora1, unixrasarit, unixapus;
String descriere;
byte dn = 0;

byte night1 = 1;  
byte night2 = 5;
byte noinfo = 0;
byte noinfo0 = 3;
byte minuteclock = 5;  // minutes between scroll the onfo

//Week Days
String weekDays[7]={"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

//Month names
String months[12]={"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};

const int binare[] = {  7,  8, 23, 24, 39, 40, 55, 56, 71, 72,
                       87, 88,103,104,119,120,135,136,151,152,
                      167,168,183,184,199,200,215,216,231,232,
                      247,248}; 

const uint16_t culbin[6] = {  0,  0,255,
                            255,255,255};

byte igrec;
byte wificon = 0;
byte vremecon = 0;

// Location - Craiova: 44.317452,23.811336
double latitude = 44.31;
double longitude = 23.81;
double transit, sunrise, sunset;
int ora1, ora2, oraactuala;
int m1, hr1, mn1, m2, hr2, mn2; 
int oravr, minvr;

unsigned long tppreluatvreme;
unsigned long tpfortare = 6*3600000; // x hours without wether info 

byte liber = 3;
byte liber0 = 7;
int an2  = 0;
int zi00, zi20, luna0, an0;
unsigned long unixora0;

void setup() {
  digitalWrite(initial, HIGH);
  pinMode(initial, OUTPUT);
  digitalWrite(initial, HIGH);
  pinMode(piviem, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  analogWriteFreq(1000);  // / Adjust PWM properties if needed
  
  Serial.begin(115200);
  Serial.println("_");
  pinMode (DSTpin, INPUT);

  matrix.begin();
  matrix.setTextWrap(false);
  matrix.setBrightness(NIGHTBRIGHTNESS);  // original 255
  matrix.setTextColor(colors[6]);
  matrix.fillScreen(0);
matrix.setCursor(0, 0);
matrix.print(F("v2.18"));
//matrix.setPixelColor(255, 0, 150, 0);  // pixel, red, green, blue level (0..255)
matrix.show();
//delay(500);  // comment to decrease time to recovery
/*
x    = matrix.width();
ltext = 6*text.length();
matrix.fillScreen(0);
  for (x; x > -ltext ; x--)
  {
  matrix.fillScreen(0); 
  matrix.setCursor(x, 0);
  matrix.print(text);  
  matrix.show();
  delay(50);
  //Serial.println(x);
 }
*/

  // Connect to Wi-Fi
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);  // https://randomnerdtutorials.com/esp32-useful-wi-fi-functions-arduino/
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    wificon = 1;
    matrix.setPixelColor(0, 150, 0, 0);
    matrix.show();
    delay(1000);
    Serial.print(".");
  }
  wificon = 2;
  Serial.println(WiFi.localIP());
  Serial.print("RRSI: ");
  Serial.println(WiFi.RSSI());

  if (digitalRead(DSTpin) == LOW)
   DST = 0;
  else
   DST = 1;

  timeClient.setTimeOffset((timezoneOffset + DST)*3600);
  timeClient.begin();
  delay(3000);  
  while (an2 < 1971)
{          
  updated = timeClient.update();
  delay(1000);
  DST0 = DST;
ora = timeClient.getHours();
minut = timeClient.getMinutes();
secundar = timeClient.getSeconds();
getYear();
getMonth();
getDay();
zi = timeClient.getDay(); 

matrix.setPixelColor(127, 150, 150, 0);  // pixel, red, green, blue level (0..255)
delay(1000);
matrix.setPixelColor(127, 0, 150, 0);  // pixel, red, green, blue level (0..255)
delay(500);
matrix.show();
   }
delay(1000); // wait
getWeatherData();
Soare();
night();

if (dn == 0)
      {
        matrix.setBrightness (NIGHTBRIGHTNESS);
        Serial.println("NIGHT !");
      }
      else
      {
      matrix.setBrightness (DAYBRIGHTNESS);
      Serial.println("DAY !");
      }
    matrix.show();

analogWrite(piviem, millis()%2);
delay(1000);
digitalWrite(initial, LOW);
    
tpsoare = millis();
tpvreme = millis();
lastUpdatedTime = millis();            
}

void loop() {
  if (digitalRead(DSTpin) == LOW)
   DST = 0;
  else
   DST = 1;

  while (liber == 1) 
  { 
//  if (WiFi.status() == WL_CONNECTED && millis() - lastUpdatedTime >= updateDelay) 
  if (WiFi.status() == WL_CONNECTED)
  {
    updated = timeClient.update();
    if (updated) {
      //matrix.setPixelColor(0, 0, 150, 0);
      //matrix.show();
      wificon = 2;
      digitalWrite(piviem, millis()%2);
      Serial.println("NTP time updated.");
      //Serial.print(timeClient.getFormattedTime());
      //Serial.print(" ->");
      //Serial.println(timeClient.getEpochTime());
      unixora = timeClient.getEpochTime();
      //Serial.print(" =? ");
      Serial.println(unixora);
      getYear();
      getMonth();
      getDay();
      zi = timeClient.getDay();
      lastUpdatedTime = millis();
      if (an2 > 2000)
      {
        //int zi00, zi20, luna0, an0;
        unixora0 = unixora;
        zi = zi00;
        zi2 =zi20;
        luna0 - luna;
        an0 = an;        
      }
      
      Soare();
      night();
      if (dn == 0)
      {
        matrix.setBrightness (NIGHTBRIGHTNESS);
        Serial.println("NIGHT !");
      }
      else
      {
      matrix.setBrightness (DAYBRIGHTNESS);
      Serial.println("DAY !");
      }
      matrix.show();
      liber = 3;
    } else {
      wificon = 1;
      Serial.println("Failed to update time. Waiting for retry...");
  //     lastUpdatedTime = millis();
      lastUpdatedTime = millis() + retryDelay;
   //   lastUpdatedTime = millis() - updateDelay + retryDelay + 3000;
      Serial.println(lastUpdatedTime);
      liber = 1;
      Serial.println("+10s");
      digitalWrite(piviem, millis()%2);
      delay(1);
    }
  } else {
    if (WiFi.status() != WL_CONNECTED) //Serial.println("WiFi disconnected!");
    {
    wificon = 0;
    Serial.println("No wi-Fi connexion :(((");
    
    lastUpdatedTime = millis() - updateDelay + retryDelay;
    digitalWrite(piviem, millis()%2);
    }
  }
  }  // end   while "liber = 1"

while (liber == 2)
{  
//if (millis() - tpvreme >= tpinterogare1)
//{
digitalWrite(piviem, millis()%2);
getWeatherData();
tpvreme = millis();  
Serial.println("Weather site was check ? Maybe...");
//}
if (millis() - tppreluatvreme > tpfortare)  // x hours without weather info
{
 Serial.println("Too much time without any weather info..."); 
// delay(10000);
 vremecon = 4;  // show too long time no info
}
liber = 3;
}  // end   while "liber = 2"

if (millis() - tpceas >= tpinterogare0)
{
digitalWrite(piviem, millis()%2);
ora = timeClient.getHours();
minut = timeClient.getMinutes();
secundar = timeClient.getSeconds();
if (zi2 == 0)
getDay();
tpceas = millis(); 
}

String ceas = "";
if (ora/10 == 0) 
  ceas = ceas + "0";
  ceas = ceas + ora;
  if (secundar%2 == 0)
    ceas = ceas + ":";
  else
    ceas = ceas + " ";
if (minut/10 == 0) 
  ceas = ceas + "0";
  ceas = ceas + minut;
int lceas = 6*ceas.length(); // https://reference.arduino.cc/reference/en/language/variables/data-types/string/functions/length/


matrix.fillScreen(0);
matrix.setTextColor(colors[culoare]);
matrix.setCursor(1, 0);
matrix.print(ceas);
//matrix.setPixelColor(255, matrix.Color(0, 150, 0));
ceasbinar();   
matrix.show();

if (DST != DST0)
{
  timeClient.setTimeOffset((timezoneOffset + DST)*3600);
  timeClient.begin();
DST0 = DST;
}

String extrainfo = " ";
extrainfo = extrainfo + weekDays[zi];
extrainfo = extrainfo + ", ";
if (zi2/10 == 0) 
  extrainfo = extrainfo + "0";
  extrainfo = extrainfo + zi2;
  extrainfo = extrainfo + ".";
/*
if (luna/10 == 0) 
  extrainfo = extrainfo + "0";
  extrainfo = extrainfo + luna;
*/
extrainfo = extrainfo + months[luna-1];
  extrainfo = extrainfo + ".";
  extrainfo = extrainfo + an;

if (umiditate > 1.)
{
//String extrainfo = ", Weather: ";
if (dn == 1)
   extrainfo = extrainfo + ", Day";
   else
   extrainfo = extrainfo + " Night";
if (millis() - tppreluatvreme < tpfortare)  // x hours without weather info
{
extrainfo = extrainfo + ", info (";
extrainfo = extrainfo + oravr/10;
extrainfo = extrainfo + oravr%10;
extrainfo = extrainfo + ":";
extrainfo = extrainfo + minvr/10;
extrainfo = extrainfo + minvr%10;    
extrainfo = extrainfo + "): ";  
extrainfo = extrainfo + descriere;
if (nori > 0) 
extrainfo = extrainfo + " (clouds: " + nori + "%)";
extrainfo = extrainfo + ", temperature: ";
if (temperatura > 0) extrainfo = extrainfo + "+";
extrainfo = extrainfo + tempint + ","+ temprest;
extrainfo = extrainfo + ((char)247) + "C, humidity: ";  // https://forum.arduino.cc/t/degree-symbol/166709/9
/*
extrainfo = extrainfo + ((char)247) + "C (";
extrainfo = extrainfo + tempmin + "/";
extrainfo = extrainfo + tempmax + "), humidity: ";
*/
extrainfo = extrainfo + umiditate;
extrainfo = extrainfo + "%RH, pressure: ";
extrainfo = extrainfo + presiune + "mmHg, wind: ";
extrainfo = extrainfo + vant + "km/h (";
extrainfo = extrainfo + directie +  ((char)247) + ") ";
}
else
{
extrainfo = extrainfo + "... ";  
}
}
//int ltempe = 6*56;  // 6*72;
int lextrainfo = 6*extrainfo.length(); // https://reference.arduino.cc/reference/en/language/variables/data-types/string/functions/length/

if (noinfo0 != noinfo)
{
Serial.print("noinfo = ");
Serial.println(noinfo);
noinfo0 = noinfo;
}
if (aratadata0 != aratadata)
{
Serial.print("aratadata = ");
Serial.println(aratadata);
aratadata0 = aratadata;
}

if ((noinfo != 1) and (aratadata == 1))
{
 // x = 32;
if ((secundar > 35) and (minut%minuteclock == 0)) // every "minuteclock" minutes
{
//matrix.fillScreen(0);
  x = 1;
  for (x; x > -lceas ; x--)
  {
  matrix.fillScreen(0); 
  matrix.setCursor(x, 0);
  matrix.print(ceas); 
  ceasbinar(); 
  matrix.show();  
//  delay(75);
  igrec = 0;
  while (igrec < 75)
  {
   digitalWrite(piviem, millis()%2);
   delay(1);
   digitalWrite(piviem, millis()%2);
   igrec++;
  }
 }
 
  x = 32;
  for (x; x > -lextrainfo ; x--)
  {
  matrix.fillScreen(0); 
  matrix.setCursor(x, 0);
  matrix.print(extrainfo);  
  ceasbinar();
  matrix.show();
//  delay(75);
   igrec = 0;
  while (igrec < 75)
  {
   digitalWrite(piviem, millis()%2);
   delay(1);
   digitalWrite(piviem, millis()%2);
   igrec++;
  }
//  //Serial.println(x);
 }
  aratadata = 0; 
  culoare++;
  if (culoare > 4) culoare = 0;

ora = timeClient.getHours();
minut = timeClient.getMinutes();
String ceas1 = "";
if (ora/10 == 0) 
  ceas1 = ceas1 + "0";
  ceas1 = ceas1 + ora;
  if (secundar%2 == 0)
    ceas1 = ceas1 + ":";
  else
    ceas1 = ceas1 + " ";
if (minut/10 == 0) 
  ceas1 = ceas1 + "0";
  ceas1 = ceas1 + minut;
int lceas1 = 6*ceas1.length(); // https://reference.arduino.cc/reference/en/language/variables/data-types/string/functions/length/

matrix.setTextColor(colors[culoare]);
  x = 32;
  for (x; x > -lceas1+30 ; x--)
  {
  matrix.fillScreen(0); 
  matrix.setCursor(x, 0);
  matrix.print(ceas1);  
  ceasbinar();   
  matrix.show();
//  delay(75);
  igrec = 0;
  while (igrec < 75)
  {
   digitalWrite(piviem, millis()%2);
   delay(1);
   digitalWrite(piviem, millis()%2);
   igrec++;
  }
 }  
}
}

if (millis() - tpsoare >= tpinterogare2)
{
if ((ora >= night1) and (ora < night2))
{
  Serial.println("no info");
  noinfo = 1;
}
else
   noinfo = 0;  
Soare();
night();
      if (dn == 0)
      {
        matrix.setBrightness (NIGHTBRIGHTNESS);
        Serial.println("check: NIGHT !");
      }
      else
      {
      matrix.setBrightness (DAYBRIGHTNESS);
      Serial.println("check: DAY !");
      }
      matrix.show();
tpsoare = millis();  
}

if ((ora == (night2-1)) and (minut == 55))  // force restart to clear buffers
{
  if (secundar < 3)
  delay(5000);
}

if ((secundar == 0) and (minut%minuteclock == (minuteclock-1))) // change option last minute
{
 //Serial.println("-");
  aratadata = 1;
 }
//digitalWrite(piviem, 0);
delay(1);
digitalWrite(piviem, millis()%2);
digitalWrite(LED_BUILTIN, secundar%2);

if (liber0 != liber)
{
  Serial.println("================ ");
  Serial.print("liber = ");
  Serial.println(liber);
  liber0 = liber;
}
  if ((liber == 3) and (millis() - lastUpdatedTime >= updateDelay)) 
  {
    liber = 1;
  }
  if ((liber >= 3) and (millis() - tpvreme >= tpinterogare1))
  {
    liber = 2;
  }


}  // end main loop


void getYear() {
  time_t rawtime = timeClient.getEpochTime();
  struct tm * ti;
  ti = localtime (&rawtime);
  an = ti->tm_year + 1900;
  an2 = an;
  //Serial.print("year = ");
  //Serial.println(an);
}

void getMonth() {
  time_t rawtime = timeClient.getEpochTime();
  struct tm * ti;
  ti = localtime (&rawtime);
  luna = ti->tm_mon + 1;
  //Serial.print("month = ");
  //Serial.println(luna);
}

void getDay() {
  time_t rawtime = timeClient.getEpochTime();
  struct tm * ti;
  ti = localtime (&rawtime);
  zi2 = ti->tm_mday;
  //Serial.print("Month day: ");
  //Serial.println(zi2);
}

void getWeatherData()     //client function to send/receive GET request data.
{
  if (client.connect(servername, 80))   
          {                         //starts client connection, checks for connection
          client.println("GET /data/2.5/weather?id="+CityID+"&units=metric&APPID="+APIKEY);
          client.println("Host: api.openweathermap.org");
          client.println("User-Agent: ArduinoWiFi/1.1");
          client.println("Connection: close");
          client.println();
          //matrix.setPixelColor(3, 0, 150, 0);
          //matrix.show();
          vremecon = 1;
          } 
  else {
         Serial.println("connection failed");        //error message if no client connect
          //Serial.println();
          //matrix.setPixelColor(3, 150, 0, 0);
          //matrix.show();
          vremecon = 0;
       }

  while(client.connected() && !client.available()) 
  // delay(1);  
  yield();//waits for data
  while (client.connected() || client.available())    
       {                                             //connected or data available
         yield(); // https://stackoverflow.com/questions/75228952/soft-wdt-reset-on-esp8266-when-using-speed-sensor
         char c = client.read();                     //gets byte from ethernet buffer
         result = result+c;
         vremecon = 2;
         tppreluatvreme = millis();
         oravr = ora;
         minvr = minut;
       }

client.stop();                                      //stop client
result.replace('[', ' ');
result.replace(']', ' ');
//Serial.println(result);
char jsonArray [result.length()+1];
result.toCharArray(jsonArray,sizeof(jsonArray));
jsonArray[result.length() + 1] = '\0';
//StaticJsonDocument<1024> json_buf;
StaticJsonBuffer<1024> json_buf;  // v.5
//deserializeJson(root, jsonArray);
JsonObject &root = json_buf.parseObject(jsonArray); // v.5

if (!root.success())
  {
    //Serial.println("parseObject() failed");
  }

String location = root["name"];
String icon = root["weather"]["icon"];    // "weather": {"id":800,"main":"Clear","description":"clear sky","icon":"01n"} 
String id = root["weather"]["description"]; 
String description = root["weather"]["description"];
int clouds = root["clouds"]["all"]; // "clouds":{"all":0},
String country = root["sys"]["country"];
float temperature = root["main"]["temp"];
float temperaturemin = root["main"]["temp_min"];
float temperaturemax = root["main"]["temp_max"];
float humidity = root["main"]["humidity"];
String weather = root["weather"]["main"];
//String description = root["weather"]["description"];
float pressure = root["main"]["pressure"];
float wind = root["wind"]["speed"];//"wind":{"speed":2.68,"deg":120},
int deg = root["wind"]["deg"];

// "sys":{"type":2,"id":50395,"country":"RO","sunrise":1683428885,"sunset":1683480878},
unixrasarit = root["sys"]["sunrise"];
unixapus = root["sys"]["sunset"];

//weatherDescription = description;
weatherDescription = weather;
weatherLocation = location;
Country = country;
temperatura = temperature;
tempint = (int)(10*temperatura)/10;
temprest = (int)(10*temperatura)%10;
tempmin = temperaturemin;
tempmax = temperaturemax;
umiditate = humidity;
presiune = pressure*0.75006;  // mmH20
vant = (float)(3.6*wind+0.5); // km/h
directie = deg;
nori = clouds;
descriere = description;

//Serial.print(weather);
//Serial.print(" / ");
//Serial.println(description);
//Serial.print(temperature);
//Serial.print("°C / ");  // as at https://github.com/tehniq3/datalloger_SD_IoT/
//Serial.print(umiditate);
//Serial.print("%RH, ");
// check https://github.com/tehniq3/matrix_clock_weather_net/blob/master/LEDMatrixV2ro2a/weather.ino
//Serial.print(presiune);
//Serial.print("mmHg / wind = ");
//Serial.print(vant);
//Serial.print("km/h ,  direction = ");
//Serial.print(deg);
if (deg >=360) deg = deg%360;  // 0..359 degree
//Serial.println("° ");
//Serial.print("ID weather = ");
//Serial.print(id);
//Serial.print(" + clouds = ");
//Serial.print(nori);
//Serial.print("% + icons = ");
//Serial.print(icon);
//Serial.println(" (d = day, n = night)");
}

void ceasbinar()
{
ora = timeClient.getHours();
minut = timeClient.getMinutes();
secundar = timeClient.getSeconds();

//tens of hour
int  h1 = ora/10;  // 0,1 or 2
int  h11 = h1/2;
int  h12 = h1%2;
  matrix.setPixelColor(binare[0], culbin[3*h11], culbin[3*h11+1], culbin[3*h11+2]);
  matrix.setPixelColor(binare[1], culbin[3*h12], culbin[3*h12+1], culbin[3*h12+2]);
//units of hour
int  h2 = ora%10;        // 0...9  (0000....1001)
int  h23 = h2/8;         // 0 or 1
int  h22 = (h2%8)/4;     // 0 or 1
int  h21 = ((h2%8)%4)/2; // 0 or 1
int  h20 = ((h2%8)%4)%2; // 0 or 1  
  matrix.setPixelColor(binare[4], culbin[3*h23], culbin[3*h23+1], culbin[3*h23+2]);
  matrix.setPixelColor(binare[5], culbin[3*h22], culbin[3*h22+1], culbin[3*h22+2]);
  matrix.setPixelColor(binare[6], culbin[3*h21], culbin[3*h21+1], culbin[3*h21+2]);  
  matrix.setPixelColor(binare[7], culbin[3*h20], culbin[3*h20+1], culbin[3*h20+2]); 
// tens of minutes
int  m1 = minut/10;      // 0...5  (000....101)
int  m12 = m1/4;         // 0 or 1
int  m11 = (m1%4)/2;     // 0 or 1
int  m10 = (m1%4)%2;     // 0 or 1
  matrix.setPixelColor(binare[11], culbin[3*m12], culbin[3*m12+1], culbin[3*m12+2]);
  matrix.setPixelColor(binare[12], culbin[3*m11], culbin[3*m11+1], culbin[3*m11+2]);  
  matrix.setPixelColor(binare[13], culbin[3*m10], culbin[3*m10+1], culbin[3*m10+2]);  
//units of minutes
int  m2 = minut%10;      // 0...9  (0000....1001)
int  m23 = m2/8;         // 0 or 1
int  m22 = (m2%8)/4;     // 0 or 1
int  m21 = ((m2%8)%4)/2; // 0 or 1
int  m20 = ((m2%8)%4)%2; // 0 or 1  
  matrix.setPixelColor(binare[16], culbin[3*m23], culbin[3*m23+1], culbin[3*m23+2]);
  matrix.setPixelColor(binare[17], culbin[3*m22], culbin[3*m22+1], culbin[3*m22+2]);
  matrix.setPixelColor(binare[18], culbin[3*m21], culbin[3*m21+1], culbin[3*m21+2]);  
  matrix.setPixelColor(binare[19], culbin[3*m20], culbin[3*m20+1], culbin[3*m20+2]); 
// tens of seconds
int  s1 = secundar/10;   // 0...5  (000....101)
int  s12 = s1/4;         // 0 or 1
int  s11 = (s1%4)/2;     // 0 or 1
int  s10 = (s1%4)%2;     // 0 or 1
  matrix.setPixelColor(binare[23], culbin[3*s12], culbin[3*s12+1], culbin[3*s12+2]);
  matrix.setPixelColor(binare[24], culbin[3*s11], culbin[3*s11+1], culbin[3*s11+2]);  
  matrix.setPixelColor(binare[25], culbin[3*s10], culbin[3*s10+1], culbin[3*s10+2]);  
//units of minutes
int  s2 = secundar%10;   // 0...9  (0000....1001)
int  s23 = s2/8;         // 0 or 1
int  s22 = (s2%8)/4;     // 0 or 1
int  s21 = ((s2%8)%4)/2; // 0 or 1
int  s20 = ((s2%8)%4)%2; // 0 or 1  
  matrix.setPixelColor(binare[28], culbin[3*s23], culbin[3*s23+1], culbin[3*s23+2]);
  matrix.setPixelColor(binare[29], culbin[3*s22], culbin[3*s22+1], culbin[3*s22+2]);
  matrix.setPixelColor(binare[30], culbin[3*s21], culbin[3*s21+1], culbin[3*s21+2]);  
  matrix.setPixelColor(binare[31], culbin[3*s20], culbin[3*s20+1], culbin[3*s20+2]); 
  digitalWrite(LED_BUILTIN, secundar%2);  

// 2 - wifi connected (green), 1 = wifi not-conected (blue), 0 = wifi disconnected (red)
if (wificon == 2)
 matrix.setPixelColor(0, 0, 150, 0);
if (wificon == 1)
 matrix.setPixelColor(0, 0, 0, 150);
if (wificon == 0)
 matrix.setPixelColor(0, 150, 0, 0); 
// 2 - weather data received (green), 1 - weather connected (blue), 0 = weather not-connected (red), 4 - no info from too long time 
if (vremecon == 2)
 matrix.setPixelColor(255, 0, 150, 0);
if (vremecon == 1)
 matrix.setPixelColor(255, 0, 0, 150);
if (vremecon == 0)
 matrix.setPixelColor(255, 150, 0, 0); 
if (vremecon == 4)
 matrix.setPixelColor(255, 150, 0, 255);  
}

void Soare()
{
   // Calculate the times of sunrise, transit, and sunset, in hours (UTC)
  calcSunriseSunset(an, luna, zi2, latitude, longitude, transit, sunrise, sunset);

m1 = int(round((sunrise + timezoneOffset + DST) * 60));
hr1 = (m1 / 60) % 24;
mn1 = m1 % 60;

m2 = int(round((sunset + timezoneOffset + DST) * 60));
hr2 = (m2 / 60) % 24;
mn2 = m2 % 60;

  Serial.print("Sunrise = ");
  Serial.print(hr1/10);
  Serial.print(hr1%10);
  Serial.print(":");
  Serial.print(mn1/10);
  Serial.print(mn1%10);
  Serial.println(" ! ");
  Serial.print("Sunset = ");
  Serial.print(hr2/10);
  Serial.print(hr2%10);
  Serial.print(":");
  Serial.print(mn2/10);
  Serial.print(mn2%10);
  Serial.println(" ! ");
}

void night() { 
  ora1 = 100*hr1 + mn1;  // rasarit 
  ora2 = 100*hr2 + mn2;  // apus
  oraactuala = 100*ora + minut;  // ora actuala

  Serial.print(ora1);
  Serial.print(" ? ");
  Serial.print(oraactuala);
  Serial.print(" ? ");
  Serial.println(ora2);  

  if ((oraactuala > ora2) or (oraactuala < ora1))  // night 
    dn = 0;  
    else
    dn = 1; 
}
