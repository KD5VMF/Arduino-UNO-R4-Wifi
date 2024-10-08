#include <WiFiS3.h>
#include <Wire.h>
#include <Adafruit_GPS.h>
#include <SoftwareSerial.h>
#include <Adafruit_MCP9808.h>
#include <Adafruit_Si7021.h>
#include <RTClib.h>
#include <Adafruit_LIS3MDL.h>    // Magnetometer
#include <WiFiUdp.h>             // For NTP
#include <NTPClient.h>           // NTP Client library
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "secrets.h"  // Include your WiFi credentials here

// Web server on port 80
WiFiServer server(80);

// GPS setup using SoftwareSerial (pins 8 for RX and 7 for TX)
SoftwareSerial mySerial(8, 7);
Adafruit_GPS GPS(&mySerial);

// Sensors setup
Adafruit_MCP9808 mcp9808 = Adafruit_MCP9808();
Adafruit_Si7021 si7021 = Adafruit_Si7021();
Adafruit_LIS3MDL lis3mdl = Adafruit_LIS3MDL();

// RTC setup
RTC_PCF8523 rtc;

// Onboard LED pin for Arduino R4
#define LED_PIN LED_BUILTIN

// Variables for timing
unsigned long previousMillis = 0;
const long interval = 1000;  // 1 second interval

// Variables to store user-selected time zone and DST status
int selectedTimeZone = 0;
bool isDSTOn = false;

int connections = 0;      // Total number of connections made
int currentClients = 0;   // Number of currently connected clients

// NTP Client setup
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000); // Update every 60 seconds

// Magnetometer calibration offsets
float magOffsetX = 0;
float magOffsetY = 0;
float magOffsetZ = 0;

// Flags to indicate sensor initialization
bool lis3mdlInitialized = false;
bool mcp9808Initialized = false;
bool si7021Initialized = false;
bool rtcInitialized = false;
bool ntpSynced = false;

// OLED display setup
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Variables for IP address display
int xPos = 0;
int yPos = 0;
int xDir = 1;  // Direction of movement in x (1 or -1)
int yDir = 1;  // Direction of movement in y (1 or -1)
unsigned long previousDisplayMillis = 0;
const long displayInterval = 100; // Update every 100 ms
String IP_Address_String = "";

// Variables to track RTC last update
DateTime rtcLastUpdate;
String rtcLastUpdateMethod = "N/A"; // "NTP", "GPS", or "N/A"

void setup() {
  Serial.begin(115200);

  // Initialize onboard LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Ensure LED is off initially

  // Initialize GPS
  mySerial.begin(9600);
  GPS.begin(9600);
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);    // Update rate to 1 Hz
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA); // Output RMC and GGA sentences
  delay(1000);

  // Initialize RTC
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    rtcInitialized = false;
  } else {
    rtcInitialized = true;
    Serial.println("RTC initialized.");
  }

  // Initialize MCP9808
  if (!mcp9808.begin()) {
    Serial.println("Couldn't find MCP9808 sensor!");
  } else {
    Serial.println("MCP9808 sensor initialized.");
    mcp9808Initialized = true;
  }

  // Initialize Si7021
  if (!si7021.begin()) {
    Serial.println("Couldn't find Si7021 sensor!");
  } else {
    Serial.println("Si7021 sensor initialized.");
    si7021Initialized = true;
  }

  // Initialize LIS3MDL Magnetometer
  if (!lis3mdl.begin_I2C()) { // Default address 0x1E
    Serial.println("Couldn't find LIS3MDL magnetometer!");
  } else {
    Serial.println("LIS3MDL magnetometer initialized.");
    lis3mdlInitialized = true;
  }

  // Connect to the existing WiFi network
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");

  // Wait until connected
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Get IP address string
  IPAddress ip = WiFi.localIP();
  IP_Address_String = ip.toString();

  // Initialize OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for SSD1306
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display.clearDisplay();
  display.display();

  // Start NTP client
  timeClient.begin();

  // Attempt to synchronize RTC via NTP
  synchronizeRTC();

  // Start the server
  server.begin();
}

