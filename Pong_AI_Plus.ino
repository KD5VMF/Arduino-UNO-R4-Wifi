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

int scorePlayer1 = 0;
int scorePlayer2 = 0;

int difficulty = 2; // Difficulty level (1 = Easy, 2 = Medium, 3 = Hard)

void setup() {
  Serial.begin(115200);
  matrix.begin();
  Serial.println("Game Start!");
}

void loop() {
  // Clear previous paddle positions
  frame[paddle1Y][0] = 0;
  frame[paddle1Y + 1][0] = 0;
  frame[paddle2Y][11] = 0;
  frame[paddle2Y + 1][11] = 0;

  // Improved AI for paddle 1 (left) - Predicts ball landing position
  if (player1CanMove) {
    int predictedY = predictBallY(x, y, a, b, 1); // Predict ball landing position
    movePaddle(paddle1Y, predictedY);
  }

  // Improved AI for paddle 2 (right) - Predicts ball landing position
  if (player2CanMove) {
    int predictedY = predictBallY(x, y, a, b, 10); // Predict ball landing position
    movePaddle(paddle2Y, predictedY);
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

  // Check if ball is missed and update scores
  if (x == 0) {
    scorePlayer2++;
    Serial.print("Player 2 Scores! Player 1: ");
    Serial.print(scorePlayer1);
    Serial.print(" | Player 2: ");
    Serial.println(scorePlayer2);
    resetGame();
  } else if (x == 11) {
    scorePlayer1++;
    Serial.print("Player 1 Scores! Player 1: ");
    Serial.print(scorePlayer1);
    Serial.print(" | Player 2: ");
    Serial.println(scorePlayer2);
    resetGame();
  }

  // Update ball position
  y += b;
  x += a;
}

// Function to reset the game after scoring
void resetGame() {
  x = 4;
  y = 4;
  a = -1;
  b = -1;
  speed = 300;
  player1CanMove = true;
  player2CanMove = false;
  delay(1000); // Pause before restarting
}

// Function to predict where the ball will land
int predictBallY(int ballX, int ballY, int dirX, int dirY, int targetX) {
  int predictedY = ballY + dirY * abs(targetX - ballX);
  while (predictedY < 0 || predictedY > 7) {
    if (predictedY < 0) predictedY = -predictedY;
    if (predictedY > 7) predictedY = 14 - predictedY;
  }
  return predictedY;
}

// Function to move paddle towards predicted position
void movePaddle(int &paddleY, int targetY) {
  if (paddleY + 1 < targetY) paddleY++;
  if (paddleY > targetY) paddleY--;
}
