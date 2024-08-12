#define debugMode 1  //0 off, 1 on
#if debugMode == 1
#define debug(x) Serial.println(x)  //printSerial
#else
#define debug(x)
#endif

//Sensor & OLED
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include "MAX30105.h"
#include "heartRate.h"
#include "spo2_algorithm.h"
MAX30105 particleSensor;
Adafruit_SSD1306 display(128, 64, &Wire, -1);

//Wifi, Web Server, mqtt
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <FS.h> //spiffs
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
DNSServer dnsServer;
AsyncWebServer server(80);

#include <PubSubClient.h>
WiFiClient espClient;
PubSubClient mqtt(espClient);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Device Configuration</title>
</head>
<body>
  <form action="/get">
    <h2>Device Settings</h2>
    <label for="mode">Mode:</label>
    <select id="modeID" name="mode" required>
        <option value="online">online</option>
        <option value="offline">offline</option>
    </select><br><br>
    <label for="wifiSsid">WiFi SSID:</label><br>
    <input type="text" id="wifiSsid" name="wifiSsid" value="%wifiSsid%" placeholder="Your WiFi SSID"><br>
    <label for="wifiPass">WiFi Password:</label><br>
    <input type="text" id="wifiPass" name="wifiPass" value="%wifiPass%" placeholder="Your WiFi Password"><br>
    <br>
    <label for="apSsid">AP SSID:</label><br>
    <input type="text" id="apSsid" name="apSsid" value="%apSsid%" placeholder="Your AP SSID"><br>
    <label for="apPass">AP Password:</label><br>
    <input type="text" id="apPass" name="apPass" value="%apPass%" placeholder="Your WiFi Password"><br>
    <br>
    <label for="mqttServer">MQTT Server:</label><br>
    <input type="text" id="mqttServer" name="mqttServer" value="%mqttServer%" placeholder="MQTT Server" ><br>
    <label for="mqttPort">MQTT Port:</label><br>
    <input type="number" id="mqttPort" name="mqttPort" value="%mqttPort%" placeholder="MQTT Port"><br>
    <label for="mqttId">MQTT ID:</label><br>
    <input type="text" id="mqttId" name="mqttId" value="%mqttId%" placeholder="MQTT Device ID"><br>
    <label for="mqttUser">MQTT Username:</label><br>
    <input type="text" id="mqttUser" name="mqttUser" value="%mqttUser%" placeholder="MQTT Username"><br>
    <label for="mqttPass">MQTT Password:</label><br>
    <input type="text" id="mqttPass" name="mqttPass" value="%mqttPass%" placeholder="MQTT Password"><br>
    <label for="mqttTopic">MQTT Pub Topic:</label><br>
    <input type="text" id="mqttTopic" name="mqttTopic" value="%mqttTopic%" placeholder="MQTT Pub Topic"><br>
    <br>
    <h2>Data Pasien</h2>
    <label for="nama">Nama:</label><br>
    <input type="text" id="nama" name="nama" value="%nama%" placeholder="Nama Pasien"><br>
    <label for="usia">Usia:</label><br>
    <input type="number" id="usia" name="usia" value="%usia%" placeholder="Usia Pasien"><br>
    <label for="gender">Gender:</label><br>
    <select id="genderID" name="gender" required>
        <option value="L">Laki-laki</option>
        <option value="P">Perempuan</option>
    </select><br>
    <br>
    <input type="submit" value="SAVE">
  </form>
  <script>
    function setSelect(id, value) {
        var dd = document.getElementById(id);
        for (var i = 0; i < dd.options.length; i++) {
            if (dd.options[i].value == value) {
                dd.selectedIndex = i;
                break;
            }
        }
    } 
    var selectedMode = "%mode%";
    setSelect('modeID', selectedMode);
    var selectedGender = "%gender%";
    setSelect('genderID', selectedGender);
  </script>
</body>
</html>
)rawliteral";

