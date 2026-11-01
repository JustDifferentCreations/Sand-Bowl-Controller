#include "mcc_generated_files/system/system.h"
#include <avr/io.h>
#include <util/delay.h>
#include "ssd1306.h"
#include <stdio.h>
#include <stdbool.h>

// === Motor state definitions ===
#define MOTOR_STOP  0
#define MOTOR_LEFT  1
#define MOTOR_RIGHT 2

// === WS2812 LED setup ===
#define LED_PIN_bm PIN7_bm
#define NUM_LEDS 9
typedef struct {
    uint8_t g;
    uint8_t r;
    uint8_t b;
} RGB_t;
RGB_t leds[NUM_LEDS];
uint8_t color = 1;
uint8_t brightness = 5;

// === Stepper control ===
#define NUM_STEPS 8
uint8_t stepIndex = 0;
uint16_t stepDelay = 10500;
uint8_t motorState = MOTOR_RIGHT;
bool paused = true;
bool fault = false;

// === Function prototypes ===
void Stepper_Update(void);
void Display_Update(void);
void Delay_us(uint16_t delay);
int Return_Speed(int x);
void WS2812_Send(void);
void WS2812_SetAll(uint8_t r, uint8_t g, uint8_t b);

// Button + fault handlers
void SW1_InterruptHandler(void);
void SW2_InterruptHandler(void);
void SW3_InterruptHandler(void);
void SW4_InterruptHandler(void);
void SW5_InterruptHandler(void);
void nFault_InterruptHandler(void);

// === Stepper sequence (half-step) ===
const uint8_t stepSequence[NUM_STEPS][4] = {
    {1, 0, 0, 0},
    {1, 0, 1, 0},
    {0, 0, 1, 0},
    {0, 1, 1, 0},
    {0, 1, 0, 0},
    {0, 1, 0, 1},
    {0, 0, 0, 1},
    {1, 0, 0, 1}
};

// === RGB Colors) ===
const uint8_t available_colors[10][3] = { 
    {0, 0, 0},         // off
    {255, 0, 0},       // green
    {0, 255, 0},       // red
    {0, 0, 255},       // blue
    {64, 224, 208},    // turquoise
    {255, 255, 255},   // white
    {255, 165, 0},     // yellow / orange
    {128, 0, 128},     // purple
    {255, 0, 255},     // magenta
    {0, 255, 255}      // cyan
};

// === Main ===
int main(void){
    SYSTEM_Initialize();

    // Configure PA2/PA3 for OLED I²C, PA7 for WS2812
    PORTA.DIRSET = (1 << 2) | (1 << 3) | LED_PIN_bm;

    // Setup button interrupts
    SW1_SetInterruptHandler(SW1_InterruptHandler);
    SW2_SetInterruptHandler(SW2_InterruptHandler);
    SW3_SetInterruptHandler(SW3_InterruptHandler);
    SW4_SetInterruptHandler(SW4_InterruptHandler);
    SW5_SetInterruptHandler(SW5_InterruptHandler);
    nFault_SetInterruptHandler(nFault_InterruptHandler);

    // Display setup
    SSD1306_Init();
    SSD1306_Clear();
    SSD1306_WriteString(0, 0, "Just Different");
    SSD1306_WriteString(0, 10, "JustDif.com");
    SSD1306_WriteString(0, 20, "Initializing...");
    SSD1306_Display();
    
    // update screen after pause
    _delay_ms(800);
    Display_Update();

    // LED startup
    WS2812_SetAll(0, 0, 0);   // lights off
    WS2812_Send();

    // start while routine
    while (1){
        Stepper_Update();
    }
}

// === Runtime delay ===
void Delay_us(uint16_t delay){
    while (delay--) _delay_us(1);
}

// === Stepper update ===
void Stepper_Update(void){
    if (paused || motorState == MOTOR_STOP)
    {
        PORTD.OUTCLR = (1 << 0)|(1 << 1)|(1 << 2)|(1 << 3);
        return;
    }

    PORTD.OUTCLR = (1 << 0)|(1 << 1)|(1 << 2)|(1 << 3);
    PORTD.OUTSET = (stepSequence[stepIndex][0] ? (1 << 0) : 0)
                 | (stepSequence[stepIndex][1] ? (1 << 1) : 0)
                 | (stepSequence[stepIndex][2] ? (1 << 3) : 0)
                 | (stepSequence[stepIndex][3] ? (1 << 2) : 0);

    if (motorState == MOTOR_LEFT) {
        if (++stepIndex >= NUM_STEPS) stepIndex = 0;
    } else if (motorState == MOTOR_RIGHT) {
        if (stepIndex == 0) stepIndex = NUM_STEPS - 1;
        else stepIndex--;
    }

    Delay_us(stepDelay);
}

