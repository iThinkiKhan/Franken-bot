# Stack-bot
 
I decided to upgrade the brain and display of the bot, primarily. I also wanted to use common parts and 3d prints I could order from the local library. 

**Hardware**
Brain - Lonely Binary ESP32-S3 N16R8. 16MB flash, 8MB octal PSRAM. Powerful chip by esp32 standards. We need it to drive all the functions.

Display - 2.8" IL19341 Touch Screen. SPI interface (important to keep cables down) 320x240 resolution. This should enable us to display all kinds of data effectively, or act as a button, etc. This also gives us an SD slot.

Movement - 2x SG90 Servos. Can be upgraded to x6 for walking capability (untested).

**3D Printing & Assembly**

Head - Case for 2.8" IL19341, bottom power by DorffMeister
  Body_-_Bottom_Power.stl
  Lid_-_Bottom_Power.stl

  Body - Standard StackChan repo from meganataaan 
    feet_SG90.stl
    bracket_SG90_b.stl

These do not fit together. Rough up the horns and superglue em.

**Software**

Development Environment
IDE: VS Code + PlatformIO
Framework: Arduino

The "Dual Brain" System
The custom operating systems here are designed to be model-agnostic, allowing hot-swapping between AI providers or local models via a preprocessor flag (comment out #define USE_GEMINI for OpenAI).

_Primary Brain (Google Gemini)_:

Model: gemini-1.5-flash
Cost: Free Tier
Role: Daily driver

_Backup Brain (OpenAI)_:

Model: ChatGPT
Cost: Paid API
Role: Fallback or complex reasoning tasks.

Secrets Management
API keys and WiFi credentials are stored in a non-tracked src/Secrets.h file to prevent accidental leaks.

**Wiring**
This is a "shared SPI" system as much as possible to keep wiring manageable. The screen is a shared SPI, meaning touch, screen and SD card all use the same same MOSI MISO and SCK, but they have different CS pins to distinguish them.

_Build one - prototyping with breadboard_

Several inputs need to share common lines, so we are going to use a breadboard to break out pins for prototyping.
--------------------------------------------------------------
Orange - top red rail - 3.3v
Red - top blue rail - 5v
Black - bottom red rail - Ground - double up for clearer signals

SPI Hub BOTTOM SIDE:

SCK Clock - blue wire- GPIO 12 - Row 1 bottom
MOSI - brown wire GPIO 11 Row 2 bottom
MISO - yellow wire GPIO 13 Row 10 bottom

I2S Hub UPPER SIDE:

BCLK - audio clock - purple wire GPIO 4 Row 1 top
LRC - word select - white wire GPIO 5 Row 5 top

CS Screen - purple wire GPIO 10
DC red wire gpio 9
RST green wire gpio 3
T_CS (Touch): -> GPIO 14 (Direct)
LED: -> 3.3V Hub (Or 5V if you want max brightness, but 3.3V is safer).

2. The Microphone (INMP441)
VDD: -> 3.3V Hub (Do NOT use 5V, it will fry).
GND: -> GND Hub
SCK: -> I2S Hub Row D (BCLK)
WS: -> I2S Hub Row E (LRC)
SD: -> GPIO 7 (Direct - Data In).
L/R: -> GND Hub (This sets it to "Left Channel" or Mono).

3. The Amplifier (MAX98357A)
VIN: -> 5V Hub (Louder volume).
GND: -> GND Hub
BCLK: -> I2S Hub Row D (BCLK)
LRC: -> I2S Hub Row E (LRC)
DIN: -> GPIO 6 (Direct - Data Out).
GAIN: -> (Leave empty or connect to GND via resistor for lower volume).
SD: -> (Leave empty, it has an internal pull-up to turn on).

4. The Servos (Muscles)
Brown: -> GND Hub
Red: -> 5V Hub
Orange (Pan): -> GPIO 1
Orange (Tilt): -> GPIO 2
2 100uF capacitors - long leg 5v, short striped leg to ground. 1 for each servo.

Summary of N16R8 Unique Pins
GPIO 1: Servo Pan
GPIO 2: Servo Tilt
GPIO 4: Audio BCLK (Shared)
GPIO 5: Audio LRC (Shared)
GPIO 6: Audio Amp Data
GPIO 7: Audio Mic Data
GPIO 9: Screen DC
GPIO 10: Screen CS
GPIO 11: SPI MOSI (Shared)
GPIO 12: SPI SCK (Shared)
GPIO 13: SPI MISO (Shared)
GPIO 14: Touch CS
GPIO 21: Screen Reset

**Software:**

**OSv2-multithreading**

My first working RTOS for the system which incorporates all the sensors. I am not a coder, but Gemini sure is. I ended up doing multithreading and some clever memory management (my first stack overflow!) to handle some of the larger functions without crashing the rather small default esp32 void loop. To that end, I spawned two dedicated FreeRTOS tasks with explicit memory allocation. brainTask exclusively handles AI, SSL and JSON processing with a 40KB stack. robotTask handles the I/O and provides 20KB. Void loop was then left empty. The global JSON buffers were moved to heap memory as well, since the buffers get massive. There are tons of other bug fixes, like properly sharing the buses, but this is confirmed working with everyhting except the SD card. Serves mostly as a proof of concept and launching off point - code is above under OSv2 - multithreading.  