void loop() {
  // GPS reading
  if (GPS.available()) {
    while (GPS.available()) {
      char c = GPS.read();
      if (GPS.newNMEAreceived()) {
        if (!GPS.parse(GPS.lastNMEA())) {
          // Failed to parse GPS data
          Serial.println("Failed to parse GPS data.");
        } else {
          // Toggle onboard LED when GPS data is received
          digitalWrite(LED_PIN, !digitalRead(LED_PIN));

          // If GPS has a fix and NTP hasn't synced yet, use GPS to set RTC
          if (GPS.fix && GPS.hour != 0 && !ntpSynced && rtcInitialized) {
            rtc.adjust(DateTime(GPS.year + 2000, GPS.month, GPS.day, GPS.hour, GPS.minute, GPS.seconds));
            rtcLastUpdate = rtc.now();
            rtcLastUpdateMethod = "GPS";
            Serial.println("RTC synchronized with GPS data.");
          }
        }
      }
    }
  } else {
    // GPS not available
    // Optionally, handle GPS failure here
  }

  // Handle NTP synchronization periodically
  if (timeClient.update()) {
    if (rtcInitialized) {
      // Get epoch time from NTP
      unsigned long epochTime = timeClient.getEpochTime();
      DateTime ntpTime = DateTime(epochTime);
      rtc.adjust(ntpTime);
      ntpSynced = true;
      rtcLastUpdate = rtc.now();
      rtcLastUpdateMethod = "NTP";
      Serial.println("RTC synchronized with NTP.");
    } else {
      Serial.println("RTC not initialized. Cannot synchronize with NTP.");
    }
  }

  // Update OLED display
  unsigned long currentMillis = millis();
  if (currentMillis - previousDisplayMillis >= displayInterval) {
    previousDisplayMillis = currentMillis;
    
    // Update position
    xPos += xDir;
    yPos += yDir;

    // Get text bounds
    int16_t x1, y1;
    uint16_t w, h;
    display.setTextSize(1);
    display.getTextBounds(IP_Address_String, xPos, yPos, &x1, &y1, &w, &h);

    // Check for collision with screen edges and reverse direction if needed
    if ((xPos + w) >= SCREEN_WIDTH || xPos <= 0) {
      xDir = -xDir;
      xPos += xDir;
    }

    if ((yPos + h) >= SCREEN_HEIGHT || yPos <= 0) {
      yDir = -yDir;
      yPos += yDir;
    }

    // Clear display
    display.clearDisplay();

    // Set text size and color
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    // Set cursor position
    display.setCursor(xPos, yPos);

    // Display IP address
    display.print(IP_Address_String);

    // Update the display
    display.display();
  }

  // Check for client connections
  WiFiClient client = server.available();
  if (client) {
    connections++;      // Increase total connections
    currentClients++;   // Track active client
    String request = client.readStringUntil('\r');
    client.flush();

    if (request.indexOf("GET / ") >= 0) {
      serveHTML(client);
    } else if (request.indexOf("GET /data") >= 0) {
      serveData(client);
    } else if (request.indexOf("GET /setTimeZone") >= 0) {
      handleTimeZoneSelection(request);
      // Respond with a confirmation message
      client.println("HTTP/1.1 200 OK");
      client.println("Content-type:text/plain");
      client.println();
      client.println("Time zone and DST settings updated.");
    } else if (request.indexOf("GET /zeroMagnetometer") >= 0) {
      zeroMagnetometer();
      // Respond with a confirmation message
      client.println("HTTP/1.1 200 OK");
      client.println("Content-type:text/plain");
      client.println();
      client.println("Magnetometer calibration offsets have been updated.");
    }

    client.stop();
    currentClients--;   // Decrease active client count
  }
}