const unsigned char bootLogo[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0xff, 0xfe, 0x00, 0x00, 0x7f, 0xff, 0x80, 0x00, 0xff, 0xff, 0xf0, 0x0f, 0xff, 0xff, 0x80,
  0x00, 0xff, 0xff, 0xfc, 0x3f, 0xff, 0xff, 0x80, 0x00, 0xff, 0xff, 0xfe, 0x7f, 0xff, 0xff, 0x80,
  0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80,
  0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80,
  0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80,
  0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80,
  0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
  0x00, 0x00, 0x01, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xfc, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x07, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xc0, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0xff, 0xfc, 0x00, 0x00, 0x1f, 0xff, 0x00,
  0x00, 0xff, 0xff, 0x00, 0x00, 0x7f, 0xff, 0x00, 0x00, 0xff, 0xff, 0x80, 0x01, 0xff, 0xff, 0x00,
  0x00, 0xff, 0xff, 0xc0, 0x03, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xe0, 0x07, 0xff, 0xff, 0x00,
  0x00, 0xff, 0xff, 0xf0, 0x07, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xf0, 0x0f, 0xff, 0xff, 0x00,
  0x00, 0xff, 0xff, 0xf8, 0x0f, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xf8, 0x1f, 0xff, 0xff, 0x00,
  0x00, 0x7f, 0xff, 0xf8, 0x1f, 0xff, 0xff, 0x00, 0x00, 0x00, 0x7f, 0xf8, 0x1f, 0xff, 0x00, 0x00,
  0x00, 0x00, 0x3f, 0xfc, 0x1f, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xfc, 0x3f, 0xf8, 0x00, 0x00,
  0x00, 0x00, 0x1f, 0xfc, 0x3f, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xfc, 0x3f, 0xf8, 0x00, 0x00,
  0x00, 0x00, 0x0f, 0xfc, 0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xfc, 0x3f, 0xf0, 0x00, 0x00,
  0x00, 0x00, 0x0f, 0xfc, 0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xfc, 0x3f, 0xf0, 0x00, 0x00,
  0x00, 0x00, 0x0f, 0xfc, 0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xfc, 0x3f, 0xf0, 0x00, 0x00,
  0x00, 0x00, 0x0f, 0xfc, 0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xfc, 0x3f, 0xf0, 0x00, 0x00,
  0x00, 0x00, 0x0f, 0xfc, 0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xfc, 0x3f, 0xf0, 0x00, 0x00,
  0x00, 0x00, 0x0f, 0xfc, 0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xfc, 0x3f, 0xf0, 0x00, 0x00,
  0x00, 0x00, 0x0f, 0xfc, 0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xfc, 0x3f, 0xf0, 0x00, 0x00,
  0x00, 0x00, 0x0f, 0xfc, 0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xfc, 0x3f, 0xf0, 0x00, 0x00,
  0x00, 0x00, 0x0f, 0xfc, 0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xfc, 0x3f, 0xf0, 0x00, 0x00,
  0x00, 0x00, 0x0f, 0xfc, 0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xfc, 0x3f, 0xf0, 0x00, 0x00,
  0x00, 0x00, 0x0f, 0xfc, 0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xfc, 0x3f, 0xf0, 0x00, 0x00,
  0x00, 0x00, 0x0f, 0xfc, 0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xfc, 0x1f, 0xf0, 0x00, 0x00,
  0x00, 0x00, 0x0f, 0xf8, 0x1f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xf0, 0x0f, 0xf0, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


// 'battLv0', 10x16px
const unsigned char battLv0[] = {
  0x1e, 0x00, 0x1e, 0x00, 0xff, 0xc0, 0x80, 0x40, 0x80, 0x40, 0x80, 0x40, 0x80, 0x40, 0x80, 0x40,
  0x80, 0x40, 0x80, 0x40, 0x80, 0x40, 0x80, 0x40, 0x80, 0x40, 0x80, 0x40, 0x80, 0x40, 0xff, 0xc0
};
// 'batt1_png-modified', 10x16px
const unsigned char battLv1[] = {
  0x1e, 0x00, 0x1e, 0x00, 0xff, 0xc0, 0x80, 0x40, 0x80, 0x40, 0x80, 0x40, 0x80, 0x40, 0x80, 0x40,
  0x80, 0x40, 0x80, 0x40, 0x80, 0x40, 0x80, 0x40, 0xbf, 0x40, 0xbf, 0x40, 0x80, 0x40, 0xff, 0xc0
};
// 'battLv2', 10x16px
const unsigned char battLv2[] = {
  0x1e, 0x00, 0x1e, 0x00, 0xff, 0xc0, 0x80, 0x40, 0x80, 0x40, 0x80, 0x40, 0x80, 0x40, 0x80, 0x40,
  0x80, 0x40, 0xbf, 0x40, 0xbf, 0x40, 0xbf, 0x40, 0xbf, 0x40, 0xbf, 0x40, 0x80, 0x40, 0xff, 0xc0
};
// 'battLv3', 10x16px
const unsigned char battLv3[] = {
  0x1e, 0x00, 0x1e, 0x00, 0xff, 0xc0, 0x80, 0x40, 0x80, 0x40, 0x80, 0x40, 0xbf, 0x40, 0xbf, 0x40,
  0xbf, 0x40, 0xbf, 0x40, 0xbf, 0x40, 0xbf, 0x40, 0xbf, 0x40, 0xbf, 0x40, 0x80, 0x40, 0xff, 0xc0
};
// 'battLv4', 10x16px
const unsigned char battLv4[] = {
  0x1e, 0x00, 0x1e, 0x00, 0xff, 0xc0, 0x80, 0x40, 0xbf, 0x40, 0xbf, 0x40, 0xbf, 0x40, 0xbf, 0x40,
  0xbf, 0x40, 0xbf, 0x40, 0xbf, 0x40, 0xbf, 0x40, 0xbf, 0x40, 0xbf, 0x40, 0x80, 0x40, 0xff, 0xc0
};

// 'chargeIcon', 16x16px
const unsigned char chargeIcon[] = {
  0x00, 0x00, 0x00, 0x80, 0x01, 0x80, 0x01, 0x80, 0x03, 0x80, 0x07, 0x80, 0x07, 0x80, 0x0f, 0xf8,
  0x1f, 0xf0, 0x01, 0xe0, 0x01, 0xe0, 0x01, 0xc0, 0x01, 0x80, 0x01, 0x80, 0x01, 0x00, 0x00, 0x00
};

// 'apIcon', 16x16px
const unsigned char apIcon[] = {
  0x00, 0x00, 0x00, 0x00, 0x20, 0x04, 0x60, 0x06, 0x58, 0x1a, 0xd0, 0x0b, 0xb4, 0x2d, 0xa5, 0xa5,
  0xa5, 0xa5, 0xb4, 0x2d, 0xd0, 0x0b, 0x58, 0x12, 0x60, 0x06, 0x20, 0x04, 0x00, 0x00, 0x00, 0x00
};
// 'wifiIcon', 16x16px
const unsigned char wifiIcon[] = {
  0x00, 0x00, 0x00, 0x00, 0x0f, 0xf0, 0x3c, 0x3c, 0x60, 0x06, 0xc7, 0xe3, 0x1e, 0x78, 0x38, 0x1c,
  0x03, 0xc0, 0x07, 0xe0, 0x00, 0x00, 0x01, 0x80, 0x03, 0xc0, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00
};

// 'heartRateIcon', 20x20px
const unsigned char heartRateIcon[] = {
  0x1f, 0x0f, 0x80, 0x3f, 0x9f, 0xc0, 0x7f, 0xff, 0xe0, 0x7f, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xfd,
  0xff, 0xf0, 0xf8, 0xff, 0xf0, 0xf8, 0xff, 0xf0, 0x00, 0x41, 0xf0, 0x02, 0x01, 0xe0, 0x7f, 0x1f,
  0xe0, 0x3f, 0x1f, 0xc0, 0x3f, 0xbf, 0xc0, 0x1f, 0xff, 0x80, 0x0f, 0xff, 0x00, 0x07, 0xfe, 0x00,
  0x01, 0xf8, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00
};
// 'oxygen', 20x20px
const unsigned char oxygenIcon[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xc0, 0x00, 0x3f, 0xf0, 0x00, 0x7f,
  0xf0, 0x00, 0x78, 0x78, 0x00, 0xf0, 0x78, 0x00, 0xf0, 0x78, 0x00, 0xf0, 0x78, 0x00, 0xf0, 0x78,
  0x00, 0xf0, 0x79, 0xe0, 0x78, 0xf9, 0xb0, 0x7f, 0xf0, 0x30, 0x3f, 0xe0, 0xe0, 0x0f, 0x81, 0x80,
  0x00, 0x01, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
// 'glucoseIcon', 20x20px
const unsigned char glucoseIcon[] = {
  0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x71, 0x00, 0x01, 0xfb, 0x80, 0x07,
  0xf7, 0x00, 0x1f, 0xb6, 0x00, 0x3f, 0x1d, 0x80, 0x7e, 0x0f, 0x80, 0x7c, 0x07, 0x80, 0xfe, 0x03,
  0x80, 0xff, 0x07, 0x00, 0xff, 0x8f, 0x00, 0xff, 0xde, 0x00, 0xff, 0xfe, 0x00, 0x7f, 0xfe, 0x00,
  0x7f, 0xfc, 0x00, 0x3f, 0xf8, 0x00, 0x1f, 0xf0, 0x00, 0x07, 0xc0, 0x00
};

String mode;
String wifiSsid;
String wifiPass;
String apSsid;
String apPass;
String mqttServer;
String mqttPort;
String mqttId;
String mqttUser;
String mqttPass;
String mqttTopic;
String nama;
String usia;
String gender;

bool guiOn = false;
bool connecting = false;
uint8_t heartRate = 0;
uint8_t oxySaturation = 0;
uint16_t bloodGlucose = 0;
uint8_t measurementStage = 0;
String diagnose = "--";

//inisiasi awal saat mikrokontroler booting
void setup() {
  Serial.begin(115200);
  pinMode(4, INPUT);
  Wire.pins(2, 0);
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.drawBitmap(31, 0, bootLogo, 64, 64, WHITE);
  display.display();
  delay(500);

  spiffSetup();
  readSetting();

  particleSensor.begin(Wire, I2C_SPEED_FAST);
  particleSensor.setup();                     //Configure sensor with default settings
  particleSensor.setPulseAmplitudeRed(0x0A);  //Turn Red LED to low to indicate sensor is running
  particleSensor.setPulseAmplitudeGreen(0);   //Turn off Green LED
  
  if (particleSensor.getIR() > 50000) {
    guiOn = true;
  }
  if (guiOn == true) {
    particleSensor.shutDown();
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1) , IPAddress(255, 255, 255, 0));
    WiFi.softAP(apSsid.c_str(), apPass.c_str());
    dnsServer.start(53, "*", IPAddress(192, 168, 1, 1));
    serverSetup();
  }
  else WiFi.softAPdisconnect(true);
  while (guiOn) {
    static unsigned long lastInfo = 0;
    if (millis() - lastInfo > 1000) {
      displayPrint();
      lastInfo = millis();
    }
    dnsServer.processNextRequest();
    delay(1);
  }
  guiOn = false;

  if (mode == "online") {
    WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
    debug("ssid :"+wifiSsid);
    debug("pass :"+wifiPass);
    connecting = true;
    displayPrint();
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      debug(".");
    }
    mqttSetup();
    debug("Connected");
    connecting = false;
  }
  else {
    WiFi.disconnect(); 
    WiFi.mode(WIFI_OFF);
  }
}
void loop() {
  if (particleSensor.getIR() > 50000) {
    measurementStage = 1;
    displayPrint();
    heartRate = getHeartRate();
    oxySaturation = getOxySaturation();
    bloodGlucose = getBloodGlucose();
    diagnose = getFuzzyDiag(bloodGlucose, heartRate, oxySaturation);
    measurementStage = 2;
    displayPrint();
    if (mode == "online" ) {
      String data = "{\"Nama\":\""+nama+"\",\"Usia\":"+usia+",\"Gender\":\""+gender+"\",\"HR\":"+String(heartRate)+",\"SpO2\":"+String(oxySaturation)+",\"Glucose\":"+String(bloodGlucose)+",\"Diag\":\""+diagnose+"\"}";
      sendData_Mqtt(data);
    }
    delay(5000);
  }
  measurementStage = 0;
  displayPrint();
  // displayPrint(chargingStatus, battLevel, heartRate);
}

