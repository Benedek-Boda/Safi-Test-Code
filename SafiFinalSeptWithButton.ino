//for temp sensor
#include <OneWire.h>
#include <DallasTemperature.h>
//general libraries
#include "Arduino.h"
#include <Wire.h>
//for display
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
//#include <Preferences.h> // for the instance counter
//for wifi and http  
#include <WiFi.h>
#include <HTTPClient.h>

// For captive portal, all code from here until pin assignment is for captive portal to avoid confusion
#include <DNSServer.h>
#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"
#include <Preferences.h>

Preferences preferences;
DNSServer dnsServer;
AsyncWebServer server(80);

//for sending data
bool sentinitial = false;
bool sentfinal = false;


// for wifi timer
unsigned long startTime;

String ssid;
String password;
bool is_setup_done = false;
bool valid_ssid_received = false;
bool valid_password_received = false;
bool wifi_timeout = false;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>Captive Portal Demo</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
  <h3>SAFI</h3>
  <br><br>
  <form action="/get">
    <br>
    SSID: <input type="text" name="ssid">
    <br>
    Password: <input type="text" name="password">
    <input type="submit" value="Submit">
  </form>
</body></html>)rawliteral";

class CaptiveRequestHandler : public AsyncWebHandler {
  public:
    CaptiveRequestHandler() {}
    virtual ~CaptiveRequestHandler() {}

    bool canHandle(AsyncWebServerRequest *request) {
      //request->addInterestingHeader("ANY");
      return true;
    }

    void handleRequest(AsyncWebServerRequest *request) {
      request->send_P(200, "text/html", index_html);
    }
};

void setupServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", index_html);
    Serial.println("Client Connected");
  });

  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest * request) {
    String inputMessage;
    String inputParam;

    if (request->hasParam("ssid")) {
      inputMessage = request->getParam("ssid")->value();
      inputParam = "ssid";
      ssid = inputMessage;
      Serial.println(inputMessage);
      valid_ssid_received = true;
    }

    if (request->hasParam("password")) {
      inputMessage = request->getParam("password")->value();
      inputParam = "password";
      password = inputMessage;
      Serial.println(inputMessage);
      valid_password_received = true;
    }
    request->send(200, "text/html", "The values entered by you have been successfully sent to the device. It will now attempt WiFi connection");
  });
}

void WiFiSoftAPSetup()
{
  WiFi.mode(WIFI_AP);
  WiFi.softAP("esp-captive");
  Serial.print("AP IP address: "); Serial.println(WiFi.softAPIP());
}

void WiFiStationSetup(String rec_ssid, String rec_password)
{
  wifi_timeout = false;
  WiFi.mode(WIFI_STA);
  char ssid_arr[20];
  char password_arr[20];
  rec_ssid.toCharArray(ssid_arr, rec_ssid.length() + 1);
  rec_password.toCharArray(password_arr, rec_password.length() + 1);
  Serial.print("Received SSID: "); Serial.println(ssid_arr); Serial.print("And password: "); Serial.println(password_arr);
  WiFi.begin(ssid_arr, password_arr);

  uint32_t t1 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    //delay(2000);
    Serial.print(".");
    if (millis() - t1 > 30000) //50 seconds elapsed connecting to WiFi
    {
      Serial.println();
      Serial.println("Timeout connecting to WiFi. The SSID and Password seem incorrect.");
      valid_ssid_received = false;
      valid_password_received = false;
      is_setup_done = false;
      preferences.putBool("is_setup_done", is_setup_done);
      startTime = millis();

      StartCaptivePortal();
      wifi_timeout = true;
      break;
    }
  }

  if (!wifi_timeout)
  {
    is_setup_done = true;
    Serial.println("");  Serial.print("WiFi connected to: "); Serial.println(rec_ssid);
    Serial.print("IP address: ");  Serial.println(WiFi.localIP());
    preferences.putBool("is_setup_done", is_setup_done);
    preferences.putString("rec_ssid", rec_ssid);
    preferences.putString("rec_password", rec_password);
  }
}

