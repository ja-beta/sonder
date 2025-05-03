#include <GxEPD2_BW.h>
#include <Adafruit_GFX.h>
#include <SPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"
#include "esp32-hal-ledc.h"
#include "SpecialChars.h"
#include <NarkissAsaf_Regular9pt7b.h>
#include <NarkissAsaf_Regular12pt7b.h>
#include <NarkissAsaf_Regular20pt7b.h>

const GFXfont* fontSmall = &NarkissAsaf_Regular9pt7b;
const GFXfont* fontMedium = &NarkissAsaf_Regular12pt7b;
const GFXfont* fontLarge = &NarkissAsaf_Regular20pt7b;

#define EPD_CS 5    // Chip Select
#define EPD_DC 17   // Data/Command
#define EPD_RST 16  // Reset
#define EPD_BUSY 4  // Busy

// Display object for Waveshare 2.66-inch B/W (296x152)
GxEPD2_BW<GxEPD2_266_BN, GxEPD2_266_BN::HEIGHT> display(GxEPD2_266_BN(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

SpecialChars specialChars(&display);

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
        
        Serial.print("Testing text: ");
        Serial.println(textToDisplay);
        
        // Process it the same way as quotes
        String testText = String(textToDisplay);
        String quotedText = addCurlyQuotes(testText);
        String processedText = preprocessText(quotedText);
        
        // Display the processed text
        displayText(processedText.c_str());
      }
      else if (strcmp(textBuffer, "#QUOTE") == 0) {
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
  Serial.println("Attempting to connect to WiFi...");
  
  const char* networks[][2] = {
    {SECRET_SSID1, SECRET_PASS1},  // ITP network
    {SECRET_SSID2, SECRET_PASS2}   // Resistor network
  };
  const int numNetworks = 2;
  
  WiFi.mode(WIFI_STA);
  delay(100);
  
  for (int network = 0; network < numNetworks; network++) {
    const char* currentSsid = networks[network][0];
    const char* currentPass = networks[network][1];
    
    Serial.print("Trying to connect to: ");
    Serial.println(currentSsid);
    
    // Disconnect if already connected
    WiFi.disconnect();
    delay(200);
    
    // Start connection attempt
    WiFi.begin(currentSsid, currentPass);
    
    // Wait for connection with feedback
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 15) {  // Reduced timeout for faster fallback
      delay(500);
      Serial.print(".");
      attempts++;
    }
    
    // If connected, exit the loop
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi connected successfully!");
      Serial.print("Connected to: ");
      Serial.println(currentSsid);
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      Serial.print("Signal strength (RSSI): ");
      Serial.print(WiFi.RSSI());
      Serial.println(" dBm");
      break;  // Exit the loop if connected
    } else {
      Serial.println("\nFailed to connect to this network, trying next...");
    }
  }
  
  // Final connection status
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nFailed to connect to any WiFi network");
    Serial.print("Final status code: ");
    Serial.println(WiFi.status());
  }
  Serial.println("---------------------------------------");
}

String addCurlyQuotes(const String& text) {
  String quotedText;
  
  // Left double quote (UTF-8 bytes for ")
  quotedText += (char)0xE2;
  quotedText += (char)0x80;
  quotedText += (char)0x9C;
  
  // Add the actual quote/text
  quotedText += text;
  
  // Right double quote (UTF-8 bytes for ")
  quotedText += (char)0xE2;
  quotedText += (char)0x80;
  quotedText += (char)0x9D;
  
  return quotedText;
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
        
        // Add curly quotes and process
        String quotedText = addCurlyQuotes(quoteText);
        String processedQuote = preprocessText(quotedText);
        
        displayText(processedQuote.c_str());
        
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
  
  // Calculate the optimal font size
  const GFXfont* font = calculateOptimalFont(text);
  
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setFont(font);
    display.setTextColor(GxEPD_BLACK);
    
    // Draw the text with special character handling
    drawCenteredWrappedText(text, display.width(), display.height());
    
  } while (display.nextPage());
  
  isDrawing = false;
  setLedConnected();
}

const GFXfont* calculateOptimalFont(const char* text) {
  display.setFont(fontLarge);
  if (allWordsWillFit(text)) {
    return fontLarge;
  }
  
  display.setFont(fontMedium);
  if (allWordsWillFit(text)) {
    return fontMedium;
  }
  
  return fontSmall;
}

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

// Display wrapped text with vertical centering
void displayWrappedText(const char* text, const GFXfont* font) {
  display.setFont(font);
  display.setTextColor(GxEPD_BLACK);
  
  int16_t x1, y1;
  uint16_t w, h;
  
  // Get character height for line spacing
  display.getTextBounds("Ay", 0, 0, &x1, &y1, &w, &h);
  int lineHeight = h * 1.2;  // 30% extra space between lines
  
  // Determine a proper space width for this font
  int spaceWidth;
  if (font == fontLarge) {
    spaceWidth = 18;  // Explicit space width for large font
  } else if (font == fontMedium) {
    spaceWidth = 13;  // Explicit space width for medium font
  } else {
    spaceWidth = 10;  // Explicit space width for small font
  }
  
  int screenWidth = display.width() - 20;  // 10px margin on each side
  int screenHeight = display.height() - 20;
  
  // FIRST PASS: Calculate how many lines the text will occupy
  int cursorX = 10;
  int numLines = 1;  // Start with at least one line
  
  // Variables for word processing
  char word[51];  // Buffer for current word
  int wordLen = 0;
  
  // Process the input text to count lines
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
          numLines++;
        }
        
        // Advance cursor by the word width
        cursorX += w;
        
        // Add space after word
        if (text[i] == ' ') {
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
        numLines++;
      }
    } else {
      // Add character to current word
      if (wordLen < sizeof(word) - 2) {
        word[wordLen++] = text[i];
      }
    }
  }
  
  // Calculate total text height
  int totalTextHeight = numLines * lineHeight;
  
  // Calculate starting Y position for proper vertical centering
  int startY = (screenHeight - totalTextHeight) / 2 + 20;  // Increased positive offset to move text down
  
  // SECOND PASS: Actually draw the text
  cursorX = 10;
  int cursorY = startY;
  wordLen = 0;
  
  // Process the input text again to draw it
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
        
        // Advance cursor by the word width
        cursorX += w;
        
        // Add space after word
        if (text[i] == ' ') {
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

String preprocessText(const String& text) {
  return specialChars.replaceWithPlaceholders(text);
}




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
    // Serial.println("Display DISCONNECTED (BUSY pin high)");
    return false;
  }
  
  return true;
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