uint8_t getHeartRate(){
  particleSensor.setup();                     //Configure sensor with default settings
  particleSensor.setPulseAmplitudeRed(0x0A);  //Turn Red LED to low to indicate sensor is running
  particleSensor.setPulseAmplitudeGreen(0);   //Turn off Green LED
  unsigned long _lastBeat = 0;
  const byte _RATE_SIZE = 5;  //Increase this for more averaging. 4 is good.
  byte _rates[_RATE_SIZE];    //Array of heart _rates
  byte _rateSpot = 0;
  float _beatsPerMinute;
  int _beatAvg;
  unsigned long startSamplingHR = millis();
  _rates[_RATE_SIZE-1] = 0;
  while (_rates[_RATE_SIZE-1] == 0) {
    displayPrint();
    long irValue = particleSensor.getIR();
    if (checkForBeat(irValue) == true) {
      //We sensed a beat!
      long delta = millis() - _lastBeat;
      _lastBeat = millis();
      _beatsPerMinute = 60 / (delta / 1000.0);
      if (_beatsPerMinute < 255 && _beatsPerMinute > 20) {
        startSamplingHR = millis();
        // debug("bpm "+String(_beatsPerMinute));
        _rates[_rateSpot++] = (byte)_beatsPerMinute;  //Store this reading in the array
        _rateSpot %= _RATE_SIZE;                      //Wrap variable
        //Take average of readings
        _beatAvg = 0;
        for (byte x = 0; x < _RATE_SIZE; x++) {
          _beatAvg += _rates[x];
        }
        _beatAvg /= _RATE_SIZE;
        //  debug(_beatAvg);
      }
    }
    if (millis() - startSamplingHR > 30000) return 0; //failover jika tetap invalid lebih dari 20 detik
  }
  debug("HR     : " + String(_beatAvg) + " bpm");
  return _beatAvg;
}