void StartCaptivePortal() {
  Serial.println("Setting up AP Mode");
  WiFiSoftAPSetup();
  Serial.println("Setting up Async WebServer");
  setupServer();
  Serial.println("Starting DNS Server");
  dnsServer.start(53, "*", WiFi.softAPIP());
  server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);//only when requested from AP
  server.begin();
  dnsServer.processNextRequest();
}

//Assigning and configuring pins for temp sensor on ESP32
const int oneWireBus = 3;
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

//Assigning correct pins motor connections on ESP32
#define motorRight 9 
#define motorLeft 8 
#define motorCycle 18

//Setting PWM properties for pin 18(motorCycle) 
const int freq = 30000;
const int pwmChannel = 0;
const int resolution = 8;

//duty cycle for motor
int dutyCycle = 225;

// assignning correct pins for LED's
#define orangeLed 4  
#define greenLed 6
#define redLed 5

// Alarm Pin
#define alarm 10 

//Defining for screen
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define I2C_SDA 0
#define I2C_SCL 1
#define OLED_RESET     -1 //or reset pin number depending on esp32
#define SCREEN_ADDRESS 0x3C //could also be 0x3C, depending on reference number will have to test
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


// Button aasignment
#define wifiswitch 20 

// Bool values for different modes
volatile int state = 0;
const int wifiMode = 1;

// For Interrupt deboucning
volatile unsigned long buttonPressTime = 0;
volatile unsigned long lastbuttonPressTime = 0;
const unsigned long minimumPressDuration = 50;

// Cleaning mode temperature (when reached cleaning is complete)
const int cleanTemp = 100;

//main timer
int timerCount = 15;

// Temp and pasturization status
const int pasturizeTemp = 72;
bool pasturizeComplete = false;
float currentTemperature = 0;
//so we can post highest temp to google sheet
float highestTemperature = 0.0;

// For waiting to subtract from time without delay
long lasttime1 = 0;

//constants that we change per each device for posting to google sheet
const char * location = "Kigali";
const char * deviceid = "safi001";

// For Alarm
unsigned int numTimesOnOff = 0;
unsigned int maxNumTimesOnOff = 3;
unsigned long intervalOn = 1000;     // 1 second ON time
unsigned long intervalOff = 500;    // 0.5 second OFF time
unsigned long previousMillis = 0;
boolean alarmState = false;

// for updating sheet on board memory
int instance = 0;
int capacity = 10;
int sizei = 0;
int sizef = 0;
 String datainitial[10];
 String datafinal[10];
int currentPosi = 0;
int currentPosf = 0;


// for posting to google sheet
String GOOGLE_SCRIPT_ID = "AKfycby8qEjGmqoiqgYqhdEH-eoDNpJINy6kyZamg-eyA4TZtmxyFB_Jtm8YCWKsgVJipR6SVw"; 

