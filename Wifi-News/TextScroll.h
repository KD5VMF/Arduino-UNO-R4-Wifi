#ifndef TEXTSCROLL_H
#define TEXTSCROLL_H

#include <ArduinoGraphics.h>
#include <Arduino_LED_Matrix.h>

class TextScroller {
  public:
    TextScroller() {
      // Constructor
    }

    void init() {
      matrix.begin();  // Initialize the matrix
    }

    void scrollText(const char* text, int scrollSpeed = 500, int delayTime = 50) {
      matrix.beginDraw();  // Start drawing on the matrix

      matrix.stroke(0xFFFFFF);  // Set the color to white
      matrix.textFont(Font_5x7);  // Use the 5x7 font
      matrix.textScrollSpeed(scrollSpeed);  // Set the scrolling speed

      // Display scrolling text with leading and trailing spaces for seamless scroll
      matrix.beginText(0, 1, 0xFFFFFF);
      matrix.print(text);  // Print the text to scroll
      matrix.endText(SCROLL_LEFT);  // Scroll the text

      matrix.endDraw();  // End the drawing and push the data to the matrix

      delay(delayTime);  // Add a small delay to control the loop speed
    }

  private:
    ArduinoLEDMatrix matrix;
};

#endif