uint8_t getOxySaturation() {
  uint32_t irBuffer[100]; //infrared LED sensor data
  uint32_t redBuffer[100];  //red LED sensor data
  int32_t bufferLength; //data length
  int32_t spo2; //SPO2 value
  bool validSPO2 = false; //indicator to show if the SPO2 calculation is valid
  byte ledBrightness = 60; //Options: 0=Off to 255=50mA
  byte sampleAverage = 4; //Options: 1, 2, 4, 8, 16, 32
  byte ledMode = 2; //Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
  byte sampleRate = 100; //Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
  int pulseWidth = 411; //Options: 69, 118, 215, 411
  int adcRange = 4096; //Options: 2048, 4096, 8192, 16384

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); //Configure sensor with these settings
  unsigned long startSamplingSPO2 = millis();
  while (!validSPO2) {
    bufferLength = 100; //buffer length of 100 stores 4 seconds of samples running at 25sps
    //read the first 100 samples, and determine the signal range
    for (byte i = 0 ; i < bufferLength ; i++)
    {
      while (particleSensor.available() == false) //do we have new data?
        particleSensor.check(); //Check the sensor for new data

      redBuffer[i] = particleSensor.getRed();
      irBuffer[i] = particleSensor.getIR();
      particleSensor.nextSample(); //We're finished with this sample so move to next sample
      displayPrint();
    }
    //calculate heart rate and SpO2 after first 100 samples (first 4 seconds of samples)
    calculate_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2);
    if (millis() - startSamplingSPO2 > 20000) return 0; //failover jika tetap invalid lebih dari 20 detik
  }
  debug("SpO2   : " + String(spo2) + " %");
  return spo2;
}

uint8_t getBloodGlucose() {
  particleSensor.setup();                     //Configure sensor with default settings
  particleSensor.setPulseAmplitudeRed(0x0A);  //Turn Red LED to low to indicate sensor is running
  particleSensor.setPulseAmplitudeGreen(0);   //Turn off Green LED
  int maxADC = 0;
  unsigned long startSamplingGlucose = millis();
  while (millis() - startSamplingGlucose < 5000) {
    int adc = particleSensor.getIR();
    // debug("[IR] Value : " + String(adc));
    if (adc > maxADC) maxADC = adc;
    // debug("[IR] Value : " + String(maxADC));
    displayPrint();
  }
  float glucose = 0.0023*maxADC - 146.03;

  debug("Glucose: " + String(glucose)+ "mg/dl");
  return glucose;
}