const char * root_ca=\
"-----BEGIN CERTIFICATE-----\n" \
"MIIFVzCCAz+gAwIBAgINAgPlk28xsBNJiGuiFzANBgkqhkiG9w0BAQwFADBHMQsw\n" \
"CQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEU\n" \
"MBIGA1UEAxMLR1RTIFJvb3QgUjEwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIyMDAw\n" \
"MDAwWjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZp\n" \
"Y2VzIExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjEwggIiMA0GCSqGSIb3DQEBAQUA\n" \
"A4ICDwAwggIKAoICAQC2EQKLHuOhd5s73L+UPreVp0A8of2C+X0yBoJx9vaMf/vo\n" \
"27xqLpeXo4xL+Sv2sfnOhB2x+cWX3u+58qPpvBKJXqeqUqv4IyfLpLGcY9vXmX7w\n" \
"Cl7raKb0xlpHDU0QM+NOsROjyBhsS+z8CZDfnWQpJSMHobTSPS5g4M/SCYe7zUjw\n" \
"TcLCeoiKu7rPWRnWr4+wB7CeMfGCwcDfLqZtbBkOtdh+JhpFAz2weaSUKK0Pfybl\n" \
"qAj+lug8aJRT7oM6iCsVlgmy4HqMLnXWnOunVmSPlk9orj2XwoSPwLxAwAtcvfaH\n" \
"szVsrBhQf4TgTM2S0yDpM7xSma8ytSmzJSq0SPly4cpk9+aCEI3oncKKiPo4Zor8\n" \
"Y/kB+Xj9e1x3+naH+uzfsQ55lVe0vSbv1gHR6xYKu44LtcXFilWr06zqkUspzBmk\n" \
"MiVOKvFlRNACzqrOSbTqn3yDsEB750Orp2yjj32JgfpMpf/VjsPOS+C12LOORc92\n" \
"wO1AK/1TD7Cn1TsNsYqiA94xrcx36m97PtbfkSIS5r762DL8EGMUUXLeXdYWk70p\n" \
"aDPvOmbsB4om3xPXV2V4J95eSRQAogB/mqghtqmxlbCluQ0WEdrHbEg8QOB+DVrN\n" \
"VjzRlwW5y0vtOUucxD/SVRNuJLDWcfr0wbrM7Rv1/oFB2ACYPTrIrnqYNxgFlQID\n" \
"AQABo0IwQDAOBgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4E\n" \
"FgQU5K8rJnEaK0gnhS9SZizv8IkTcT4wDQYJKoZIhvcNAQEMBQADggIBAJ+qQibb\n" \
"C5u+/x6Wki4+omVKapi6Ist9wTrYggoGxval3sBOh2Z5ofmmWJyq+bXmYOfg6LEe\n" \
"QkEzCzc9zolwFcq1JKjPa7XSQCGYzyI0zzvFIoTgxQ6KfF2I5DUkzps+GlQebtuy\n" \
"h6f88/qBVRRiClmpIgUxPoLW7ttXNLwzldMXG+gnoot7TiYaelpkttGsN/H9oPM4\n" \
"7HLwEXWdyzRSjeZ2axfG34arJ45JK3VmgRAhpuo+9K4l/3wV3s6MJT/KYnAK9y8J\n" \
"ZgfIPxz88NtFMN9iiMG1D53Dn0reWVlHxYciNuaCp+0KueIHoI17eko8cdLiA6Ef\n" \
"MgfdG+RCzgwARWGAtQsgWSl4vflVy2PFPEz0tv/bal8xa5meLMFrUKTX5hgUvYU/\n" \
"Z6tGn6D/Qqc6f1zLXbBwHSs09dR2CQzreExZBfMzQsNhFRAbd03OIozUhfJFfbdT\n" \
"6u9AWpQKXCBfTkBdYiJ23//OYb2MI3jSNwLgjt7RETeJ9r/tSQdirpLsQBqvFAnZ\n" \
"0E6yove+7u7Y/9waLd64NnHi/Hm3lCXRSHNboTXns5lndcEZOitHTtNCjv0xyBZm\n" \
"2tIMPNuzjsmhDYAPexZ3FL//2wmUspO8IFgV6dtxQ/PeEMMA3KgqlbbC1j+Qa3bb\n" \
"bP6MvPJwNQzcmRk13NfIRmPVNnGuV/u3gm3c\n" \
"-----END CERTIFICATE-----\n";

WiFiClientSecure client;


//the function tempCheck does the checking to see if 73 celcius has been acheived 
// every second and returns true or false based on this, use this function to determine when to start pasturizing
bool tempCheck(int temp)
{
  sensors.requestTemperatures(); 
  float tempC = sensors.getTempCByIndex(0);
  if(tempC>temp)
  {
    return true;
  }
  else
  {
    digitalWrite(redLed,HIGH);
    digitalWrite(orangeLed, LOW);
    return false;
  }
}

// clears the temp on display
void cleartemp(){
  int y =0;
  int x = 0;
  for (y=8; y<=16; y++)
      {
       for (x=0; x<127; x++)
       {
        display.drawPixel(x, y, BLACK); 
       }
      }
}

