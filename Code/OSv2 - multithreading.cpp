#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "Audio.h" 
#include <TFT_eSPI.h>
#include <ESP32Servo.h>
#include <driver/i2s.h>
#include "Secrets.h"

// ==============================
//      HARDWARE PINS
// ==============================
#define I2S_BCLK      4 
#define I2S_LRC       5 
#define I2S_DOUT      6  
#define I2S_DIN       7  
#define SERVO_PIN_X   1 
#define SERVO_PIN_Y   2  
#define SCREEN_RST    21 

// ==============================
//      GLOBALS
// ==============================
Audio audio; 
TFT_eSPI tft = TFT_eSPI(); 
Servo servoX;
Servo servoY;

// Mic Buffer (Global)
#define I2S_PORT I2S_NUM_0
#define BUFFER_LEN 512
int16_t sBuffer[BUFFER_LEN];

// State
String lastStatus = ""; 
unsigned long lastServoMove = 0;
bool inSpeakerMode = false;
unsigned long micCooldown = 0;

// Threading Handles
TaskHandle_t aiTaskHandle = NULL;
bool aiRequestPending = false;     
bool aiResponseReady = false;      
String globalResponse = "";        

// JSON Buffers (Global)
DynamicJsonDocument chatDoc(4096); 
DynamicJsonDocument responseDoc(8192);

// Gemini Config
const char* MODEL_ID = "gemini-1.5-flash"; 
String apiUrl = "https://generativelanguage.googleapis.com/v1beta/models/" + String(MODEL_ID) + ":generateContent?key=" + GEMINI_API_KEY;

// ==============================
//      DRIVER MANAGEMENT
// ==============================
void enterMicMode() {
    if (!inSpeakerMode) return; 

    i2s_driver_uninstall(I2S_PORT);
    pinMode(I2S_DOUT, OUTPUT);
    digitalWrite(I2S_DOUT, LOW); 

    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 512,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCLK,
        .ws_io_num = I2S_LRC,
        .data_out_num = -1,
        .data_in_num = I2S_DIN
    };
    i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_PORT, &pin_config);
    i2s_zero_dma_buffer(I2S_PORT);
    
    inSpeakerMode = false;
    micCooldown = millis() + 1000; 
}

void enterSpeakerMode() {
    if (inSpeakerMode) return; 

    i2s_driver_uninstall(I2S_PORT);
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(14); 
    
    inSpeakerMode = true;
}

// ==============================
//      UI 
// ==============================
void drawFace(String status, uint16_t color) {
    if (status == lastStatus) return;
    lastStatus = status;

    tft.fillScreen(TFT_BLACK);
    
    int w = tft.width();
    int h = tft.height();
    int eyeRadius = w / 10;
    int eyeY = h / 3;
    int leftEyeX = w / 4;
    int rightEyeX = (w * 3) / 4;
    int mouthY = (h * 2) / 3;

    tft.setTextColor(color, TFT_BLACK);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(status, w / 2, h - 30);

    if(status == "LISTENING" || status == "ONLINE" || status == "BOOTING...") {
        tft.fillCircle(leftEyeX, eyeY, eyeRadius, TFT_GREEN);
        tft.fillCircle(rightEyeX, eyeY, eyeRadius, TFT_GREEN);
        tft.fillCircle(w/2, mouthY, 40, TFT_GREEN);
        tft.fillCircle(w/2, mouthY - 5, 40, TFT_BLACK); 
    } 
    else if (status == "THINKING") {
        tft.fillCircle(leftEyeX, eyeY, eyeRadius, TFT_BLUE);
        tft.fillCircle(rightEyeX, eyeY, eyeRadius, TFT_BLUE);
        tft.fillRect(w/2 - 40, mouthY, 80, 10, TFT_BLUE);
    } 
    else {
        tft.fillCircle(leftEyeX, eyeY, eyeRadius, TFT_RED);
        tft.fillCircle(rightEyeX, eyeY, eyeRadius, TFT_RED);
        tft.fillCircle(w/2, mouthY + 10, 20, TFT_WHITE);
    }
}

