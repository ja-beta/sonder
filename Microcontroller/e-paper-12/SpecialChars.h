#ifndef SPECIAL_CHARS_H
#define SPECIAL_CHARS_H

#include <GxEPD2_BW.h>
#include <Adafruit_GFX.h>

class SpecialChars {
  private:
    GxEPD2_BW<GxEPD2_266_BN, GxEPD2_266_BN::HEIGHT>* display;
    
  public:
    SpecialChars(GxEPD2_BW<GxEPD2_266_BN, GxEPD2_266_BN::HEIGHT>* _display) {
      display = _display;
    }
    
    // Draw left double quote (") - TRULY horizontally flipped
    void drawLeftDoubleQuote(int16_t x, int16_t y, uint16_t size = 1) {
      int16_t w = 3 * size;
      int16_t h = 6 * size;
      int16_t spacing = 2 * size;
      
      // For a true horizontal flip, the offset direction needs to be reversed
      
      // First quote (left side)
      display->fillRect(x, y, w, h/2, GxEPD_BLACK);           // Top part 
      display->fillRect(x, y + h/2, w-1, h/2, GxEPD_BLACK);   // Bottom part with LEFT offset
      
      // Second quote (right side)
      display->fillRect(x+spacing+w, y, w, h/2, GxEPD_BLACK);         // Top part
      display->fillRect(x+spacing+w, y + h/2, w-1, h/2, GxEPD_BLACK); // Bottom part with LEFT offset
    }
    
    // Draw right double quote (")
    void drawRightDoubleQuote(int16_t x, int16_t y, uint16_t size = 1) {
      int16_t w = 3 * size;
      int16_t h = 6 * size;
      int16_t spacing = 2 * size;
      
      // Properly oriented: top parts first (vertical flip)
      display->fillRect(x, y, w, h/2, GxEPD_BLACK);
      display->fillRect(x+spacing+w, y, w, h/2, GxEPD_BLACK);
      
      // Bottom parts with offset
      display->fillRect(x+1, y + h/2, w-1, h/2, GxEPD_BLACK);
      display->fillRect(x+spacing+w+1, y + h/2, w-1, h/2, GxEPD_BLACK);
    }
    
    // Draw apostrophe (') - flipped both horizontally and vertically
    void drawApostrophe(int16_t x, int16_t y, uint16_t size = 1) {
      int16_t w = 3 * size;
      int16_t h = 6 * size;
      
      // Flipped both ways
      display->fillRect(x, y, w, h/2, GxEPD_BLACK);
      display->fillRect(x+1, y + h/2, w-1, h/2, GxEPD_BLACK);
    }
    
    // Draw en dash (–)
    void drawEnDash(int16_t x, int16_t y, uint16_t size = 1) {
      int16_t w = 10 * size;
      int16_t h = 2 * size;
      
      display->fillRect(x, y, w, h, GxEPD_BLACK);
    }
    
    // Draw em dash (—)
    void drawEmDash(int16_t x, int16_t y, uint16_t size = 1) {
      int16_t w = 16 * size;
      int16_t h = 2 * size;
      
      display->fillRect(x, y, w, h, GxEPD_BLACK);
    }
    
    // Get width of special character
    int16_t getCharWidth(uint8_t c, uint16_t size = 1) {
      int16_t baseWidth;
      
      switch(c) {
        case 0x01: baseWidth = 12; break; // Left double quote - increased width
        case 0x02: baseWidth = 18; break; // Right double quote - increased width to add spacing
        case 0x03: baseWidth = 6; break;  // Apostrophe - increased width
        case 0x04: baseWidth = 12; break; // En dash - increased width
        case 0x05: baseWidth = 18; break; // Em dash - increased width
        default: return 0;
      }
      
      // Get font metrics to determine scale
      int16_t x1, y1;
      uint16_t w, h;
      display->getTextBounds("M", 0, 0, &x1, &y1, &w, &h);
      
      // Scale based on font size
      if (h < 12) {
        // Small font
        return baseWidth * size;
      } else if (h > 19) {
        // Large font needs much bigger characters
        return baseWidth * size;  // Extra scaling for large font
      } else {
        // Medium font
        return baseWidth * size * 1.2;  // Slight scaling for medium font
      }
    }
    
    // Draw a special character and return its width
    int16_t drawChar(int16_t x, int16_t y, uint8_t c, uint16_t fontSize = 1) {
      // Get font metrics to determine appropriate scale
      int16_t x1, y1;
      uint16_t w, h;
      display->getTextBounds("M", 0, 0, &x1, &y1, &w, &h);
      
      // Set size and vertical offset based on font size
      uint16_t size;
      int16_t verticalOffset;
      
      if (h < 12) {
        // Small font
        size = 1;
        verticalOffset = -h + 2;  // Adjusted to be at the top
      } else if (h > 19) {
        // Large font
        size = 2;  // Increased size for large font
        verticalOffset = -h + 5;  // Slightly higher
      } else {
        // Medium font
        size = 1.5;  // Reduced from 2 to 1.5
        verticalOffset = -h + 3;  // Adjusted to be at the top
      }
      
      int16_t width = getCharWidth(c, size);
      
      // Adjust spacing for quotes
      if (c == 0x01) {  // Left quote
        if (h > 19) {
          // For large font
          width -= 1;  // Reduced negative adjustment - was -3 which might be too much
        } else {
          width += 2;  // Keep normal spacing for other fonts
        }
      } else if (c == 0x02) {  // Right quote
        x += 5;
      }
      
      // Adjust vertical position for specific characters
      int16_t charVerticalOffset = verticalOffset;
      if (c == 0x01 || c == 0x02 || c == 0x03) {  // Quotes and apostrophe
        charVerticalOffset -= 6;  // Move slightly higher
      }
      
      switch(c) {
        case 0x01: 
          drawLeftDoubleQuote(x, y + charVerticalOffset, size);
          break;
        case 0x02: 
          drawRightDoubleQuote(x, y + charVerticalOffset, size);
          break;
        case 0x03: 
          drawApostrophe(x, y + charVerticalOffset, size);
          break;
        case 0x04: 
          drawEnDash(x, y + (charVerticalOffset / 2), size); 
          break;
        case 0x05: 
          drawEmDash(x, y + (charVerticalOffset / 2), size);  
          break;
      }
      
      return width;
    }
    
    // Replace special characters in a string with placeholders
    String replaceWithPlaceholders(const String& text) {
      String result = text;
      
      // Use byte arrays for UTF-8 characters to avoid compiler issues
      // Left double quote (")
      const char leftQuote[] = {0xE2, 0x80, 0x9C, 0}; // U+201C
      result.replace(leftQuote, String((char)0x01));
      
      // Right double quote (")
      const char rightQuote[] = {0xE2, 0x80, 0x9D, 0}; // U+201D
      result.replace(rightQuote, String((char)0x02));
      
      // Curly apostrophe (')
      const char apostrophe[] = {0xE2, 0x80, 0x99, 0}; // U+2019
      result.replace(apostrophe, String((char)0x03));
      
      // En dash (–)
      const char enDash[] = {0xE2, 0x80, 0x93, 0}; // U+2013
      result.replace(enDash, String((char)0x04));
      
      // Em dash (—)
      const char emDash[] = {0xE2, 0x80, 0x94, 0}; // U+2014
      result.replace(emDash, String((char)0x05));
      
      return result;
    }
    
    // Check if character is one of our special placeholders
    bool isSpecialChar(char c) {
      return (c >= 0x01 && c <= 0x05);
    }
};

#endif