String getFuzzyDiag(uint16_t &bloodGlucose, uint8_t &heartRate, uint8_t &oxySaturation) {
  //Fuzzifikasi
  float rendahW = miuRendahW(bloodGlucose);
  float normalW = miuNormalW(bloodGlucose);
  float agakTinggiW = miuAgakTinggiW(bloodGlucose);
  float tinggiW = miuTinggiW(bloodGlucose);

  float rendahX = miuRendahX(heartRate);
  float normalX = miuNormalX(heartRate);
  float tinggiX = miuTinggiX(heartRate);

  float rendahY = miuRendahY(oxySaturation);
  float normalY = miuNormalY(oxySaturation);

  //Inferensi Rule Base
  float a1 = min(rendahW, min(rendahX, rendahY)); //periksa
  float a2 = min(rendahW, min(rendahX, normalY)); //periksa
  float a3 = min(rendahW, min(normalX, rendahY)); //periksa
  float a4 = min(rendahW, min(normalX, normalY)); //sehat
  float a5 = min(rendahW, min(tinggiX, rendahY)); //periksa
  float a6 = min(rendahW, min(tinggiX, normalY)); //sehat
  float a7 = min(normalW, min(rendahX, rendahY)); //periksa
  float a8 = min(normalW, min(rendahX, normalY)); //periksa
  float a9 = min(normalW, min(normalX, rendahY)); //periksa
  float a10 = min(normalW, min(normalX, normalY)); //sehat
  float a11 = min(normalW, min(tinggiX, rendahY)); //periksa
  float a12 = min(normalW, min(tinggiX, normalY)); //sehat
  float a13 = min(agakTinggiW, min(rendahX, rendahY)); //periksa
  float a14 = min(agakTinggiW, min(rendahX, normalY)); //periksa
  float a15 = min(agakTinggiW, min(normalX, rendahY)); //periksa
  float a16 = min(agakTinggiW, min(normalX, normalY)); //sehat
  float a17 = min(agakTinggiW, min(tinggiX, rendahY)); //periksa
  float a18 = min(agakTinggiW, min(tinggiX, normalY)); //sehat
  float a19 = min(tinggiW, min(rendahX, rendahY)); //periksa
  float a20 = min(tinggiW, min(rendahX, normalY)); //periksa
  float a21 = min(tinggiW, min(normalX, rendahY)); //periksa
  float a22 = min(tinggiW, min(normalX, normalY)); //periksa
  float a23 = min(tinggiW, min(tinggiX, rendahY)); //periksa
  float a24 = min(tinggiW, min(tinggiX, normalY)); //periksa

  //Inferensi (Konsekuen)
  float z1 = zValuePeriksa(a1);
  float z2 = zValuePeriksa(a2);
  float z3 = zValuePeriksa(a3);
  float z4 = zValuePeriksa(a4);
  float z5 = zValuePeriksa(a5);
  float z6 = zValuePeriksa(a6);
  float z7 = zValuePeriksa(a7);
  float z8 = zValuePeriksa(a8);
  float z9 = zValuePeriksa(a9);
  float z10 = zValueSehat(a10);
  float z11 = zValuePeriksa(a11);
  float z12 = zValueSehat(a12);
  float z13 = zValuePeriksa(a13);
  float z14 = zValuePeriksa(a14);
  float z15 = zValuePeriksa(a15);
  float z16 = zValueSehat(a16);
  float z17 = zValuePeriksa(a17);
  float z18 = zValueSehat(a18);
  float z19 = zValuePeriksa(a19);
  float z20 = zValuePeriksa(a20);
  float z21 = zValuePeriksa(a21);
  float z22 = zValuePeriksa(a22);
  float z23 = zValuePeriksa(a23);
  float z24 = zValuePeriksa(a24);

  //Defuzzyfikasi
  float pembilang = a1*z1 + a2*z2 + a3*z3 + a4*z4 + a5*z5 + a6*z6 + a7*z7 + a8*z8 + a9*z9 + a10*z10 + a11*z11 + a12*z12 + a13*z13 + a14*z14 + a15*z15 + a16*z16 + a17*z17 + a18*z18 + a19*z19 + a20*z20 + a21*z21 + a22*z22 + a23*z23 + a24*z24;
  float penyebut = a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8 + a9 + a10 + a11 + a12 + a13 + a14 + a15 + a16 + a17 + a18 + a19 + a20 + a21 + a22 + a23 + a24;
  float crispZ = pembilang / penyebut;

  float periksaZ = miuPeriksaZ(crispZ);
  float sehatZ = miuSehatZ(crispZ);

  String diag;
  if (periksaZ > sehatZ) diag = "PERIKSA";
  else if (periksaZ < sehatZ) diag = "SEHAT";
  debug("Diag   : " + diag);
  return diag;
}


//Fuzzifikasi Gula Darah Sesaat (W)
float miuRendahW(uint16_t &w) {
  float degKeanggotaan;
  if (w >= 85) degKeanggotaan = 0;
  else if (65 < w and w < 85) degKeanggotaan = (85 - w) / 20.0;
  else if (w <= 65) degKeanggotaan = 1;
  return degKeanggotaan;
}

float miuNormalW(uint16_t &w) {
  float degKeanggotaan;
  if (w <= 65 or w >= 160) degKeanggotaan = 0;
  else if (65 < w and w < 85) degKeanggotaan = (w - 65) / 20.0;
  else if (85 <= w and w <= 140) degKeanggotaan = 1;
  else if (140 < w and w < 160) degKeanggotaan = (160 - w) / 20.0;
  return degKeanggotaan;
}

float miuAgakTinggiW(uint16_t &w) {
  float degKeanggotaan;
  if (w <= 140 or w >= 180) degKeanggotaan = 0;
  else if (140 < w and w < 160) degKeanggotaan = (w - 140) / 20.0;
  else if (w == 160) degKeanggotaan = 1;
  else if (160 < w and w < 180) degKeanggotaan = (180 - w) / 20.0;
  return degKeanggotaan;
}

float miuTinggiW(uint16_t &w) {
  float degKeanggotaan;
  if (w <= 160) degKeanggotaan = 0;
  else if (160 < w and w < 180) degKeanggotaan = (w - 160) / 20.0;
  else if (w >= 180) degKeanggotaan = 1;
  return degKeanggotaan;
}

