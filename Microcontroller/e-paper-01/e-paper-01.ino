#include <GxEPD2_3C.h>
#include <Adafruit_GFX.h>
#include <Fonts/FreeSans9pt7b.h>
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
  // Initialize serial at 115200 baud
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n---------------------------------");
  Serial.println("E-Paper Display Serial Test v1.1");
  Serial.println("---------------------------------");
  
  // Initialize SPI with debugging
  Serial.println("Initializing SPI...");
  SPI.begin();
  Serial.println("SPI initialized");
  
  // Initialize the display with debugging
  Serial.println("Initializing display...");
  pinMode(EPD_BUSY, INPUT_PULLUP);
  pinMode(EPD_CS, OUTPUT);
  pinMode(EPD_DC, OUTPUT);
  pinMode(EPD_RST, OUTPUT);
  
  // Full reset sequence
  Serial.println("Performing reset sequence...");
  digitalWrite(EPD_RST, HIGH);
  delay(20);
  digitalWrite(EPD_RST, LOW);
  delay(20);
  digitalWrite(EPD_RST, HIGH);
  delay(200);
  
  // Initialize with debugging messages
  Serial.println("Starting display init()...");
  display.init(false);
  Serial.println("Display initialized successfully!");
  
  display.setRotation(1); // Landscape mode
  Serial.println("Display rotation set to landscape");
  
  // Show startup message
  Serial.println("Displaying welcome message...");
  displayText("Serial Test v1.1");
  Serial.println("Welcome message displayed");
  
  // Ready for input
  Serial.println("\n*** Ready for text input ***");
  Serial.println("Type something and press Enter");
}

void loop() {
  // Show blinking indicator in serial to confirm code is running
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > 5000) {
    Serial.println("Waiting for input...");
    lastBlink = millis();
  }
  
  // Check if data is available
  if (Serial.available() > 0) {
    Serial.println("Serial data detected!");
    
    // Clear any old data
    memset(textBuffer, 0, MAX_TEXT_LENGTH);
    
    // Read with timeout
    Serial.println("Reading input...");
    int bytesRead = Serial.readBytesUntil('\n', textBuffer, MAX_TEXT_LENGTH - 1);
    
    // Null terminate
    textBuffer[bytesRead] = '\0';
    
    // Echo what was received
    Serial.print("Received [");
    Serial.print(bytesRead);
    Serial.print(" bytes]: ");
    Serial.println(textBuffer);
    
    if (bytesRead > 0) {
      // Show on display
      Serial.println("Updating display with new text...");
      displayText(textBuffer);
      Serial.println("Display update complete!");
    } else {
      Serial.println("Empty input, nothing to display");
    }
    
    // Ready for more
    Serial.println("\n*** Ready for more text ***");
  }
  
  // Short delay
  delay(100);
}

void displayText(const char* text) {
  Serial.println("Starting display update sequence...");
  
  display.setFullWindow();
  Serial.println("Full window set");
  
  Serial.println("Beginning page rendering...");
  display.firstPage();
  int pageCount = 0;
  
  do {
    pageCount++;
    Serial.print("Rendering page ");
    Serial.println(pageCount);
    
    display.fillScreen(GxEPD_WHITE);
    
    // Add a header to confirm display is updating
    display.setTextColor(GxEPD_RED);
    display.setTextSize(1);
    display.setCursor(5, 10);
    display.print("Updated: ");
    
    // Add timestamp
    long seconds = millis() / 1000;
    display.print(seconds);
    display.print("s");
    
    // Main text content in black
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(5, 30);
    display.print(text);
    
  } while (display.nextPage());
  
  Serial.print("Display updated with ");
  Serial.print(pageCount);
  Serial.println(" pages rendered");
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
  
  Serial.println("Partial area updated");
} 