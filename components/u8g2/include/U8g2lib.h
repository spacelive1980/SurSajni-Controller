#ifndef U8G2LIB_H
#define U8G2LIB_H

#include <stdint.h>
#include <stddef.h>

// U8g2 Rotation values
#define U8G2_R0   0
#define U8G2_R1   1
#define U8G2_R2   2
#define U8G2_R3   3

// Pin definitions
#define U8X8_PIN_NONE 255

// Font declarations (external - would normally come from u8g2 library)
extern const uint8_t u8g2_font_6x12_tr[] ;
extern const uint8_t u8g2_font_7x13_tf[] ;
extern const uint8_t u8g2_font_helvB14_tr[] ;
extern const uint8_t u8g2_font_ncenB08_tr[] ;
extern const uint8_t u8g2_font_ncenR10_tr[] ;

// Simplified U8g2 class for ESP-IDF
class U8G2_SSD1306_128X64_NONAME_F_HW_I2C {
public:
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C(uint8_t rotation = U8G2_R0, 
                                         uint8_t reset = U8X8_PIN_NONE,
                                         uint8_t scl = 22, 
                                         uint8_t sda = 21);
    
    void begin();
    void clearBuffer();
    void sendBuffer();
    void setFont(const uint8_t *font);
    void drawStr(int16_t x, int16_t y, const char *s);
    void drawXBMP(int16_t x, int16_t y, uint16_t w, uint16_t h, const uint8_t *bitmap);
    void drawBox(int16_t x, int16_t y, uint16_t w, uint16_t h);
    void drawFrame(int16_t x, int16_t y, uint16_t w, uint16_t h);
    void drawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2);
    void drawCircle(int16_t x, int16_t y, uint16_t r);
    
    uint16_t getStrWidth(const char *s);
    int8_t getFontAscent();
    int8_t getFontDescent();
    
private:
    uint8_t _rotation;
    uint8_t _scl;
    uint8_t _sda;
    uint8_t _reset;
    const uint8_t *_font;
};

#endif // U8G2LIB_H
