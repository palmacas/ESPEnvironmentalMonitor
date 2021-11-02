/*
   ESPEnvironmentalMonitor

   Environmental monitor using an ESP8266, a BME280 sensor and Netion Display

   https://palmacas.com/monitor-ambiente-esp8266
*/

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Arduino_JSON.h>
#include <time.h>

#include "settings.h"

// Create a server on port 80
AsyncWebServer server(80);
AsyncEventSource events("/events");
AsyncEventSource local_events("/local_events");

// Connection using I2C
Adafruit_BME280 bme;
#define SDA_PIN 0
#define SCL_PIN 2

// Sea level pressure definition
#define SEALEVELPRESSURE_HPA (1013.25)

// Variable to save OpenWeatherMap reply
String json_buffer;

// NTP server
const char* ntpServer = "pool.ntp.org";

// Measured variables
int temperature;
int feels_like;
int pressure;
int humidity;
int wind;
String weather;
double l_temperature;
int l_pressure;
int l_humidity;
int l_altitude;
String l_date = "12-SEP-2021";
String l_time = "12:23 PM";

// Time of last update
unsigned long last_time = 0;
unsigned long last_time_long = 0;

// Updates every 10 seconds and 15 minutes
unsigned long interval = 1000 * 10;
unsigned long interval_long = 1000 * 900;