// clears the word "Temperature:" on display once pasturized so we can display new text
void cleartempline(){
  int y =0;
  int x = 0;
  for (y=40; y<=46; y++)
      {
       for (x=0; x<127; x++)
       {
        display.drawPixel(x, y, BLACK); 
       }
      }
}

// clears the time on display
void cleartime(){
  int y =0;
  int x = 0;
  for (y=48; y<=56; y++)
      {
       for (x=0; x<127; x++)
       {
        display.drawPixel(x, y, BLACK); 
       }
      }
}

// Display text on the screen
void displaytext(const char* text){
  cleartime(); // clears the time everytime so no flicker
  cleartempline();
  display.setCursor(0, 40); // can edit position later (x,y) coordinate
  display.println(text);
  display.display();
}

// Displays the temperature to the display, also stores highest temp so we can post later
void displayTemp() {
  sensors.requestTemperatures();
  float currentTemperature = sensors.getTempCByIndex(0);
  //sets the highest temp
  if (currentTemperature > highestTemperature)
  {
    highestTemperature = currentTemperature;
  }

  cleartemp(); // clears the temp everytime so no flicker
  display.setCursor(0, 0); // starts at top left corner
  display.println("Temperature:");
  display.println(currentTemperature); 
  display.display();
}

// Displays the countdown until pasturization, and displays pasturization when complete 
void displaycountdown() {

if (timerCount > 0) {
  // calculate seconds and display countdown
  cleartime(); // clears the time everytime so no flicker
  display.setCursor(0, 40); // can edit position later (x,y) coordinate
  display.println("Time Remaining:");
  display.println(timerCount);
  
  display.display();

} 
else {
  cleartime(); // clears the time everytime so no flicker
  cleartempline();
  display.setCursor(0, 40); // can edit position later (x,y) coordinate
  display.println("Pasteurization Complete");
  display.display();
  }

}

void displayend(){
  display.clearDisplay();
  display.setCursor(0, 0); // starts at top left corner
  display.println("Device ID: ");
  display.println(deviceid);
  

  
  display.setCursor(0, 40); // can edit position later (x,y) coordinate
  display.println("Instance: ");
  display.println(instance);
  
  
  display.display();
  
}

//afterPasturize funtion activates when milk has been above 73 degrees for 15 seconds while being stirred using a flag
// the function sounds a beeping sound and turns orange, red led off and green led on
void afterPasturize(){
  // After pasturization is complete, sound alarm and LED
  if(pasturizeComplete == true){

    if (!sentfinal){
      updatesheetfinal();
      sentfinal = true;
    }
    
    displayend();
    digitalWrite(orangeLed,LOW);
    digitalWrite(greenLed,HIGH);
    unsigned long currentMillis = millis();

    if (numTimesOnOff < maxNumTimesOnOff) {
      if (!alarmState && (currentMillis - previousMillis >= intervalOff)) {
        // Turn the alarm on
        alarmState = true;
        tone(alarm, 3000);
        previousMillis = currentMillis;
      } else if (alarmState && (currentMillis - previousMillis >= intervalOn)) {
        // Turn the alarm off
        alarmState = false;
        noTone(alarm);
        previousMillis = currentMillis;
        numTimesOnOff++;
    }
  }
}
}

void dopasturize(){
  if (pasturizeComplete == false){
    if (!sentinitial){
      updatesheetinitial();
      sentinitial = true;
    }


  //Always display temp
  displayTemp();
  digitalWrite(motorRight, HIGH);
  digitalWrite(motorLeft, LOW);

  // Check temp, if true then start countdown
  if (tempCheck(pasturizeTemp) == true){

  // Set LED's
  digitalWrite(orangeLed,HIGH);
  digitalWrite(redLed,LOW);

  // Display coutndown to dispaly
  displaycountdown();
  long time1 = millis();

  // Subtract one second after a second has passed
  if (time1 - lasttime1 > 1000){
    lasttime1 = time1;
    timerCount -= 1;
  }

  // If timer complete set pasturize
  if (timerCount == 0){
     pasturizeComplete = true;
    }
  } else{
  timerCount = 15; // If temp ever falls below 73 celcius timerCount is reset to 0 and will continue checking

    }
  }

}