//Fuzzifikasi Detak Jantung (X)
float miuRendahX(uint8_t &x) {
  float degKeanggotaan;
  if (x >= 80) degKeanggotaan = 0;
  else if (60 < x and x < 80) degKeanggotaan = (80 - x) / 20.0;
  else if (x <= 60) degKeanggotaan = 1;
  return degKeanggotaan;
}

float miuNormalX(uint8_t &x) {
  float degKeanggotaan;
  if (x <= 60 or x >= 100) degKeanggotaan = 0;
  else if (60 < x and x < 80) degKeanggotaan = (x - 60) / 20.0;
  else if (x == 80) degKeanggotaan = 1;
  else if (80 < x and x < 100) degKeanggotaan = (100 - x) / 20.0;
  return degKeanggotaan;
}

float miuTinggiX(uint8_t &x) {
  float degKeanggotaan;
  if (x <= 80) degKeanggotaan = 0;
  else if (80 < x and x < 100) degKeanggotaan = (x - 80) / 20.0;
  else if (x >= 100) degKeanggotaan = 1;
  return degKeanggotaan;
}

//Fuzzifikasi Saturasi Oksigen (Y)
float miuRendahY(uint8_t &y) {
  float degKeanggotaan;
  if (y >= 95) degKeanggotaan = 0;
  else if (90 < y and y < 95) degKeanggotaan = (95 - y) / 5.0;
  else if (y <= 90) degKeanggotaan = 1;
  return degKeanggotaan;
}

float miuNormalY(uint8_t &y) {
  float degKeanggotaan;
  if (y <= 90) degKeanggotaan = 0;
  else if (90 < y and y < 95) degKeanggotaan = (y - 90) / 5.0;
  else if (y >= 95) degKeanggotaan = 1;
  return degKeanggotaan;
}

//Agregasi
float zValuePeriksa(float &aPredikat) {
  float zValue = 60 - (20 * aPredikat);
  return zValue;
}

float zValueSehat(float &aPredikat) {
  float zValue = 40 + (20 * aPredikat);
  return zValue;
}

//Keanggotaan Output
float miuPeriksaZ(float &z) {
  float degKeanggotaan;
  if (z >= 60) degKeanggotaan = 0;
  else if (40 < z and z < 60) degKeanggotaan = (60 - z) / 20.0;
  else if (z <= 40) degKeanggotaan = 1;
  return degKeanggotaan;
}

float miuSehatZ(float &z) {
  float degKeanggotaan;
  if (z <= 40) degKeanggotaan = 0;
  else if (40 < z and z < 60) degKeanggotaan = (z - 40) / 20.0;
  else if (z >= 60) degKeanggotaan = 1;
  return degKeanggotaan;
}