void drawCenteredWrappedText(const char* text, int screenWidth, int screenHeight) {
    display.setTextWrap(false);

    // Get font metrics for current font
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds("Aj", 0, 0, &x1, &y1, &w, &h);
    int lineHeight = h + 6; // Add some extra spacing between lines

    // Set space width and letter spacing based on font size
    int spaceWidth;
    int letterSpacing;

    if (h < 12) {
        // Small font
        spaceWidth = 6;
        letterSpacing = 2; // Increased spacing
    } else if (h > 19) {
        // Large font
        spaceWidth = 10;
        letterSpacing = 2; // Increased spacing
    } else {
        // Medium font
        spaceWidth = 8;
        letterSpacing = 2; // Increased spacing
    }

    // FIRST PASS: Calculate layout
    int cursorX = 10;
    int numLines = 1;
    int wordLen = 0;
    char word[100];

    for (int i = 0; i <= strlen(text); i++) {
        if (text[i] == ' ' || text[i] == '\0' || text[i] == '\n') {
            if (wordLen > 0) {
                word[wordLen] = '\0';

                int wordWidth = 0;
                for (int j = 0; j < wordLen; j++) {
                    if (specialChars.isSpecialChar(word[j])) {
                        wordWidth += specialChars.getCharWidth(word[j]);
                    } else {
                        char temp[2] = {word[j], '\0'};
                        display.getTextBounds(temp, 0, 0, &x1, &y1, &w, &h);
                        wordWidth += w + letterSpacing;
                    }
                }

                if (cursorX + wordWidth > screenWidth - 10) {
                    cursorX = 10;
                    numLines++;
                }

                cursorX += wordWidth;

                if (text[i] == ' ') {
                    cursorX += spaceWidth;
                }

                wordLen = 0;
            } else if (text[i] == ' ') {
                cursorX += spaceWidth;
            }

            if (text[i] == '\n') {
                cursorX = 10;
                numLines++;
            }
        } else {
            if (wordLen < sizeof(word) - 2) {
                word[wordLen++] = text[i];
            }
        }
    }

    int totalTextHeight = numLines * lineHeight;
    int startY = (screenHeight - totalTextHeight) / 2;
    if (h > 19) {
        startY += 32; 
    } else if (h < 12){
        startY += 13;
    } else {
        startY += 26;  
    }

    // Check if the text is too long
    if (totalTextHeight > screenHeight) {
        Serial.println("Text too long, fetching a new quote...");
        fetchAndDisplayQuote();
        return;
    }

    // SECOND PASS: Actually draw the text
    cursorX = 10;
    int cursorY = startY;
    wordLen = 0;
    bool isFirstWord = true; // Add this flag to track first word

    for (int i = 0; i <= strlen(text); i++) {
        if (text[i] == ' ' || text[i] == '\0' || text[i] == '\n') {
            if (wordLen > 0) {
                word[wordLen] = '\0';

                int wordWidth = 0;
                
                // Calculate word width
                for (int j = 0; j < wordLen; j++) {
                    if (specialChars.isSpecialChar(word[j])) {
                        wordWidth += specialChars.getCharWidth(word[j]);
                    } else {
                        char temp[2] = {word[j], '\0'};
                        display.getTextBounds(temp, 0, 0, &x1, &y1, &w, &h);
                        wordWidth += w + letterSpacing;
                    }
                }

                // Add extra spacing after first word for large font
                if (isFirstWord && h > 19) {
                    wordWidth += 12;  // Increased from 8 to 12 for more space between first and second word
                }

                if (cursorX + wordWidth > screenWidth - 10) {
                    cursorX = 10;
                    cursorY += lineHeight;
                }

                int wordX = cursorX;
                for (int j = 0; j < wordLen; j++) {
                    if (specialChars.isSpecialChar(word[j])) {
                        int charWidth = specialChars.drawChar(wordX, cursorY, word[j]);
                        wordX += charWidth;
                    } else {
                        char temp[2] = {word[j], '\0'};
                        display.setCursor(wordX, cursorY);
                        display.print(temp);

                        display.getTextBounds(temp, 0, 0, &x1, &y1, &w, &h);
                        wordX += w + letterSpacing;
                    }
                }

                cursorX += wordWidth;
                isFirstWord = false; // After drawing first word, set flag to false

                if (text[i] == ' ') {
                    cursorX += spaceWidth;
                }

                wordLen = 0;
            } else if (text[i] == ' ') {
                cursorX += spaceWidth;
            }

            if (text[i] == '\n') {
                cursorX = 10;
                cursorY += lineHeight;
                isFirstWord = true; // Reset first word flag for new line
            }
        } else {
            if (wordLen < sizeof(word) - 2) {
                word[wordLen++] = text[i];
            }
        }
    }
}
