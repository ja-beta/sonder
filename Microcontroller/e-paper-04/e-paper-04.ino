#include <GxEPD.h>
#include <GxGDEH0213B73/GxGDEH0213B73.h>  // 2.13" b/w for TTGO T5 V2.0
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>
#include <Adafruit_GFX.h>
#include <SPI.h>

// Pin definitions SPECIFICALLY for TTGO T5 V2.0
#define EPD_BUSY 4  // BUSY
#define EPD_CS   5  // CS
#define EPD_RST  16 // RST
#define EPD_DC   17 // DC
#define EPD_SCK  18 // SCK
#define EPD_MISO -1 // Not used
#define EPD_MOSI 23 // MOSI
#define EPD_EN   14 // POWER ENABLE PIN - CRITICAL FOR TTGO T5 V2.0

// Initialize with explicit SPI definition
SPIClass hspi(HSPI);
GxIO_Class io(hspi, EPD_CS, EPD_DC, EPD_RST);
GxEPD_Class display(io, EPD_RST, EPD_BUSY);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("TTGO T5 V2.0 E-Paper Display Final Fix Attempt");
  
  // POWER ENABLE - CRITICAL STEP FOR TTGO T5
  pinMode(EPD_EN, OUTPUT);
  digitalWrite(EPD_EN, HIGH);  // Turn ON the display power
  Serial.println("Display power enabled");
  delay(200);  // Give time for power to stabilize
  
  // Initialize custom SPI for TTGO T5 V2.0
  hspi.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
  
  // Hard reset sequence
  pinMode(EPD_RST, OUTPUT);
  digitalWrite(EPD_RST, LOW);
  delay(100);
  digitalWrite(EPD_RST, HIGH);
  delay(100);
  
  // Initialize the display
  Serial.println("Initializing display...");
  display.init(115200); // With baud rate for TTGO boards
  
  Serial.println("Display initialized!");
  
  // Clear the display
  Serial.println("Clearing display...");
  display.fillScreen(GxEPD_WHITE);
  display.update();
  delay(1000);
  
  // Draw a simple high-contrast test pattern
  Serial.println("Drawing test pattern...");
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display.setRotation(1);
  
  // Make the pattern very high contrast
  display.fillRect(0, 0, display.width(), display.height()/2, GxEPD_BLACK);
  display.setTextSize(2);
  display.setTextColor(GxEPD_WHITE);
  display.setCursor(10, 40);
  display.print("TTGO TEST");
  
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(10, display.height()/2 + 30);
  display.print("DISPLAY");
  
  // Update the display
  display.update();
  
  Serial.println("Setup complete");
}

void loop() {
  if (Serial.available() > 0) {
    String message = Serial.readStringUntil('\n');
    
    if (message == "#CLEAR") {
      display.fillScreen(GxEPD_WHITE);
      display.update();
      Serial.println("Display cleared");
    } 
    else {
      display.fillScreen(GxEPD_WHITE);
      display.setCursor(10, 30);
      display.setTextSize(2);  // Larger text
      display.print(message);
      display.update();
      Serial.println("Message displayed: " + message);
    }
  }
  
  delay(100);
} 