void displayPrint() {
  
  if (measurementStage == 0) {
    display.clearDisplay();
    bool _chargingStatus = getChargeState();
    uint8_t _battLevel = getBattLevel();

    if (_chargingStatus == true) {
      static unsigned long _animTimer = 0;
      static uint8_t _animLevel = 0;
      if (millis() - _animTimer > 500) {
        _animLevel += 1;
        if (_animLevel > 4) _animLevel = 0;
        if (_battLevel > 99) _animLevel = 4;
        _animTimer = millis();
      }
      display.drawBitmap(102, 0, chargeIcon, 16, 16, WHITE);
      switch (_animLevel) {
        case 0: display.drawBitmap(118, 0, battLv0, 10, 16, WHITE); break;
        case 1: display.drawBitmap(118, 0, battLv1, 10, 16, WHITE); break;
        case 2: display.drawBitmap(118, 0, battLv2, 10, 16, WHITE); break;
        case 3: display.drawBitmap(118, 0, battLv3, 10, 16, WHITE); break;
        case 4: display.drawBitmap(118, 0, battLv4, 10, 16, WHITE); break;
      }
    } 
    else {
      if (_battLevel <= 5) {
        display.drawBitmap(118, 0, battLv0, 10, 16, WHITE);
      }
      else if (_battLevel > 5 && _battLevel <= 25) {
        display.drawBitmap(118, 0, battLv1, 10, 16, WHITE);
      } 
      else if (_battLevel > 25 && _battLevel <= 50) {
        display.drawBitmap(118, 0, battLv2, 10, 16, WHITE);
      } 
      else if (_battLevel > 50 && _battLevel <= 75) {
        display.drawBitmap(118, 0, battLv3, 10, 16, WHITE);
      } 
      else if (_battLevel > 75) {  // Jika _battLevel di atas 75%
        display.drawBitmap(118, 0, battLv4, 10, 16, WHITE);
      }
    }

    if (guiOn == true) {
      display.drawBitmap(0, 0, apIcon, 16, 16, WHITE);
      display.setCursor(0,20);  display.print("SSID  : "); display.print(apSsid);
      display.setCursor(0,35);  display.print("Pass  : ");  display.print(apPass);
      display.setCursor(0,50);  display.print("IPweb : ");  display.print(WiFi.softAPIP());
    }
    else if (connecting == true) {
      display.setCursor(0, 1);
      display.setTextSize(1);
      display.print("Network");
      display.setCursor(0,35); 
      display.setTextSize(2);
      display.print("Connecting");
    }
    else {
      if (mode == "offline") {
        display.setCursor(0, 1);
        display.setTextSize(1);
        display.print("Offline");
      }
      else display.drawBitmap(0, 0, wifiIcon, 16, 16, WHITE);
      display.setCursor(50, 20);
      display.setTextSize(2);
      display.print("NO");
      display.setCursor(25, 40);
      display.setTextSize(2);
      display.print("FINGER!");
    }
    display.display();
  } 
  else if (measurementStage == 1) {  //menampilkan counter jika sedang sampling
    static unsigned long _loadingTimer = 0;
    static uint8_t _loadingCounter = 1;

    if (millis() - _loadingTimer > 1500) {
      display.clearDisplay();
      display.setCursor(40, 20);
      display.setTextSize(2);
      display.print("Wait");
      display.setCursor(43, 40);
      display.setTextSize(2);
      if (_loadingCounter == 1) display.print(".  ");
      if (_loadingCounter == 2) display.print(".. ");
      if (_loadingCounter == 3) {
        display.print("...");
        _loadingCounter = 0;
      }
      _loadingCounter++;
      
      _loadingTimer = millis();
      display.display();
    }

    // static unsigned long _loadingTimer = 0;
    // static uint8_t _loadingCounter = 10;

    // if (millis() - _loadingTimer > 1000) {
    //   if (_loadingCounter < 1) _loadingCounter = 10;
    //   display.clearDisplay();
    //   display.setCursor(40, 20);
    //   display.setTextSize(2);
    //   display.print("Wait");
    //   display.setCursor(55, 40);
    //   display.setTextSize(2);
    //   display.print(_loadingCounter);
    //   _loadingCounter -= 1;
    //   _loadingTimer = millis();
    //   display.display();
    // }
    
  } 
  else if (measurementStage == 2) { //stage menampiilkan hasil akhir
    bool validHR = false;
    bool validOxSat = false;
    bool validGlucose = false;
    display.clearDisplay();
    
    display.drawBitmap(11, 16, heartRateIcon, 20, 20, WHITE);
    display.drawBitmap(56, 16, oxygenIcon, 20, 20, WHITE);
    display.drawBitmap(99, 16, glucoseIcon, 20, 20, WHITE);

    if (heartRate > 50 && heartRate <= 250) {
      validHR = true;
      if (heartRate < 100) {
        display.setCursor(10, 38);
        display.setTextSize(2);
      } else {
        display.setCursor(3, 38);
        display.setTextSize(2);
      }
      display.print(heartRate);
    } else {
      display.setCursor(10, 38);
      display.setTextSize(2);
      display.print("--");
    }

    if (oxySaturation > 75 && oxySaturation <= 100) {
      validOxSat = true;
      if (oxySaturation < 100) {
        display.setCursor(53, 38);
        display.setTextSize(2);
      } else {
        display.setCursor(45, 38);
        display.setTextSize(2);
      }
      display.print(oxySaturation);
    } else {
      display.setCursor(53, 38);
      display.setTextSize(2);
      display.print("--");
    }

    if (bloodGlucose > 50) {
      validGlucose = true;
      if (bloodGlucose < 100) {
        display.setCursor(96, 38);
        display.setTextSize(2);
      } else {
        display.setCursor(90, 38);
        display.setTextSize(2);
      }
      display.print(bloodGlucose, 1);
    } else {
      display.setCursor(96, 38);
      display.setTextSize(2);
      display.print("--");
    }

    display.setCursor(12, 56);
    display.setTextSize(1);
    display.print("BPM");
    display.setCursor(61, 56);
    display.setTextSize(1);
    display.print("%");
    display.setCursor(94, 56);
    display.setTextSize(1);
    display.print("mg/dl");

    display.setCursor(0, 1);
    display.setTextSize(1);
    display.print("Diag:");
    display.setCursor(32, 1);
    display.setTextSize(2);

    if (validHR == true and validOxSat == true and validGlucose == true) display.print(diagnose);
    else display.print(diagnose = "--");

    display.display();
    while (particleSensor.getIR() > 50000);
  }

  // if (measurementStage != 1) display.display();
}

bool getChargeState() {
  bool _status = digitalRead(4);
  return _status;
}

uint8_t getBattLevel() {
  int _adcValue = analogRead(A0);
  float _battVolt = (_adcValue / 1024.0) * 5.545286;
  int _battLevel = mapBatt(_battVolt, 2.5, 4.18, 0, 100);
  return _battLevel;
}

int mapBatt(float volt, float minVolt, float maxVolt, float minPercent, float maxPercent) {
  return (volt - minVolt) * (maxPercent - minPercent) / (maxVolt - minVolt) + minPercent;
}

//MQTT
void mqttSetup() {
  mqtt.setServer(mqttServer.c_str(), mqttPort.toInt());
  Serial.println("Status  : MQTT SetServer Finish");
}

void mqttConnect() {
  if (!mqtt.connected()) {
    Serial.println("Status  : Reconnecting to MQTT");
    mqtt.connect(mqttId.c_str(), mqttUser.c_str(), mqttPass.c_str()); //menghubungkan ulang
    if (!mqtt.connected()) {
      mqtt.connect(mqttId.c_str(), mqttUser.c_str(), mqttPass.c_str()); //menghubungkan ulang jika gagal
    }
  }
  Serial.println("Status  : Reconnected to MQTT");
}
void sendData_Mqtt(String msg) {
  mqttConnect();
  char finalDataChar[msg.length() + 1];
  char topicChar[mqttTopic.length() + 1];
  msg.toCharArray(finalDataChar, msg.length() + 1);
  mqttTopic.toCharArray(topicChar, mqttTopic.length() + 1);
  debug("send data: " + msg);
  mqtt.publish(topicChar, finalDataChar);
}