void sendCmd(String cmd);
void connectWiFi();
String processor(const String& var);
void readBME280();
void getWeather();
String httpGet(const char* server_name);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta content="width=device-width, initial-scale=1" name="viewport">
    <title>ESP Monitor Ambiental</title>
    <link href="https://use.fontawesome.com/releases/v5.14.0/css/all.css" rel="stylesheet">
    <link href="https://fonts.googleapis.com/css2?family=Roboto&display=swap" rel="stylesheet">
    <style>
        html {font-family: 'Roboto', sans-serif; display: inline-block; text-align: center; background-color:#eeeeee;}
        p {font-size: 1.2rem;}
        a {color: black; text-decoration: none;}
        body {margin: 0}
        .topnav {overflow: hidden; background-color: #00286b; color: #ededed; font-size: 1.7rem;}
        .content {padding: 10px;}
        .cards {max-width: 700px; margin: 0 auto; display: grid; grid-gap: 0.5rem; grid-template-columns: repeat(2, minmax(40%, 1fr));}
        .header {grid-column: 1 /span 2;}
        .card {box-shadow: 1px 1px 5px 1px rgba(140, 140, 140, .5); border-radius: 5px;}
        .reading {font-size: 2rem;}
        .reading-big {font-size: 5rem;}
        .fas{ color: #00286b;}
    </style>
</head>
<body>
<div class="topnav">
    <h4>MONITOR AMBIENTAL</h4>
</div>
<div class="content">
    <div class="cards">
        <div class="header">
            <h4><i class="fas fa-map-marker-alt"></i><span id="city"> %CITY%</span>, <span id="country">%COUNTRY%</span></h4>
            <span class="reading-big"><span id="temp">%TEMPERATURE%</span>&deg;C</span>
            <h4><i class="fas fa-cloud"></i> <span id="weather">%WEATHER%</span></h4>
        </div>
        <div class="card">
            <h4><i class="fas fa-thermometer-half"></i> SENSACIÓN</h4>
            <span class="reading"><span id="feels_like">%FEELS_LIKE%</span>&deg;C</span>
        </div>
        <div class="card">
            <h4><i class="fas fa-tachometer-alt"></i> PRESIÓN</h4>
            <span class="reading"><span id="press">%PRESSURE%</span> hPa</span>
        </div>
        <div class="card">
            <h4><i class="fas fa-tint"></i> HUMEDAD</h4>
            <span class="reading"><span id="hum">%HUMIDITY%</span>&percnt;</span>
        </div>
        <div class="card">
            <h4><i class="fas fa-wind"></i> VIENTO</h4>
            <span class="reading"><span id="wind">%WIND%</span> km/h</span>
        </div>
        <div class="header">
            <h4>LECTURAS LOCALES</h4>
        </div>
        <div class="card">
            <h4><i class="fas fa-thermometer-half"></i> TEMPERATURA</h4>
            <span class="reading"><span id="l_temp">%L_TEMPERATURE%</span>&deg;C</span>
        </div>
        <div class="card">
            <h4><i class="fas fa-tachometer-alt"></i> PRESIÓN</h4>
            <span class="reading"><span id="l_press">%L_PRESSURE%</span> hPa</span>
        </div>
        <div class="card">
            <h4><i class="fas fa-tint"></i> HUMEDAD</h4>
            <span class="reading"><span id="l_hum">%L_HUMIDITY%</span>&percnt;</span>
        </div>
        <div class="card">
            <h4><i class="fas fa-mountain"></i> ALTITUD</h4>
            <span class="reading"><span id="l_alt">%L_ALTITUDE%</span> m</span>
        </div>
    </div>
</div>
<p>v1.0<br>
    <a href="https://palmacas.com/" target="_blank">palmacas</a> 2021<br></p>
<script>
    if (!!window.EventSource) {
    var source = new EventSource('/events');
    source.addEventListener('open', function(e) {console.log("Events Connected"); }, false);
    source.addEventListener('error', function(e) {if (e.target.readyState != EventSource.OPEN) {console.log("Events Disconnected"); } }, false);
    source.addEventListener('message', function(e) {console.log("message", e.data); }, false);
    source.addEventListener('temperature', function(e) {console.log("temperature", e.data); document.getElementById("temp").innerHTML = e.data; }, false);
    source.addEventListener('weather', function(e) {console.log("weather", e.data); document.getElementById("weather").innerHTML = e.data; }, false);
    source.addEventListener('feels_like', function(e) {console.log("feels_like", e.data); document.getElementById("feels_like").innerHTML = e.data; }, false);
    source.addEventListener('pressure', function(e) {console.log("pressure", e.data); document.getElementById("press").innerHTML = e.data; }, false);
    source.addEventListener('humidity', function(e) {console.log("humidity", e.data); document.getElementById("hum").innerHTML = e.data; }, false);
    source.addEventListener('wind', function(e) {console.log("wind", e.data); document.getElementById("wind").innerHTML = e.data; }, false);
    }
    if (!!window.EventSource) {
    var source = new EventSource('/local_events');
    source.addEventListener('open', function(e) {console.log("Events Connected"); }, false);
    source.addEventListener('error', function(e) {if (e.target.readyState != EventSource.OPEN) {console.log("Events Disconnected"); } }, false);
    source.addEventListener('message', function(e) {console.log("message", e.data); }, false);
    source.addEventListener('l_temperature', function(e) {console.log("l_temperature", e.data); document.getElementById("l_temp").innerHTML = e.data; }, false);
    source.addEventListener('l_pressure', function(e) {console.log("l_pressure", e.data); document.getElementById("l_press").innerHTML = e.data; }, false);
    source.addEventListener('l_humidity', function(e) {console.log("l_humidity", e.data); document.getElementById("l_hum").innerHTML = e.data; }, false);
    source.addEventListener('l_altitude', function(e) {console.log("l_altitude", e.data); document.getElementById("l_alt").innerHTML = e.data; }, false);
    }
</script>
</body>
</html>)rawliteral";

void setup() {
  Serial.begin(115200); // Start communication with Nextion display
  sendCmd("");
  sendCmd("n_status.txt=\"Starting...\"");

  delay(250);

  connectWiFi();

  delay(250);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", index_html, processor);
  });
  server.addHandler(&events);
  server.addHandler(&local_events);
  server.begin();
  sendCmd("n_status.txt:\"Server started...\"");

  delay(250);

  // Start sensor communication
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!bme.begin(0x76, &Wire)) {
    sendCmd("n_status.txt:\"Could not find a valid BME280 sensor\"");
    Serial.println("Could not find a valid BME280 sensor, check wiring, address, sensor ID!");
    while (1) delay(10);
  }

  sendCmd("page 1");
  readBME280();
  getWeather();
}

