#include <GxEPD2_3C.h>
#include <Adafruit_GFX.h>
#include <SPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

#define EPD_CS   5   // Chip Select
#define EPD_DC   17  // Data/Command
#define EPD_RST  16  // Reset
#define EPD_BUSY 4   // Busy

// Display object for Waveshare 1.54-inch B/W/Red Rev 2.1
GxEPD2_3C<GxEPD2_154_Z90c, GxEPD2_154_Z90c::HEIGHT> display(
    GxEPD2_154_Z90c(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

const char* ssid = SECRET_SSID;
const char* password = SECRET_PASS;

const char* quoteEndpoint = "https://us-central1-sonder-2813.cloudfunctions.net/serve_quote";

String deviceId = "eink-display-001"; 

#define MAX_TEXT_LENGTH 256
char textBuffer[MAX_TEXT_LENGTH];

bool displayConnected = false;

bool displayWasConnected = false;
unsigned long lastConnectionCheck = 0;
const unsigned long connectionCheckInterval = 2000; 

bool initialStartupComplete = false;
unsigned long startupTime = 0;

uint32_t lastDrawnHash = 0;

bool displayDisconnected = false;

// Add a physical button for manual refreshing
#define REFRESH_BUTTON 21  // Use an available GPIO pin
unsigned long lastRefreshTime = 0;
const unsigned long refreshDebounce = 500;

void setup() {
  Serial.begin(115200);
  delay(500);
  
  SPI.begin();
  
  pinMode(EPD_BUSY, INPUT_PULLUP);
  pinMode(EPD_CS, OUTPUT);
  pinMode(EPD_DC, OUTPUT);
  pinMode(EPD_RST, OUTPUT);
  
  digitalWrite(EPD_RST, HIGH);
  delay(20);
  digitalWrite(EPD_RST, LOW);
  delay(20);
  digitalWrite(EPD_RST, HIGH);
  delay(200);
  
  // Add the refresh button
  pinMode(REFRESH_BUTTON, INPUT_PULLUP);
  
  display.init(false);
  displayConnected = true;
  displayWasConnected = true; 
  display.setRotation(1); // Landscape mode
  
  connectToWiFi();
  
  fetchAndDisplayQuote();
  
  // Record startup time
  startupTime = millis();
}

void loop() {
  // Wait for 10 seconds after startup before checking connection changes
  if (!initialStartupComplete && (millis() - startupTime > 10000)) {
    initialStartupComplete = true;
    Serial.println("Connection detection now active");
    displayWasConnected = true;  // Assume connected initially
    displayDisconnected = false; // Not disconnected yet
  }

  // Only do connection detection after initial grace period
  if (initialStartupComplete) {
    // Check for display connection changes less frequently (5 seconds)
    unsigned long currentTime = millis();
    if (currentTime - lastConnectionCheck >= 5000) {
      lastConnectionCheck = currentTime;
      
      // Check if display is connected
      bool currentDisplayState = checkDisplayConnection();
      
      // Handle disconnection detection 
      if (displayWasConnected && !currentDisplayState) {
        Serial.println("DISPLAY DISCONNECTED!");
        displayDisconnected = true; // Mark as disconnected
      }
      
      // If previously disconnected and now reconnected, fetch a new quote
      if (displayDisconnected && currentDisplayState) {
        Serial.println("DISPLAY RECONNECTED - fetching new quote");
        displayDisconnected = false; // Reset disconnect flag
        
        // Reset the display on reconnection
        digitalWrite(EPD_RST, LOW);
        delay(20);
        digitalWrite(EPD_RST, HIGH);
        delay(200);
        display.init(false);
        
        // Fetch new quote
        fetchAndDisplayQuote();
      }
      
      displayWasConnected = currentDisplayState;
    }
  }

  // Check for manual refresh button
  if (digitalRead(REFRESH_BUTTON) == LOW) {
    if (millis() - lastRefreshTime > refreshDebounce) {
      lastRefreshTime = millis();
      Serial.println("Manual refresh requested");
      fetchAndDisplayQuote();
    }
  }

  // Check if data is available
  if (Serial.available() > 0) {
    // Read input with timeout
    int bytesRead = Serial.readBytesUntil('\n', textBuffer, MAX_TEXT_LENGTH - 1);
    
    // Null terminate the string
    textBuffer[bytesRead] = '\0';
    
    if (bytesRead > 0) {
      // Special command for refreshing quote
      if (strcmp(textBuffer, "#QUOTE") == 0) {
        fetchAndDisplayQuote();
      } 
      // Special command for partial update
      else if (strcmp(textBuffer, "#PARTIAL") == 0) {
        updatePartialArea(50, 50, 100, 30, "Partial Update!");
        Serial.println("Partial update performed");
      } else {
        // Display the received text
        displayText(textBuffer);
        Serial.println("Text updated on display");
      }
    }
  }
  
  // Short delay
  delay(100);
}

void connectToWiFi() {
  Serial.println("\n---------------------------------------");
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
  // Set WiFi mode explicitly
  WiFi.mode(WIFI_STA);
  delay(100);
  
  // Disconnect if already connected
  WiFi.disconnect();
  delay(200);
  
  // Start connection attempt
  WiFi.begin(ssid, password);
  
  // Wait for connection with more detailed feedback
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
    
    // Every 10 attempts, print status code
    if (attempts % 10 == 0) {
      Serial.println();
      Serial.print("Status code: ");
      Serial.println(WiFi.status());
      /*
       * WiFi Status Codes:
       * 0 : WL_IDLE_STATUS - WiFi is changing state
       * 1 : WL_NO_SSID_AVAIL - SSID not available
       * 2 : WL_SCAN_COMPLETED - Scan completed
       * 3 : WL_CONNECTED - Connection successful
       * 4 : WL_CONNECT_FAILED - Connection failed
       * 5 : WL_CONNECTION_LOST - Connection lost
       * 6 : WL_DISCONNECTED - Disconnected
       */
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected successfully!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal strength (RSSI): ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("\nWiFi connection failed");
    Serial.print("Final status code: ");
    Serial.println(WiFi.status());
  }
  Serial.println("---------------------------------------");
}

void fetchAndDisplayQuote() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
    if (WiFi.status() != WL_CONNECTED) {
      displayText("WiFi not connected");
      return;
    }
  }
  
  HTTPClient http;
  String url = String(quoteEndpoint) + "?device_id=" + deviceId;
  
  http.begin(url);
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    
    // Parse JSON response
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      bool success = doc["success"];
      
      if (success) {
        String quoteText = doc["quote"];
        
        String displayString = "\"" + quoteText + "\"";
        displayText(displayString.c_str());
        
        Serial.println("New quote displayed");
      } else {
        // No quotes available
        displayText("No new quotes available");
      }
    } else {
      displayText("Error parsing response");
    }
  } else {
    String message = "HTTP error: " + String(httpCode);
    displayText(message.c_str());
  }
  
  http.end();
}

