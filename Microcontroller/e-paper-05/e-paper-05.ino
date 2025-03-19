#include <GxEPD2_BW.h>
#include <Adafruit_GFX.h>
#include <SPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"
#include "esp32-hal-ledc.h"

#define EPD_CS 5    // Chip Select
#define EPD_DC 17   // Data/Command
#define EPD_RST 16  // Reset
#define EPD_BUSY 4  // Busy

// Display object for Waveshare 2.66-inch B/W (296x152)
GxEPD2_BW<GxEPD2_266_BN, GxEPD2_266_BN::HEIGHT> display(GxEPD2_266_BN(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

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


#define LED_G 12  // Green pin
#define LED_B 13  // Blue pin
#define LED_R 12  // Red pin

bool isDrawing = false;  

int consecutiveDetectionCount = 0;
bool lastDetectedState = true;

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

  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
    
  Serial.println("Initializing 2.66\" Waveshare e-Paper display...");
  display.init(115200);  
  displayConnected = true;
  displayWasConnected = true;
  display.setRotation(3);  // Landscape mode
  setLedConnected();

  clearDisplay();

  connectToWiFi();
  fetchAndDisplayQuote();

  startupTime = millis();
}

void loop() {
  if (!initialStartupComplete && (millis() - startupTime > 5000)) {
    initialStartupComplete = true;
    Serial.println("Connection detection now active");
    displayWasConnected = true;   
    displayDisconnected = false; 
  }

  if (initialStartupComplete) {
    unsigned long currentTime = millis();
    if (currentTime - lastConnectionCheck >= 1000) {
      lastConnectionCheck = currentTime;

      bool currentDetectedState = checkDisplayConnection();

      if (currentDetectedState != lastDetectedState) {
        consecutiveDetectionCount = 0;
      } else {
        consecutiveDetectionCount++;
        
        if (consecutiveDetectionCount >= 2) {
          bool currentDisplayState = currentDetectedState;
          
          if (displayWasConnected && !currentDisplayState) {
            Serial.println("DISPLAY DISCONNECTED!");
            displayDisconnected = true;
            setLedDisconnected();
          }
          
          if (displayDisconnected && currentDisplayState) {
            Serial.println("DISPLAY RECONNECTED - fetching new quote");
            displayDisconnected = false;
            setLedConnected(); 
            
            digitalWrite(EPD_RST, LOW);
            delay(20);
            digitalWrite(EPD_RST, HIGH);
            delay(200);
            display.init(false);
            
            fetchAndDisplayQuote();
          }
          
          displayWasConnected = currentDisplayState;
        }
      }

      lastDetectedState = currentDetectedState;
    }
  }

  if (Serial.available() > 0) {
    int bytesRead = Serial.readBytesUntil('\n', textBuffer, MAX_TEXT_LENGTH - 1);

    textBuffer[bytesRead] = '\0';

    if (bytesRead > 0) {
      if (strcmp(textBuffer, "#QUOTE") == 0) {
        fetchAndDisplayQuote();
      }
      else if (strcmp(textBuffer, "#PARTIAL") == 0) {
        updatePartialArea(50, 50, 100, 30, "Partial Update!");
        Serial.println("Partial update performed");
      } else {
        displayText(textBuffer);
        Serial.println("Text updated on display");
      }
    }
  }

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
  // Set drawing state
  isDrawing = true;
  setLedDrawing();
  
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
  
  // Reset drawing state
  isDrawing = false;
  setLedConnected();
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
  int charsPerLine = screenWidth / (textSize * 6);  // Approximate average char width
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

  int screenWidth = display.width() - 16;  // Leaving 5px margin on each side
  int cursorX = 8;
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
  isDrawing = true;
  setLedDrawing();
  
  display.setPartialWindow(x, y, w, h);
  display.firstPage();
  do {
    display.fillRect(x, y, w, h, GxEPD_WHITE);
    display.setCursor(x, y + 15);
    display.setTextColor(GxEPD_BLACK);
    display.print(message);
  } while (display.nextPage());
  
  isDrawing = false;
  setLedConnected();
}

bool checkDisplayConnection() {
  // Save current state for diagnostic
  int initialBusyState = digitalRead(EPD_BUSY);
  
  // First, select the display with CS low
  digitalWrite(EPD_CS, LOW);
  delay(1);  // Give it a moment
  
  // Send a harmless command (0x71 = get status)
  digitalWrite(EPD_DC, LOW);  // Command mode
  SPI.transfer(0x71);         // Get status command
  
  delay(5);
  
  int busyStateAfterCommand = digitalRead(EPD_BUSY);
  
  digitalWrite(EPD_CS, HIGH);
  
  delay(10);
  int finalBusyState = digitalRead(EPD_BUSY);
  
  bool connected = (initialBusyState == 0) && 
                  (busyStateAfterCommand == 0) && 
                  (finalBusyState == 0);
  
  Serial.print("Display detection - BUSY states: ");
  Serial.print(initialBusyState);
  Serial.print(" → ");
  Serial.print(busyStateAfterCommand);
  Serial.print(" → ");
  Serial.print(finalBusyState);
  Serial.print(" - Display ");
  Serial.println(connected ? "CONNECTED" : "DISCONNECTED");
  
  return connected;
}

void clearDisplay() {
  Serial.println("Clearing display...");
  isDrawing = true;
  setLedDrawing();
  
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
  } while (display.nextPage());
  
  delay(100);
  isDrawing = false;
  setLedConnected();
  Serial.println("Display cleared");
}

void setLedColor(uint8_t r, uint8_t g, uint8_t b) {
  analogWrite(LED_R, r);
  analogWrite(LED_G, g);
  analogWrite(LED_B, b);
}

void setLedConnected() {
  setLedColor(20, 255, 20);  
}

void setLedDisconnected() {
  setLedColor(20, 20, 255);  
}

void setLedDrawing() {
  setLedColor(255, 5, 5);  
}