void loop() {
  if ((millis() - last_time) > interval) {
    readBME280();
    local_events.send("ping", NULL, millis());
    local_events.send(String(l_temperature).c_str(), "l_temperature", millis());
    local_events.send(String(l_pressure).c_str(), "l_pressure", millis());
    local_events.send(String(l_humidity).c_str(), "l_humidity", millis());
    local_events.send(String(l_altitude).c_str(), "l_altitude", millis());

    last_time = millis();
  }
  if ((millis() - last_time_long) > interval_long) {
    getWeather();
    events.send("ping", NULL, millis());
    events.send(String(temperature).c_str(), "temperature", millis());
    events.send(String(feels_like).c_str(), "feels_like", millis());
    events.send(String(weather).c_str(), "weather", millis());
    events.send(String(pressure).c_str(), "pressure", millis());
    events.send(String(humidity).c_str(), "humidity", millis());
    events.send(String(wind).c_str(), "wind", millis());

    last_time_long = millis();
  }
  sendCmd("n_date.txt=\"" + l_date + "\"");
  sendCmd("n_time.txt=\"" + l_time + "\"");
  sendCmd("n_temp.txt=\"" + String(temperature) + "\"");
  sendCmd("n_feel.txt=\"" + String(feels_like) + "\"");
  sendCmd("n_weather.txt=\"" + String(weather) + "\"");
  sendCmd("n_city.txt=\"" + city + "\"");
  sendCmd("n_press.txt=\"" + String(pressure) + "\"");
  sendCmd("n_hum.txt=\"" + String(humidity) + "\"");
  sendCmd("n_wind.txt=\"" + String(wind) + "\"");
  sendCmd("n_ltemp.txt=\"" + String(l_temperature) + "\"");
  sendCmd("n_lpress.txt=\"" + String(l_pressure) + "\"");
  sendCmd("n_lhum.txt=\"" + String(l_humidity) + "\"");
  sendCmd("n_lalt.txt=\"" + String(l_altitude) + "\"");
  sendCmd("qr0.txt=\"http://" + String(WiFi.localIP().toString()) + "\"");

  delay(5000);
}

void sendCmd(String cmd)
{
  Serial.print(cmd);
  Serial.print("\xFF\xFF\xFF");
}

void connectWiFi() {
  sendCmd("n_status.txt=\"Connecting to " + ssid + "\"");
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.hostname(wifi_hostname);
  WiFi.begin(ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }
  sendCmd("n_status.txt=\"WiFi Connected\"");
}

String processor(const String& var) {
  readBME280();
  if (var == "CITY") {
    return String(city);
  }
  else if (var == "COUNTRY") {
    return String(country_code);
  }
  else if (var == "TEMPERATURE") {
    return String(temperature);
  }
  else if (var == "WEATHER") {
    return String(weather);
  }
  else if (var == "FEELS_LIKE") {
    return String(feels_like);
  }
  else if (var == "PRESSURE") {
    return String(pressure);
  }
  else if (var == "HUMIDITY") {
    return String(humidity);
  }
  else if (var == "WIND") {
    return String(wind);
  }
  else if (var == "L_TEMPERATURE") {
    return String(l_temperature);
  }
  else if (var == "L_PRESSURE") {
    return String(l_pressure);
  }
  else if (var == "L_HUMIDITY") {
    return String(l_humidity);
  }
  else if (var == "L_ALTITUDE") {
    return String(l_altitude);
  }
}

void readBME280() {
  if (pressure != 0) {
    #define SEALEVELPRESSURE_HPA (pressure)
  }
  l_temperature = bme.readTemperature();
  l_pressure = round(bme.readPressure() / 100.0F);
  l_humidity = round(bme.readHumidity());
  l_altitude = round(bme.readAltitude(SEALEVELPRESSURE_HPA));
}

void getWeather() {
  if (WiFi.status() == WL_CONNECTED) {
    String server_path = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "," + country_code + "&lang=" + lang + "&units=" + units + "&appid=" + API_key;
    json_buffer = httpGet(server_path.c_str());
    JSONVar weather_data = JSON.parse(json_buffer);

    double temperature_k = weather_data["main"]["temp"];
    temperature = round(temperature_k);
    double feels_like_k = weather_data["main"]["feels_like"];
    feels_like = round(feels_like_k);
    double pressure_f = weather_data["main"]["pressure"];
    pressure = round(pressure_f);
    double humidity_f = weather_data["main"]["humidity"];
    humidity = round(humidity_f);
    double wind_ms = weather_data["wind"]["speed"];
    wind = round(wind_ms * 3.6);
    weather = weather_data["weather"][0]["description"];
    weather.toUpperCase();
  }
  else {
    Serial.println("WiFi Disconnected");
  }
}

String httpGet(const char* server_name) {
  HTTPClient http;

  // Domain name with IP, port and URL
  http.begin(server_name);
  // Send request
  int http_code = http.GET();

  String payload = "{}";
  if (http_code > 0) {
    payload = http.getString();
  }
  else {
  }

  // Free resources
  http.end();
  return payload;
}