void displayText(const char* text) {
  // Calculate optimal text size that fits all words
  int textSize = calculateOptimalTextSize(text);
  Serial.print("Using text size: ");
  Serial.println(textSize);
  
  display.setFullWindow();
  display.firstPage();
  
  do {
    // Clear screen
    display.fillScreen(GxEPD_WHITE);
    
    // Display text with word wrapping
    displayWrappedText(text, textSize);
    
  } while (display.nextPage());
}

int calculateOptimalTextSize(const char* text) {
  // Start with largest size and work downward
  for (int size = 3; size >= 1; size--) {
    if (allWordsWillFit(text, size)) {
      return size;
    }
  }
  
  // Fallback to smallest size
  return 1;
}

// Check if all words will fit at the given text size
bool allWordsWillFit(const char* text, int textSize) {
  int16_t x1, y1;
  uint16_t w, h;
  
  display.setTextSize(textSize);
  int screenWidth = display.width() - 10;  // 5px margin on each side
  
  // Extract and measure each word
  char word[51];  // Buffer for current word
  int wordLen = 0;
  
  for (int i = 0; i <= strlen(text); i++) {
    if (text[i] == ' ' || text[i] == '\0' || text[i] == '\n') {
      if (wordLen > 0) {
        // Null-terminate the word
        word[wordLen] = '\0';
        
        // Get word width
        display.getTextBounds(word, 0, 0, &x1, &y1, &w, &h);
        
        // If any word is too wide for the screen, this size won't work
        if (w > screenWidth) {
          return false;
        }
        
        // Reset word buffer
        wordLen = 0;
      }
    } else {
      // Add character to current word
      if (wordLen < sizeof(word) - 2) {
        word[wordLen++] = text[i];
      }
    }
  }
  
  // Also check if total text will fit vertically
  // (rough estimate based on character height and line count)
  display.getTextBounds("Ay", 0, 0, &x1, &y1, &w, &h);
  int lineHeight = h + 2;
  
  // Estimate number of lines needed (very rough approximation)
  int totalChars = strlen(text);
  int charsPerLine = screenWidth / (textSize * 6); // Approximate average char width
  int estimatedLines = (totalChars / charsPerLine) + 1;
  
  // Check if estimated lines will fit vertically
  return (estimatedLines * lineHeight) < display.height();
}