void IRAM_ATTR wifiISR() {
    buttonPressTime = millis();

    if(buttonPressTime - lastbuttonPressTime > minimumPressDuration){
      state = 0;
      state = 1;
      lastbuttonPressTime = buttonPressTime;
      display.clearDisplay();
      }

}

void wifi(){

  if (!is_setup_done && millis() - startTime < 30000)
  {
    StartCaptivePortal();
  }
  else if(!is_setup_done) {
    return;
  }
  else{
    Serial.println("Using saved SSID and Password to attempt WiFi Connection!");
    Serial.print("Saved SSID is ");Serial.println(ssid);
    Serial.print("Saved Password is ");Serial.println(password);
    WiFiStationSetup(ssid, password);
  }

  while (!is_setup_done && millis() - startTime < 30000)
  {
    dnsServer.processNextRequest();
    delay(10);
    if (valid_ssid_received && valid_password_received)
    {
      Serial.println("Attempting WiFi Connection!");
      WiFiStationSetup(ssid, password);
    }
  }
  display.clearDisplay();
  Serial.println("All Done!");
}

// for sending data to the google sheet
void sendData(String params) {
   HTTPClient http;
   String url="https://script.google.com/macros/s/"+GOOGLE_SCRIPT_ID+"/exec?"+params;
   Serial.print(url);
    Serial.print("Making a request");
    http.begin(url, root_ca); //Specify the URL and certificate
    int httpCode = http.GET();  
    http.end();
    Serial.println(": done "+httpCode);
}

// will be called when pasturization started
void updatesheetinitial(){
  //each time the pasturize function is called the instance is updated once
  instance++;
  // Write the new value to preferences
  Preferences preferences;
  preferences.begin("myApp", false);
  preferences.putInt("instance", instance);
  preferences.end();
  // Print the current value
  Serial.print("Current value: ");
  Serial.println(instance);

  String device_id(deviceid);
  String location_(location);
  String maxtemp_(highestTemperature);
  String instance_(instance);

  const String params = "deviceid=" + device_id + "&location=" + location_ + "&status=not-pasturized&maxtemp=" + maxtemp_ + "&instance=" + instance_;
  if(sizei < capacity){
  datainitial[sizei] = params;
  sizei++;
  } else{
    // Replace the oldest params in the circular buffer
    datainitial[currentPosi] = params;
    currentPosi = (currentPosi + 1); // Move to the next position in the circular buffer
     if(currentPosi == 10){
      currentPosi = 0;
    }
  }

  preferences.begin("myApp", false);
  for (int i = 0; i < sizei; i++) {
    const char* params = "datainitial_" + i;
    preferences.putString(params, datainitial[i] );
  }
  preferences.putInt("sizei", sizei);
  preferences.putInt("currentPosi", currentPosi);
  preferences.end();

  // send inital data
   if (WiFi.status() == WL_CONNECTED) {
     
     for (int i = 0; i < sizei; i++){
       String params = datainitial[i];
       sendData(params);
     }
    for (int i = sizei; i >= 0; i--) {
        sizei = i;
     }
        currentPosi = 0;
        Preferences preferences;
        preferences.begin("myApp", false);
        preferences.putInt("currentPosi", currentPosi);
        preferences.putInt("sizei", sizei);
        preferences.end();
  } else {
    Serial.println("WiFi not connected");
  }
}

