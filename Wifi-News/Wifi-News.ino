#include <ArduinoGraphics.h>
#include <Arduino_LED_Matrix.h>
#include <WiFiS3.h>
#include "TextScroll.h"
#include "secrets.h"  // Include the secrets file

WiFiClient client;

const char* server = "newsapi.org";
String apiEndpoint = "/v2/top-headlines?country=us&apiKey=" + String(NEWS_API_KEY);

TextScroller scroller;

void setup() {
  Serial.begin(115200);  // Start serial communication at 115200 baud rate
  Serial.println("Serial started at 115200");

  scroller.init();  // Initialize the matrix
  Serial.println("Matrix initialized");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connecting to News API...");

    if (client.connect(server, 80)) {
      Serial.println("Connected to News API server");

      // Send HTTP request to the server with User-Agent header
      client.print(String("GET ") + apiEndpoint + " HTTP/1.1\r\n" +
                   "Host: " + server + "\r\n" +
                   "User-Agent: Arduino/1.0\r\n" +
                   "Connection: close\r\n\r\n");

      String response = "";
      while (client.connected() || client.available()) {
        if (client.available()) {
          char c = client.read();
          response += c;
        }
      }
      
      Serial.println("HTTP response received:");
      Serial.println(response);  // Print the full response for debugging

      // Find the start of the payload (after the HTTP headers)
      int payloadStart = response.indexOf("\r\n\r\n") + 4;
      String jsonResponse = response.substring(payloadStart);

      Serial.println("Extracted JSON payload:");
      Serial.println(jsonResponse);  // Print the payload for debugging

      // Extract and scroll headlines
      String headlines = extractHeadlines(jsonResponse);
      if (headlines.length() > 0) {
        Serial.println("Headlines extracted:");
        Serial.println(headlines);
        scroller.scrollText(headlines.c_str(), 50, 100);  // Scroll the extracted headlines
      } else {
        Serial.println("No headlines found in the response");
      }
      
      client.stop();  // Disconnect
    } else {
      Serial.println("Failed to connect to News API server");
    }
  } else {
    Serial.println("WiFi is not connected");
  }

  delay(60000);  // Fetch news every 60 seconds
}

// Function to extract headlines from the JSON payload
String extractHeadlines(String json) {
  String headlines = "";
  int startIdx = 0;

  // Parse the JSON to extract each headline
  while ((startIdx = json.indexOf("\"title\":\"", startIdx)) >= 0) {
    startIdx += 9;  // Move past the "title":" part
    int endIdx = json.indexOf("\"", startIdx);
    if (endIdx >= 0) {
      String headline = json.substring(startIdx, endIdx);
      headlines += headline + "  ***  ";  // Add separator between headlines
      startIdx = endIdx + 1;
    } else {
      break;
    }
  }

  return headlines;
}