// Display text with proper word wrapping
void displayWrappedText(const char* text, int textSize) {
  display.setTextColor(GxEPD_BLACK);
  display.setTextSize(textSize);
  
  int16_t x1, y1;
  uint16_t w, h;
  
  // Get character height for line spacing
  display.getTextBounds("Ay", 0, 0, &x1, &y1, &w, &h);
  int lineHeight = h + 2;
  
  int screenWidth = display.width() - 10;  // Leaving 5px margin on each side
  int cursorX = 5;
  int cursorY = 5 + lineHeight;  // Start position
  
  // Variables for word processing
  char word[51];  // Buffer for current word
  int wordLen = 0;
  
  // Process the input text character by character
  for (int i = 0; i <= strlen(text); i++) {
    // End of word or end of text
    if (text[i] == ' ' || text[i] == '\0' || text[i] == '\n') {
      if (wordLen > 0) {
        // Null-terminate the word
        word[wordLen] = '\0';
        
        // Get word width
        display.getTextBounds(word, 0, 0, &x1, &y1, &w, &h);
        
        // Check if word fits on current line
        if (cursorX + w > screenWidth) {
          // Move to next line
          cursorX = 5;
          cursorY += lineHeight;
        }
        
        // Draw the word
        display.setCursor(cursorX, cursorY);
        display.print(word);
        
        // Advance cursor
        cursorX += w;
        
        // Add space after word (except at end of text)
        if (text[i] == ' ') {
          display.print(" ");
          // Approximate space width
          cursorX += textSize * 6;
        }
        
        // Reset word buffer
        wordLen = 0;
      }
      
      // Handle explicit newline
      if (text[i] == '\n') {
        cursorX = 5;
        cursorY += lineHeight;
      }
    } else {
      // Add character to current word
      if (wordLen < sizeof(word) - 2) {
        word[wordLen++] = text[i];
      }
    }
  }
}

// Function to update a small portion of the screen (partial refresh)
void updatePartialArea(int x, int y, int w, int h, const char* message) {
  display.setPartialWindow(x, y, w, h);
  display.firstPage();
  do {
    display.fillRect(x, y, w, h, GxEPD_WHITE);
    display.setCursor(x, y + 15);
    display.setTextColor(GxEPD_BLACK);
    display.print(message);
  } while (display.nextPage());
}

bool checkDisplayConnection() {
  // Simple BUSY pin check - if disconnected, should read HIGH due to pull-up
  // When connected, might be LOW during operations or HIGH when idle
  
  // First check - read the pin
  int busyState = digitalRead(EPD_BUSY);
  
  // Force a brief CS operation which should trigger activity on BUSY pin
  digitalWrite(EPD_CS, LOW);
  delayMicroseconds(100);  // Very brief delay
  digitalWrite(EPD_CS, HIGH);
  
  // Second check - read again
  int busyState2 = digitalRead(EPD_BUSY);
  
  // Check if pin is in a floating state (disconnected)
  bool connected = !(busyState == HIGH && busyState2 == HIGH);
  
  // Debug output
  Serial.print("BUSY pin states: ");
  Serial.print(busyState);
  Serial.print(", ");
  Serial.print(busyState2);
  Serial.print(" - Display ");
  Serial.println(connected ? "CONNECTED" : "DISCONNECTED");
  
  return connected;
} 