// === Display update ===
void Display_Update(void){
    SSD1306_Clear();

    // top line
    if (fault)
        SSD1306_WriteString(46, 0, "FAULT!");
    else if (paused)
        SSD1306_WriteString(46, 0, "Paused");
    else
        SSD1306_WriteString(43, 0, "Running");
    // second line
    if (motorState == MOTOR_LEFT)
        SSD1306_WriteString(3, 8, "Dir: Left");
    else if (motorState == MOTOR_RIGHT)
        SSD1306_WriteString(3, 8, "Dir: Right");
    else
        SSD1306_WriteString(3, 8, "Dir: Stop");
    char colorText[8];
    sprintf(colorText, "Color: %d", color);
    SSD1306_WriteString(68, 8, colorText);
    // third line
    char speedText[9];
    sprintf(speedText, "Speed: %d", Return_Speed(stepDelay));
    SSD1306_WriteString(3, 16, speedText);
    char brightText[9];
    sprintf(brightText, "Bright: %d", brightness);
    SSD1306_WriteString(68, 16, brightText);
    // fourth line
    SSD1306_WriteString(32, 24, "JustDif.com");
    SSD1306_Display();
    
    // update colors now
   uint8_t scale = (brightness * 255) / 10;  // map 1?10 ? 25?255
   uint8_t r = (available_colors[color-1][0] * scale) / 255;
   uint8_t g = (available_colors[color-1][1] * scale) / 255;
   uint8_t b = (available_colors[color-1][2] * scale) / 255;
   WS2812_SetAll(r, g, b);
   WS2812_Send();
}

void WS2812_Send(void){
    cli();
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        uint8_t grb[3] = { leds[i].g, leds[i].r, leds[i].b }; // explicit order
        for (uint8_t j = 0; j < 3; j++) {
            uint8_t val = grb[j];
            for (uint8_t bit = 0; bit < 8; bit++) {
                if (val & 0x80) {
                    PORTA.OUTSET = LED_PIN_bm;
                    _delay_us(0.8);
                    PORTA.OUTCLR = LED_PIN_bm;
                    _delay_us(0.45);
                } else {
                    PORTA.OUTSET = LED_PIN_bm;
                    _delay_us(0.4);
                    PORTA.OUTCLR = LED_PIN_bm;
                    _delay_us(0.85);
                }
                val <<= 1;
            }
        }
    }
    sei();
    _delay_us(60); // reset pulse
}


void WS2812_SetAll(uint8_t r, uint8_t g, uint8_t b){
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        leds[i].r = r;
        leds[i].g = g;
        leds[i].b = b;
    }
}


// === Speed table ===
int Return_Speed(int x){
    switch(x){
        case 3000: return 10;
        case 4500: return 9;
        case 6000: return 8;
        case 7500: return 7;
        case 9000: return 6;
        case 10500: return 5;
        case 12000: return 4;
        case 13500: return 3;
        case 15000: return 2;
        case 16500: return 1;
    }
}

// === Button Handlers ===
void SW1_InterruptHandler(void) { 
    paused = !paused; 
    fault = false; 
    Display_Update(); }

void SW2_InterruptHandler(void) { 
    fault = false; 
    if ((paused)&&(color > 1)){ color -= 1; Display_Update(); return;}
    if (motorState == MOTOR_LEFT){return;} 
    if (!paused) { motorState = MOTOR_LEFT; Display_Update(); } 
}
    
void SW3_InterruptHandler(void) { 
    fault = false; 
    if ((paused)&&(color < 10)){ color += 1; Display_Update(); return;}
    if (motorState == MOTOR_RIGHT){return;}  
    if (!paused) { motorState = MOTOR_RIGHT; Display_Update(); } 
}

void SW4_InterruptHandler(void) { 
    fault = false; 
    if ((paused)&&(brightness > 1)){ brightness -= 1; Display_Update(); return;}
    if (stepDelay == 16500){return;} 
    if ((!paused) && (stepDelay < 16500)){ stepDelay += 1500; Display_Update(); }
}

void SW5_InterruptHandler(void) { 
    fault = false; 
    if ((paused)&&(brightness < 10)){ brightness += 1; Display_Update(); return;}
    if (stepDelay == 3000){return;} 
    if ((!paused) && (stepDelay > 3000)){ stepDelay -= 1500; Display_Update(); }
}

void nFault_InterruptHandler(void){
    fault = true;
    motorState = MOTOR_STOP;
    paused = false;
    Display_Update();
}