// Serve the enhanced HTML + JavaScript page
void serveHTML(WiFiClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println();

  client.println("<!DOCTYPE html><html><head><title>Live Sensor Data</title>");
  client.println("<style>");
  
  // Enhanced CSS for better spacing and table styling
  client.println("body {");
  client.println("  font-family: 'Courier New', Courier, monospace;");
  client.println("  font-size: 20px;");
  client.println("  color: #00FF00;");
  client.println("  background-color: #000000;");
  client.println("  margin: 0;");
  client.println("  padding: 20px;");
  client.println("  display: flex;");
  client.println("  flex-direction: column;");
  client.println("  align-items: center;");
  client.println("}");

  client.println(".container {");
  client.println("  display: flex;");
  client.println("  flex-wrap: wrap;");
  client.println("  justify-content: space-between;");
  client.println("  width: 100%;");
  client.println("  max-width: 1200px;");
  client.println("}");

  client.println(".column {");
  client.println("  flex: 1;");
  client.println("  padding: 20px;");
  client.println("  margin: 10px;");
  client.println("  box-sizing: border-box;");
  client.println("  background-color: #1A1A1A;");
  client.println("  border: 1px solid #333333;");
  client.println("  border-radius: 5px;");
  client.println("}");

  client.println("h2 {");
  client.println("  text-align: center;");
  client.println("  color: #00FF00;");
  client.println("}");

  client.println("table {");
  client.println("  width: 100%;");
  client.println("  border-collapse: collapse;");
  client.println("}");

  client.println("th, td {");
  client.println("  text-align: left;");
  client.println("  padding: 8px;");
  client.println("}");

  client.println("th {");
  client.println("  background-color: #333333;");
  client.println("  color: #FFFFFF;");
  client.println("  width: 40%;");
  client.println("}");

  client.println("td {");
  client.println("  background-color: #1A1A1A;");
  client.println("}");

  client.println("button {");
  client.println("  padding: 10px 20px;");
  client.println("  font-size: 16px;");
  client.println("  margin-top: 20px;");
  client.println("  cursor: pointer;");
  client.println("  background-color: #00FF00;");
  client.println("  color: #000000;");
  client.println("  border: none;");
  client.println("  border-radius: 5px;");
  client.println("}");

  client.println("button:hover {");
  client.println("  background-color: #33FF33;");
  client.println("}");

  client.println("select, input[type='checkbox'] {");
  client.println("  font-size: 16px;");
  client.println("  margin-top: 10px;");
  client.println("}");

  client.println(".footer {");
  client.println("  position: fixed;");
  client.println("  bottom: 10px;");
  client.println("  left: 20px;");
  client.println("  color: #00FF00;");
  client.println("}");

  client.println("@media (max-width: 768px) {");
  client.println("  .container {");
  client.println("    flex-direction: column;");
  client.println("    align-items: center;");
  client.println("  }");
  client.println("  .column {");
  client.println("    margin: 10px 0;");
  client.println("  }");
  client.println("}");

  client.println("</style>");
  client.println("<script type=\"text/javascript\">");
  
  client.println("function fetchData() {");
  client.println("  var xhr = new XMLHttpRequest();");
  client.println("  xhr.onreadystatechange = function() {");
  client.println("    if (this.readyState == 4 && this.status == 200) {");
  client.println("      var data = JSON.parse(this.responseText);");
  client.println("      document.getElementById('gpsData').innerHTML = data.gps;");
  client.println("      document.getElementById('sensorData').innerHTML = data.sensor;");
  client.println("      document.getElementById('rtcData').innerHTML = data.rtc;");
  client.println("    }");
  client.println("  };");
  client.println("  xhr.open('GET', '/data', true);");
  client.println("  xhr.send();");
  client.println("}");
  client.println("setInterval(fetchData, 5000);"); // Fetch data every 5 seconds

  // Timezone and DST Selection
  client.println("function setTimeZone() {");
  client.println("  var timeZone = document.getElementById('timeZoneSelect').value;");
  client.println("  var isDST = document.getElementById('dstCheckbox').checked ? 1 : 0;");
  client.println("  var xhr = new XMLHttpRequest();");
  client.println("  xhr.onreadystatechange = function() {");
  client.println("    if (this.readyState == 4 && this.status == 200) {");
  client.println("      alert(this.responseText);"); // Display confirmation from server
  client.println("    }");
  client.println("  };");
  client.println("  xhr.open('GET', '/setTimeZone?tz=' + timeZone + '&dst=' + isDST, true);");
  client.println("  xhr.send();");
  client.println("}");

  // Function to zero out magnetometer calibration offsets
  client.println("function zeroMagnetometer() {");
  client.println("  var xhr = new XMLHttpRequest();");
  client.println("  xhr.onreadystatechange = function() {");
  client.println("    if (this.readyState == 4 && this.status == 200) {");
  client.println("      alert(this.responseText);"); // Display confirmation from server
  client.println("      fetchData();"); // Refresh data to reflect changes
  client.println("    }");
  client.println("  };");
  client.println("  xhr.open('GET', '/zeroMagnetometer', true);");
  client.println("  xhr.send();");
  client.println("}");
  
  client.println("</script></head><body onload=\"fetchData()\">");
  client.println("<h1>Live GPS, Sensor, and RTC Data</h1>");

  // Container for three columns
  client.println("<div class=\"container\">");

  // GPS Data Column
  client.println("<div class=\"column\" id=\"gpsData\"><h2>GPS Data</h2><p>Loading GPS data...</p></div>");

  // Sensor Data Column
  client.println("<div class=\"column\" id=\"sensorData\"><h2>Sensor Data</h2><p>Loading sensor data...</p></div>");

  // RTC Data Column
  client.println("<div class=\"column\" id=\"rtcData\"><h2>RTC Data</h2><p>Loading RTC data...</p></div>");

  client.println("</div>"); // Close container

  // Time zone and DST selection dropdowns
  client.println("<div style=\"width: 100%; text-align: center; padding-top: 20px;\">");
  client.println("<h2>Select Time Zone</h2>");
  client.println("<select id='timeZoneSelect'>");
  // Generate options dynamically to reduce code
  for (int tz = -12; tz <= 12; tz++) {
    String tzStr = "UTC ";
    if (tz >= 0) tzStr += "+";
    tzStr += String(tz) + ":00";
    client.println("<option value='" + String(tz) + "'>" + tzStr + "</option>");
  }
  client.println("</select><br><br>");

  client.println("<input type='checkbox' id='dstCheckbox'> Daylight Saving Time<br><br>");
  client.println("<button onclick='setTimeZone()'>Set Time Zone</button>");

  // Zero Magnetometer Button
  client.println("<br><br>"); // Add some spacing
  client.println("<button onclick='zeroMagnetometer()'>Zero Magnetometer</button>");
  client.println("</div>");

  // Connection stats
  client.println("<div class=\"footer\">Connections: " + String(connections) + "<br>Active Clients: " + String(currentClients) + "</div>");
  client.println("</body></html>");
}