//SPIFFS
void spiffSetup() {
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS Mount Failed");
    return;
  }
}

String readFile(const char *path) {
  File file = SPIFFS.open(path, "r");
  if (!file) {
    Serial.println("Failed to open file for reading "+String(path));
    return String();
  }
  String fileContent;
  while (file.available()) {
    fileContent += (char)file.read();
  }
  return fileContent;
}

void writeFile(const char *path, const char *fileContent) {
  File file = SPIFFS.open(path, "w");
  if (file.print(fileContent)) {
    Serial.println("- file written "+String(path));
  } else {
    Serial.println("- write failed "+String(path));
  }
}

void readSetting() {
  wifiSsid = readFile("/wifiSsid.txt");
  wifiPass = readFile("/wifiPass.txt");
  if (wifiSsid == "" && wifiPass == "") {
    writeFile("/mode.txt", "online");
    writeFile("/wifiSsid.txt", "Nasgor");
    writeFile("/wifiPass.txt", "12345678");
    writeFile("/apSsid.txt", "Smart Glucometer");
    writeFile("/apPass.txt", "12345678");
    writeFile("/mqttServer.txt", "mqtt.telkomiot.id");
    writeFile("/mqttPort.txt", "1883");
    writeFile("/mqttId.txt", "DEV664c9619c9d8291809");
    writeFile("/mqttUser.txt", "18f9b32b088c8db8");
    writeFile("/mqttPass.txt", "18f9b32b088bc4ae");
    writeFile("/mqttTopic.txt", "v2.0/pubs/APP664c96067352923409/DEV664c9619c9d8291809");
    writeFile("/nama.txt", "Rifki");
    writeFile("/usia.txt", "22");
    writeFile("/gender.txt", "L");
    wifiSsid = readFile("/wifiSsid.txt");
    wifiPass = readFile("/wifiPass.txt");
  }
  mode = readFile("/mode.txt");
  apSsid = readFile("/apSsid.txt");
  apPass = readFile("/apPass.txt");
  mqttServer = readFile("/mqttServer.txt");
  mqttPort = readFile("/mqttPort.txt");
  mqttId = readFile("/mqttId.txt");
  mqttUser = readFile("/mqttUser.txt");
  mqttPass = readFile("/mqttPass.txt");
  mqttTopic = readFile("/mqttTopic.txt");
  nama = readFile("/nama.txt");
  usia = readFile("/usia.txt");
  gender = readFile("/gender.txt");
} 

//Redirect
class CaptiveRequestHandler : public AsyncWebHandler {
public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest *request){
    return true;
  }

  void handleRequest(AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    request->redirect("/");
  }
};

//WEB Server
void serverSetup() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if (request->hasParam("wifiSsid") && request->hasParam("wifiPass")) {
      mode = request->getParam("mode")->value();
      wifiSsid = request->getParam("wifiSsid")->value();
      wifiPass = request->getParam("wifiPass")->value();
      apSsid = request->getParam("apSsid")->value();
      apPass = request->getParam("apPass")->value();
      mqttServer = request->getParam("mqttServer")->value();
      mqttPort = request->getParam("mqttPort")->value();
      mqttId = request->getParam("mqttId")->value();
      mqttUser = request->getParam("mqttUser")->value();
      mqttPass = request->getParam("mqttPass")->value();
      mqttTopic = request->getParam("mqttTopic")->value();
      nama = request->getParam("nama")->value();
      usia = request->getParam("usia")->value();
      gender = request->getParam("gender")->value();

      writeFile("/mode.txt", mode.c_str());
      writeFile("/wifiSsid.txt", wifiSsid.c_str());
      writeFile("/wifiPass.txt", wifiPass.c_str());
      writeFile("/apSsid.txt", apSsid.c_str());
      writeFile("/apPass.txt", apPass.c_str());
      writeFile("/mqttServer.txt", mqttServer.c_str());
      writeFile("/mqttPort.txt", mqttPort.c_str());
      writeFile("/mqttId.txt", mqttId.c_str());
      writeFile("/mqttUser.txt", mqttUser.c_str());
      writeFile("/mqttPass.txt", mqttPass.c_str());
      writeFile("/mqttTopic.txt", mqttTopic.c_str());
      writeFile("/nama.txt", nama.c_str());
      writeFile("/usia.txt", usia.c_str());
      writeFile("/gender.txt", gender.c_str());
    }
    request->redirect("/");
  });
  server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);//only when requested from AP
  server.begin();
}

String processor(const String& var){
  if (var == "mode") return mode;
  else if (var == "wifiSsid") return wifiSsid;
  else if (var == "wifiPass") return wifiPass;
  else if (var == "apSsid") return apSsid;
  else if (var == "apPass") return apPass;
  else if (var == "mqttServer") return mqttServer;
  else if (var == "mqttPort") return mqttPort;
  else if (var == "mqttId") return mqttId;
  else if (var == "mqttUser") return mqttUser;
  else if (var == "mqttPass") return mqttPass;
  else if (var == "mqttTopic") return mqttTopic; 
  else if (var == "nama") return nama;
  else if (var == "usia") return usia;
  else if (var == "gender") return gender;
  else return String();
}