// ==============================
//      TASK 1: BRAIN (AI)
// ==============================
void brainTask(void *pvParameters) {
    while (true) {
        if (aiRequestPending) {
            if(WiFi.status() == WL_CONNECTED) {
                HTTPClient http;
                http.setReuse(true); 
                http.begin(apiUrl);
                http.addHeader("Content-Type", "application/json");

                chatDoc.clear();
                responseDoc.clear();

                JsonObject content = chatDoc.createNestedObject("contents").createNestedObject("0");
                JsonObject parts = content.createNestedArray("parts").createNestedObject();
                parts["text"] = "You are a witty robot. Answer in 1 short sentence. Prompt: I just clapped my hands.";

                String jsonOutput;
                serializeJson(chatDoc, jsonOutput);
                int httpCode = http.POST(jsonOutput);
                
                if (httpCode > 0) {
                    String response = http.getString();
                    deserializeJson(responseDoc, response);
                    if (responseDoc.containsKey("candidates")) {
                         globalResponse = responseDoc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
                    } else {
                         globalResponse = "I am confused.";
                    }
                } else {
                    globalResponse = "Internet Error.";
                }
                http.end();
            } else {
                globalResponse = "No WiFi.";
            }
            aiRequestPending = false; 
            aiResponseReady = true;   
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

// ==============================
//      TASK 2: ROBOT (Logic)
// ==============================
// We move the entire loop() into here and give it a HUGE stack (20KB)
void robotTask(void *pvParameters) {
    while(true) {
        
        // --- 1. HANDLE AI RESPONSE ---
        if (aiResponseReady) {
            aiResponseReady = false; 
            Serial.print("AI Said: "); Serial.println(globalResponse);
            
            drawFace("SPEAKING", TFT_RED);
            String tts = "https://translate.google.com/translate_tts?ie=UTF-8&tl=en&client=tw-ob&q=" + globalResponse;
            audio.connecttohost(tts.c_str());
        }

        // --- 2. SPEAKER MODE ---
        if (inSpeakerMode) {
            audio.loop(); 
            
            if (!audio.isRunning() && !aiRequestPending && !aiResponseReady) {
                 delay(100); 
                 enterMicMode();
                 drawFace("LISTENING", TFT_GREEN);
                 servoX.write(90); servoY.write(90);
            }
            
            if (millis() - lastServoMove > 100) { 
                int moveX = 90 + random(-15, 15);
                int moveY = 90 + random(-10, 10);
                servoX.write(moveX); servoY.write(moveY);
                lastServoMove = millis();
            }
        } 
        // --- 3. LISTENING MODE ---
        else {
            if (!aiRequestPending && millis() > micCooldown) {
                size_t bytesIn = 0;
                i2s_read(I2S_PORT, &sBuffer, 512, &bytesIn, 0); // Read 512 bytes safely
                
                long sum = 0;
                // Process only valid bytes
                int samples = bytesIn / 2;
                if(samples > 0) {
                    for (int i = 0; i < samples; i++) sum += abs(sBuffer[i]);
                    int volume = sum / samples;

                    if (volume > 3000) { 
                        Serial.println("Clap Detected!");
                        enterSpeakerMode(); 
                        drawFace("THINKING", TFT_BLUE);
                        aiRequestPending = true;
                    }
                }
            }
        }
        
        // Critical: Allow other tasks to run
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

// ==============================
//      SETUP
// ==============================
void setup() {
    Serial.begin(115200);
    delay(1000);

    // Screen
    pinMode(SCREEN_RST, OUTPUT);
    digitalWrite(SCREEN_RST, HIGH); delay(50);
    digitalWrite(SCREEN_RST, LOW); delay(50);
    digitalWrite(SCREEN_RST, HIGH); delay(100);
    tft.init();
    tft.setRotation(1);
    tft.invertDisplay(false); 
    tft.setSwapBytes(true);
    drawFace("BOOTING...", TFT_WHITE);

    // Servos
    servoX.attach(SERVO_PIN_X);
    servoY.attach(SERVO_PIN_Y);
    servoX.write(90); servoY.write(90);

    // WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) delay(500);
    drawFace("ONLINE", TFT_GREEN);
    
    // --- SPAWN TASKS ---
    
    // 1. Brain Task (AI) - 40KB Stack
    xTaskCreatePinnedToCore(brainTask, "BrainTask", 40960, NULL, 1, &aiTaskHandle, 1);
    
    // 2. Robot Task (Logic) - 20KB Stack (Replaces loop())
    xTaskCreatePinnedToCore(robotTask, "RobotTask", 20480, NULL, 1, NULL, 1);

    // Start in Mic Mode
    inSpeakerMode = true; 
    enterMicMode();
}

// ==============================
//      EMPTY LOOP
// ==============================
void loop() {
    // Delete the default loop task to free up 8KB of RAM
    vTaskDelete(NULL); 
}