// Serve live sensor data as JSON
void serveData(WiFiClient &client) {
  String gpsData, rtcData, sensorData;

  // Get GPS data
  if (GPS.fix) {
    gpsData = "<table>";
    gpsData += "<tr><th>Latitude</th><td>" + String(GPS.latitudeDegrees, 6) + "°</td></tr>";
    gpsData += "<tr><th>Longitude</th><td>" + String(GPS.longitudeDegrees, 6) + "°</td></tr>";
    gpsData += "<tr><th>Speed</th><td>" + String(GPS.speed) + " knots</td></tr>";
    gpsData += "<tr><th>Altitude</th><td>" + String(GPS.altitude) + " meters</td></tr>";
    gpsData += "<tr><th>Satellites</th><td>" + String(GPS.satellites) + "</td></tr>";
    gpsData += "<tr><th>Fix Quality</th><td>" + String(GPS.fixquality) + "</td></tr>";
    gpsData += "<tr><th>Course</th><td>" + String(GPS.angle) + "°</td></tr>";
    gpsData += "<tr><th>HDOP</th><td>" + String(GPS.HDOP) + "</td></tr>";
    gpsData += "<tr><th>VDOP</th><td>" + String(GPS.VDOP) + "</td></tr>";
    gpsData += "<tr><th>PDOP</th><td>" + String(GPS.PDOP) + "</td></tr>";
    gpsData += "<tr><th>Fix Time</th><td>" + addLeadingZeros(GPS.hour, 2) + ":" + addLeadingZeros(GPS.minute, 2) + ":" + addLeadingZeros(GPS.seconds, 2) + "</td></tr>";
    gpsData += "</table>";
  } else {
    gpsData = "<p>Waiting for GPS fix...</p>";
  }

  // Get RTC data
  if (rtcInitialized) {
    DateTime now = rtc.now();
    int adjustedHour = now.hour() + selectedTimeZone;
    if (isDSTOn) {
      adjustedHour++;
    }

    // Handle hour overflow/underflow
    if (adjustedHour >= 24) adjustedHour -= 24;
    if (adjustedHour < 0) adjustedHour += 24;

    rtcData = "<table>";
    rtcData += "<tr><th>Date</th><td>" + addLeadingZeros(now.month(), 2) + "/" + addLeadingZeros(now.day(), 2) + "/" + String(now.year()) + "</td></tr>";
    rtcData += "<tr><th>Time</th><td>" + addLeadingZeros(adjustedHour, 2) + ":" + addLeadingZeros(now.minute(), 2) + ":" + addLeadingZeros(now.second(), 2) + "</td></tr>";
    // Add last update time and method
    rtcData += "<tr><th>Last Update</th><td>" + rtcLastUpdate.timestamp(DateTime::TIMESTAMP_FULL) + "</td></tr>";
    rtcData += "<tr><th>Update Method</th><td>" + rtcLastUpdateMethod + "</td></tr>";
    rtcData += "</table>";
  } else {
    rtcData = "<p>RTC not initialized.</p>";
  }

  // Get sensor data from MCP9808 and Si7021
  float tempC_MCP = mcp9808.readTempC();  // Best from MCP9808
  float tempF_MCP = tempC_MCP * 9.0 / 5.0 + 32.0;
  float humidity_Si7021 = si7021.readHumidity();

  sensorData = "<table>";
  if (mcp9808Initialized) {
    sensorData += "<tr><th>Temperature (MCP9808)</th><td>" + String(tempC_MCP, 2) + " °C / " + String(tempF_MCP, 2) + " °F</td></tr>";
  } else {
    sensorData += "<tr><th>Temperature (MCP9808)</th><td>Not Initialized</td></tr>";
  }

  if (si7021Initialized) {
    sensorData += "<tr><th>Humidity (Si7021)</th><td>" + String(humidity_Si7021, 2) + " %</td></tr>";
  } else {
    sensorData += "<tr><th>Humidity (Si7021)</th><td>Not Initialized</td></tr>";
  }

  // Get Magnetometer data from LIS3MDL
  if (lis3mdlInitialized) {
    sensors_event_t event;
    lis3mdl.getEvent(&event);

    // Apply calibration offsets
    float calibratedX = event.magnetic.x - magOffsetX;
    float calibratedY = event.magnetic.y - magOffsetY;
    float calibratedZ = event.magnetic.z - magOffsetZ;

    sensorData += "<tr><th>Magnetometer X</th><td>" + String(calibratedX, 2) + " µT</td></tr>";
    sensorData += "<tr><th>Magnetometer Y</th><td>" + String(calibratedY, 2) + " µT</td></tr>";
    sensorData += "<tr><th>Magnetometer Z</th><td>" + String(calibratedZ, 2) + " µT</td></tr>";
  } else {
    sensorData += "<tr><th>Magnetometer</th><td>Not Initialized</td></tr>";
    Serial.println("Error: LIS3MDL magnetometer not initialized.");
  }

  sensorData += "</table>";

  // Create JSON response
  String jsonResponse = "{";
  jsonResponse += "\"gps\":\"" + gpsData + "\",";
  jsonResponse += "\"sensor\":\"" + sensorData + "\",";
  jsonResponse += "\"rtc\":\"" + rtcData + "\"";
  jsonResponse += "}";

  // Debugging: Print JSON response to Serial Monitor
  Serial.println("Serving Data:");
  Serial.println(jsonResponse);

  // Serve the live data
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type: application/json");  // Serve as JSON
  client.println("Access-Control-Allow-Origin: *"); // Allow cross-origin (optional)
  client.println();
  client.println(jsonResponse);
}

