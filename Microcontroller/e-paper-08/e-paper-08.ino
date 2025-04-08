#include <GxEPD2_BW.h>
#include <Adafruit_GFX.h>
#include <SPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"
#include "esp32-hal-ledc.h"

#include <Fonts/FreeMonoBoldOblique9pt7b.h>
#include <Fonts/FreeMonoBoldOblique12pt7b.h>
#include <Fonts/FreeMonoBoldOblique18pt7b.h>

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


#define LED_G 12  // Green 
#define LED_B 13  // Blue 
#define LED_R 14  // Red 

bool isDrawing = false;  

int consecutiveDetectionCount = 0;
bool lastDetectedState = true;

#define CMD_TEST_TEXT "#TEXT:"

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
    if (currentTime - lastConnectionCheck >= 250) {
      lastConnectionCheck = currentTime;

      bool currentDetectedState = checkDisplayConnection();

      if (currentDetectedState != lastDetectedState) {
        consecutiveDetectionCount = 0;
      } else {
        consecutiveDetectionCount++;
        
        if (consecutiveDetectionCount >= 1) {
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

  // Handle serial commands
  if (Serial.available() > 0) {
    int bytesRead = Serial.readBytesUntil('\n', textBuffer, MAX_TEXT_LENGTH - 1);
    textBuffer[bytesRead] = '\0';

    if (bytesRead > 0) {
      // Check if it's the test command
      if (strncmp(textBuffer, CMD_TEST_TEXT, strlen(CMD_TEST_TEXT)) == 0) {
        // Extract the text part after the command
        char* textToDisplay = textBuffer + strlen(CMD_TEST_TEXT);
        
        // Display the text
        Serial.print("Testing text: ");
        Serial.println(textToDisplay);
        
        // Display text and report which font was used
        displayTextWithFontInfo(textToDisplay);
      }
      else if (strcmp(textBuffer, "#QUOTE") == 0) {
        // Existing quote command
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
  isDrawing = true;
  setLedDrawing();
  
  // Calculate optimal font
  const GFXfont* font = calculateOptimalFont(text);
  
  display.setFullWindow();
  display.firstPage();
  
  do {
    display.fillScreen(GxEPD_WHITE);
    displayWrappedText(text, font);
  } while (display.nextPage());
  
  isDrawing = false;
  setLedConnected();
}

const GFXfont* calculateOptimalFont(const char* text) {
  // Try largest font first, then fall back to smaller ones if needed
  display.setFont(&FreeMonoBoldOblique18pt7b);
  if (allWordsWillFit(text)) {
    return &FreeMonoBoldOblique18pt7b;
  }
  
  display.setFont(&FreeMonoBoldOblique12pt7b);
  if (allWordsWillFit(text)) {
    return &FreeMonoBoldOblique12pt7b;
  }
  
  // Default to smallest font if others don't fit
  return &FreeMonoBoldOblique9pt7b;
}

// Check if all words will fit with the currently set font
bool allWordsWillFit(const char* text) {
  int16_t x1, y1;
  uint16_t w, h;
  
  int screenWidth = display.width() - 20;  // 10px margin on each side
  
  // Extract and measure each word
  char word[51];  // Buffer for current word
  int wordLen = 0;
  
  for (int i = 0; i <= strlen(text); i++) {
    if (text[i] == ' ' || text[i] == '\0' || text[i] == '\n') {
      if (wordLen > 0) {
        // Null-terminate the word
        word[wordLen] = '\0';
        
        // Get word width with current font
        display.getTextBounds(word, 0, 0, &x1, &y1, &w, &h);
        
        // If any word is too wide for the screen, this font won't work
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
  display.getTextBounds("Ay", 0, 0, &x1, &y1, &w, &h);
  int lineHeight = h * 1.3;  // 30% extra space between lines
  
  // Estimate number of lines needed
  int totalChars = strlen(text);
  int charsPerLine = screenWidth / (w / 2);  // Rough estimate
  int estimatedLines = (totalChars / charsPerLine) + 1;
  
  // Check if estimated lines will fit vertically
  return (estimatedLines * lineHeight) < display.height();
}

// Display wrapped text with custom font - Fixed version
void displayWrappedText(const char* text, const GFXfont* font) {
  display.setFont(font);
  display.setTextColor(GxEPD_BLACK);
  
  int16_t x1, y1;
  uint16_t w, h;
  
  // Get character height for line spacing
  display.getTextBounds("Ay", 0, 0, &x1, &y1, &w, &h);
  int lineHeight = h * 1.3;  // 30% extra space between lines
  
  // Determine a proper space width for this font
  int spaceWidth;
  if (font == &FreeMonoBoldOblique18pt7b) {
    spaceWidth = 18;  // Explicit space width for 18pt
  } else if (font == &FreeMonoBoldOblique12pt7b) {
    spaceWidth = 13;  // Explicit space width for 12pt
  } else {
    spaceWidth = 10;   // Explicit space width for 9pt
  }
  
  int screenWidth = display.width() - 20;  // 10px margin on each side
  int cursorX = 10;
  int cursorY = h + 5;  // Start position - add ascender height
  
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
          cursorX = 10;
          cursorY += lineHeight;
        }
        
        // Draw the word
        display.setCursor(cursorX, cursorY);
        display.print(word);
        
        // Advance cursor by the actual word width
        cursorX += w;
        
        // Add space after word (except at end of text)
        if (text[i] == ' ') {
          // Add explicit space width rather than printing a space
          cursorX += spaceWidth;
        }
        
        // Reset word buffer
        wordLen = 0;
      } else if (text[i] == ' ') {
        // Handle consecutive spaces
        cursorX += spaceWidth;
      }
      
      // Handle explicit newline
      if (text[i] == '\n') {
        cursorX = 10;
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
  int busyState = digitalRead(EPD_BUSY);
  
  if (busyState == HIGH) {
    Serial.println("Display DISCONNECTED (BUSY pin high)");
    return false;
  }
  
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
  setLedColor(250, 145, 25);  // light green
}

void setLedDisconnected() {
  setLedColor(255, 80, 20);  // yellow
}

void setLedDrawing() {
  setLedColor(255, 40, 5);  // orange
}

void testFontSizes() {
  // Short quote - should use 18pt
  displayText("Life is short. Make it count.");
  delay(5000);
  
  // Medium quote - should use 12pt
  displayText("The future belongs to those who believe in the beauty of their dreams. -Eleanor Roosevelt");
  delay(5000);
  
  // Longer quote - should use 9pt
  displayText("The only way to do great work is to love what you do. If you haven't found it yet, keep looking. Don't settle. As with all matters of the heart, you'll know when you find it. -Steve Jobs");
}

void displayTextWithFontInfo(const char* text) {
  isDrawing = true;
  setLedDrawing();
  
  // Calculate optimal font and get info about which one was chosen
  const GFXfont* font = calculateOptimalFont(text);
  String fontName;
  
  if (font == &FreeMonoBoldOblique18pt7b) {
    fontName = "18pt Bold Italic";
  } else if (font == &FreeMonoBoldOblique12pt7b) {
    fontName = "12pt Bold Italic";
  } else {
    fontName = "9pt Bold Italic";
  }
  
  Serial.print("Using font: ");
  Serial.println(fontName);
  
  // Display the text using the selected font
  display.setFullWindow();
  display.firstPage();
  
  do {
    display.fillScreen(GxEPD_WHITE);
    displayWrappedText(text, font);
  } while (display.nextPage());
  
  isDrawing = false;
  setLedConnected();
  
  Serial.println("Text displayed successfully");
}