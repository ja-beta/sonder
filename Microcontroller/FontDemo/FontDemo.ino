#include <GxEPD2_BW.h>
#include <Adafruit_GFX.h>
#include <SPI.h>
// Bold Italic Mono (called "Bold Oblique" in the font family)
#include <Fonts/FreeMonoBoldOblique9pt7b.h>
#include <Fonts/FreeMonoBoldOblique12pt7b.h>
#include <Fonts/FreeMonoBoldOblique18pt7b.h>

// Pin definitions for Waveshare 2.66" display
#define EPD_CS   5    // Chip Select
#define EPD_DC   17   // Data/Command
#define EPD_RST  16   // Reset
#define EPD_BUSY 4    // Busy

// Display object for Waveshare 2.66-inch B/W (296x152)
GxEPD2_BW<GxEPD2_266_BN, GxEPD2_266_BN::HEIGHT> display(GxEPD2_266_BN(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

void setup() {
  Serial.begin(115200);
  Serial.println("E-Paper Bold Italic Monospace Font Demo");
  
  // Initialize SPI
  SPI.begin();
  
  // Set up display pins
  pinMode(EPD_BUSY, INPUT_PULLUP);
  pinMode(EPD_CS, OUTPUT);
  pinMode(EPD_DC, OUTPUT);
  pinMode(EPD_RST, OUTPUT);
  
  // Reset sequence
  digitalWrite(EPD_RST, HIGH);
  delay(20);
  digitalWrite(EPD_RST, LOW);
  delay(20);
  digitalWrite(EPD_RST, HIGH);
  delay(200);
  
  // Initialize display
  Serial.println("Initializing display...");
  display.init(115200);
  display.setRotation(3);  // Landscape mode
  
  // Draw font samples
  showBoldItalicMonoFonts();
  
  Serial.println("Font samples displayed.");
}

void loop() {
  // Nothing to do in loop
  delay(10000);
}

void showBoldItalicMonoFonts() {
  display.setFullWindow();
  display.firstPage();
  
  do {
    display.fillScreen(GxEPD_WHITE);
    
    int y = 30; // Starting y position
    
    // Bold Italic (Oblique) Monospace Fonts
    // 9pt
    display.setFont(&FreeMonoBoldOblique9pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(5, y);
    display.print("Bold Italic Mono 9pt");
    
    y += 25;
    display.setCursor(5, y);
    display.print("ABCDEFG 1234567890");
    
    // 12pt
    y += 40;
    display.setFont(&FreeMonoBoldOblique12pt7b);
    display.setCursor(5, y);
    display.print("Bold Italic Mono 12pt");
    
    y += 30;
    display.setCursor(5, y);
    display.print("ABCDEFG 123456");
    
    // 18pt
    y += 45;
    display.setFont(&FreeMonoBoldOblique18pt7b);
    display.setCursor(5, y);
    display.print("Bold Italic 18pt");
    
    y += 35;
    display.setCursor(5, y);
    display.print("ABCDEF 1234");
    
  } while (display.nextPage());
} 