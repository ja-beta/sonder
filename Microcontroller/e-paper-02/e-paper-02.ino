#include <GxEPD2_3C.h>
#include <Adafruit_GFX.h>
#include <SPI.h>

// Pin definitions (ESP32-WROOM-32)
#define EPD_CS   5   // Chip Select
#define EPD_DC   17  // Data/Command
#define EPD_RST  16  // Reset
#define EPD_BUSY 4   // Busy

// Display object for Waveshare 1.54-inch B/W/Red Rev 2.1
GxEPD2_3C<GxEPD2_154_Z90c, GxEPD2_154_Z90c::HEIGHT> display(
    GxEPD2_154_Z90c(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

// Buffer for incoming serial data
#define MAX_TEXT_LENGTH 256
char textBuffer[MAX_TEXT_LENGTH];

void setup() {
  // Initialize serial
  Serial.begin(115200);
  delay(500);
  
  // Initialize SPI and display
  SPI.begin();
  
  // Initialize pins
  pinMode(EPD_BUSY, INPUT_PULLUP);
  pinMode(EPD_CS, OUTPUT);
  pinMode(EPD_DC, OUTPUT);
  pinMode(EPD_RST, OUTPUT);
  
  // Reset display
  digitalWrite(EPD_RST, HIGH);
  delay(20);
  digitalWrite(EPD_RST, LOW);
  delay(20);
  digitalWrite(EPD_RST, HIGH);
  delay(200);
  
  // Initialize display
  display.init(false);
  display.setRotation(1); // Landscape mode
  
  // Show default message
  displayText("Send text via Serial Monitor");
  
  // Ready for input
  Serial.println("Ready for text input. Type and press Enter.");
}

void loop() {
  // Check if data is available
  if (Serial.available() > 0) {
    // Read input with timeout
    int bytesRead = Serial.readBytesUntil('\n', textBuffer, MAX_TEXT_LENGTH - 1);
    
    // Null terminate the string
    textBuffer[bytesRead] = '\0';
    
    if (bytesRead > 0) {
      // Special command for partial update
      if (strcmp(textBuffer, "#PARTIAL") == 0) {
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

// Determine optimal text size that ensures no word is cut off
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