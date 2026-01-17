# Franken-bot (Frank)

This one project brought so many things that I have fallen in love with together. I think its fair to say this is pushing what people can do/have done with ESP32s. I set out to upgrade the brain and display (2.8in touch w/ SD card slot) of the StackChan to improve usability as a desk companion. I ended up fundamentally changing the project from a cute robot AI demo to an embedded AI system that happens to be a robot.

I also wanted to use common, cheap/salvaged parts and 3d prints I could order from the local library. This code is 100% original and shares no lines in common with the official firmware. There is LOTS of room for expansion. 


**Software:**

_I believe I have made considerable improvements to the stock firmware, such as removing the local speech processing in favor of sending a raw audio file to the LLM freeing memory and enabling much more nuance in audio processing. I have also meaningfully improved the memory managment. There are many more, and I continue to improve it regularly. Please share your ideas for improvements!_

   **OSv3.0**
   -Recording raw WAV file, encoding it and sending to Gemini as a JSON rather than local speech to text
   -Automatically detect ambient noise level, set mic level accordingly
   -Variable spoken prompt length - no wasted bytes. This involves checks and balances.
   -More memory optimization - working around ESP32s internal heap memory and writing+encoding JSON string in PSRAM. Setting bank of PSRAM aside for audio prompt recordings up to 30 seconds. 
   -Ditched LLM auto selection and model agnostic framework in favor of immediate functionality. Will add back later.

   **OSv2.6**
   -Working LLM response to audio! By far the most complex stack I've ever built.
   -Automatic model selection put on hold after much API wrestling. (gemini-flash-latest).
   -Added local database of randomized prompts and local database in case of LLM failure. (redacted from code section OS2.6)


   **OSv2.5 'Frank'** 

   -Fixed dynamic driver handling - automatic handling had many bugs, mostly pointer resolution after automatic delete of     previous driver
   -Smoothed audio - fixed a looping issue that caused audio stutter, added 100KB buffer in PSRAM for audio to address latency, forced mono mode
   -Auto-Model Select - Frank will now look at available Gemini models and choose the best one for him. _NOT ABLE TO USE TEXT TO SPEECH MODELS YET_
   -Changed name from Stack-bot to Franken-bot. Codename 'Frank'

   **OSv2-multithreading**

   My first working RTOS for the system which incorporates all the sensors. I am not a coder, but Gemini sure is. I ended up doing multithreading and some clever memory management (my first stack overflow!) to handle some of the larger functions without crashing the rather small default esp32 void loop. To that end, I spawned two dedicated FreeRTOS tasks with explicit memory allocation. brainTask exclusively handles AI, SSL and JSON processing with a 40KB stack. robotTask handles the I/O and provides 20KB. Void loop was then left empty. I also implemented dynamic driver swapping, which uninstalls the mic or speaker driver before installing the other. The global JSON buffers were moved to heap memory as well, since the buffers get massive. There are tons of other bug fixes, like properly sharing the buses, but this is confirmed working with everyhting except the SD card. Serves mostly as a proof of concept and launching off point - code is above under OSv2 - multithreading.  


**Hardware**

Brain - _Lonely Binary ESP32-S3 N16R8_.  Dual core FreeRTOS architecture and dynamic driver switching for shared SPI bus. Optimized memory handling.

Display - _2.8" IL19341 Touch Screen_. SPI interface (important to keep cables down) 320x240 resolution. This should enable us to display all kinds of data effectively, or act as a button, etc. Added SD slot.

Movement - _2x SG90 Servos_. Can be upgraded to x6 for walking capability (untested).

**3D Printing & Assembly**

Head - Case for 2.8" IL19341, bottom power by DorffMeister
  Body_-_Bottom_Power.stl
  Lid_-_Bottom_Power.stl

  Body - Standard StackChan repo from meganataaan 
    feet_SG90.stl
    bracket_SG90_b.stl

These cases do not fit together. Rough up the horns of the server arms and and superglue em.



**Software Enviornment**

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




