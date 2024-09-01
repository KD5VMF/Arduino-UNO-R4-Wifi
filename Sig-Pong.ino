#include "Arduino_LED_Matrix.h"
ArduinoLEDMatrix matrix;

uint8_t frame[8][12] = {0};

int y = 4; // starting y position
int x = 4; // starting x position
int b = 1; // starting y direction
int a = 1; // starting x direction
int speed = 300; // initial delay

int paddle1Y = 4; // Paddle 1 position
int paddle2Y = 4; // Paddle 2 position

bool player1CanMove = true;
bool player2CanMove = false;

void setup() {
  Serial.begin(115200);
  matrix.begin();
}

void loop() {
  // Clear previous paddle positions
  frame[paddle1Y][0] = 0;
  frame[paddle1Y + 1][0] = 0;
  frame[paddle2Y][11] = 0;
  frame[paddle2Y + 1][11] = 0;

  // AI for paddle 1 (left) - only move if allowed
  if (player1CanMove) {
    if (y > paddle1Y + 1) paddle1Y++;
    if (y < paddle1Y) paddle1Y--;
  }

  // AI for paddle 2 (right) - only move if allowed
  if (player2CanMove) {
    if (y > paddle2Y + 1) paddle2Y++;
    if (y < paddle2Y) paddle2Y--;
  }

  // Draw paddles
  frame[paddle1Y][0] = 1;
  frame[paddle1Y + 1][0] = 1;
  frame[paddle2Y][11] = 1;
  frame[paddle2Y + 1][11] = 1;

  // Move the ball
  frame[y][x] = 1;
  matrix.renderBitmap(frame, 8, 12);
  delay(speed);
  frame[y][x] = 0;

  // Ball movement and bouncing
  if (y == 7 || y == 0) {
    b = -b;
  }

  // Ball collision with paddles
  if (x == 1 && (y == paddle1Y || y == paddle1Y + 1)) {
    a = -a;
    speed = max(50, speed - 20); // Increase speed
    player1CanMove = false; // Lock player 1
    player2CanMove = true;  // Allow player 2 to move
  } else if (x == 10 && (y == paddle2Y || y == paddle2Y + 1)) {
    a = -a;
    speed = max(50, speed - 20); // Increase speed
    player2CanMove = false; // Lock player 2
    player1CanMove = true;  // Allow player 1 to move
  }

  // Reset game if ball is missed
  if (x == 0 || x == 11) {
    x = 4;
    y = 4;
    a = -a;
    speed = 300; // Reset speed
    player1CanMove = true;
    player2CanMove = false;
  }

  // Update ball position
  y += b;
  x += a;
}
