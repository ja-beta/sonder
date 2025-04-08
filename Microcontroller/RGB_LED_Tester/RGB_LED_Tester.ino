/*
 * Ultra-Simple RGB LED Color Tester
 * Works with any Arduino board
 */

#define LED_R 14  // Red pin
#define LED_G 12  // Green pin  
#define LED_B 13  // Blue pin

void setup() {
  Serial.begin(115200);
  
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  
  // All LEDs off
  analogWrite(LED_R, 0);
  analogWrite(LED_G, 0);
  analogWrite(LED_B, 0);
  
  Serial.println("\n\n=== Simple RGB Color Tester ===");
  Serial.println("Send R,G,B values (0-255 each)");
  Serial.println("Example: 255,0,128");
  Serial.println("Or try: red, green, blue, cyan, yellow, purple, white, off");
}

void loop() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    if (input.equals("red")) setColor(255, 0, 0);
    else if (input.equals("green")) setColor(0, 255, 0);
    else if (input.equals("blue")) setColor(0, 0, 255);
    else if (input.equals("cyan")) setColor(0, 255, 255);
    else if (input.equals("yellow")) setColor(255, 255, 0);
    else if (input.equals("purple")) setColor(255, 0, 255);
    else if (input.equals("white")) setColor(255, 255, 255);
    else if (input.equals("off")) setColor(0, 0, 0);
    else {
      // Try to parse R,G,B values
      int firstComma = input.indexOf(',');
      int lastComma = input.lastIndexOf(',');
      
      if (firstComma > 0 && lastComma > firstComma) {
        int r = input.substring(0, firstComma).toInt();
        int g = input.substring(firstComma + 1, lastComma).toInt();
        int b = input.substring(lastComma + 1).toInt();
        
        setColor(r, g, b);
      }
    }
  }
  
  delay(50);
}

void setColor(int r, int g, int b) {
  // Keep values in range
  r = constrain(r, 0, 255);
  g = constrain(g, 0, 255);
  b = constrain(b, 0, 255);
  
  // Set colors
  analogWrite(LED_R, r);
  analogWrite(LED_G, g);
  analogWrite(LED_B, b);
  
  // Show what was set
  Serial.print("Color set to: R=");
  Serial.print(r);
  Serial.print(", G=");
  Serial.print(g);
  Serial.print(", B=");
  Serial.println(b);
} 