// called in after pasturization, so only when it is done
void updatesheetfinal(){
String device_id(deviceid);
  String location_(location);
  String maxtemp_(highestTemperature);
  String instance_(instance);

  const String params = "deviceid=" + device_id + "&location=" + location_ + "&status=pasturized&maxtemp=" + maxtemp_ + "&instance=" + instance_;
  if(sizef < capacity){
  datafinal[sizef] = params;
  sizef++;
  } else{
    // Replace the oldest params in the circular buffer
    datafinal[currentPosf] = params;
    currentPosf = (currentPosf + 1); // Move to the next position in the circular buffer
    if(currentPosf == 10){
      currentPosf = 0;
    }
  }
  Preferences preferences;
  preferences.begin("myApp", false);
  for (int i = 0; i < sizef; i++) {
    const char* params = "datafinal_" + i;
    preferences.putString(params, datafinal[i]);
  }
  preferences.putInt("sizef", sizef);
  preferences.putInt("currentPosf", currentPosf);
  preferences.end();

  // send final data
   if (WiFi.status() == WL_CONNECTED) {
     for (int i = 0; i < sizef; i++){
        String params = datafinal[i];
       sendData(params);
     }
    for (int i = sizef; i >= 0; i--) {
        sizef = i;
     } 
        currentPosf = 0;
        Preferences preferences;
        preferences.begin("myApp", false);
        preferences.putInt("currentPosf", currentPosf);
        preferences.putInt("sizef", sizef);
        preferences.end();
  } else {
    Serial.println("WiFi not connected");
  }
}

void pasturize(){
  
  // start and finish the pasturization process
  dopasturize();

  // Alarm and LED after pasturize
  afterPasturize();

}

void setup() {
// Setting LED pin modes
pinMode(orangeLed,OUTPUT);
pinMode(redLed,OUTPUT);
pinMode(greenLed,OUTPUT);

// Setting motor pin modes
pinMode(motorRight, OUTPUT);
pinMode(motorLeft, OUTPUT);
pinMode(motorCycle, OUTPUT);

// Alarm pin mode
pinMode(alarm, OUTPUT);

//Activates temp sensor
sensors.begin();

//Treats motor as an LED using PWM to control the duty cycle at which voltage is given, which controls speed of motor
ledcSetup(pwmChannel, freq, resolution); //Sets up a PWM channel
ledcAttachPin(motorCycle, pwmChannel); // Assigns PWM channel to the correct pin
ledcWrite(pwmChannel, dutyCycle); // Sets the duty cycle at which channel will run at, sets motor speed

// Serial.begin, for debugging
Serial.begin(115200);

//Initiializing display
Wire.begin(I2C_SDA, I2C_SCL);
display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS); //initialize the OLED display
display.clearDisplay();
display.setTextSize(1.75); // depening on how big we want text, pixel scale
display.setTextColor(SSD1306_WHITE);

// interuppt for wifi
attachInterrupt(digitalPinToInterrupt(wifiswitch), wifiISR, FALLING);

// Button for interrupts
pinMode(wifiswitch, INPUT_PULLUP);

// pretty sure this goes here, for the captive portal
preferences.begin("my-pref", false);
is_setup_done = preferences.getBool("is_setup_done", false);
ssid = preferences.getString("rec_ssid", "Sample_SSID");
password = preferences.getString("rec_password", "abcdefgh");

Preferences preferences;
  preferences.begin("myApp", false);

  instance = preferences.getInt("instance", 0);
  sizei = preferences.getInt("sizei", 0);
  sizef = preferences.getInt("sizef", 0);
  for (int i = 0; i < capacity; i++) {
    const char* params = "datainitial_" + i;
    datainitial[i] = preferences.getString(params," ");
  }
  for (int i = 0; i < capacity; i++) {
    const char* params = "datafinal_" + i;
    datafinal[i] = preferences.getString(params, " ");
  }
  currentPosi = preferences.getInt("currentPosi", 0);
  currentPosf = preferences.getInt("currentPosf", 0);

  preferences.end();

}

void loop() {
if (tempCheck(pasturizeTemp) == false){
if(state == wifiMode){
  startTime = millis();
  displaytext("wifi");
  wifi();
  
  state = 0;
}
}
pasturize();

}