// Handle time zone selection
void handleTimeZoneSelection(String request) {
  int tzIndex = request.indexOf("tz=");
  int dstIndex = request.indexOf("dst=");

  if (tzIndex != -1 && dstIndex != -1) {
    String tzString = request.substring(tzIndex + 3, request.indexOf("&", tzIndex));
    String dstString = request.substring(dstIndex + 4, request.indexOf(" ", dstIndex));

    selectedTimeZone = tzString.toInt();
    isDSTOn = (dstString == "1");

    Serial.println("Time zone selected: UTC " + String(selectedTimeZone));
    Serial.println("DST: " + String(isDSTOn ? "On" : "Off"));
  }
}

// Function to zero out magnetometer calibration offsets
void zeroMagnetometer() {
  if (lis3mdlInitialized) {
    sensors_event_t event;
    lis3mdl.getEvent(&event);
    magOffsetX = event.magnetic.x;
    magOffsetY = event.magnetic.y;
    magOffsetZ = event.magnetic.z;
    Serial.println("Magnetometer calibration offsets have been updated.");
  } else {
    Serial.println("LIS3MDL magnetometer not initialized. Cannot zero offsets.");
  }
}

// Function to add leading zeros
String addLeadingZeros(int number, int width) {
  String result = String(number);
  while (result.length() < width) {
    result = "0" + result;
  }
  return result;
}

// Function to synchronize RTC via NTP
void synchronizeRTC() {
  Serial.println("Synchronizing RTC with NTP...");
  timeClient.update();

  if (timeClient.getEpochTime() > 0 && rtcInitialized) { // Simple check to see if NTP time is valid
    unsigned long epochTime = timeClient.getEpochTime();
    DateTime ntpTime = DateTime(epochTime);
    rtc.adjust(ntpTime);
    ntpSynced = true;
    rtcLastUpdate = rtc.now();
    rtcLastUpdateMethod = "NTP";
    Serial.println("RTC synchronized with NTP.");
  } else {
    Serial.println("NTP synchronization failed or RTC not initialized. Will use GPS as fallback.");
  }
}
