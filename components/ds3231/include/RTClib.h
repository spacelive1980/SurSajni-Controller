#ifndef RTCLIB_H
#define RTCLIB_H

#include <stdint.h>
#include <time.h>
#include "driver/i2c.h"

class TimeSpan {
public:
    TimeSpan(int32_t seconds = 0);
    TimeSpan(int16_t days, int8_t hours, int8_t minutes, int8_t seconds);
    int32_t totalseconds() const { return _seconds; }
    int16_t days() const { return _seconds / 86400L; }
    int8_t hours() const { return (_seconds % 86400L) / 3600; }
    int8_t minutes() const { return (_seconds % 3600) / 60; }
    int8_t seconds() const { return _seconds % 60; }
    
private:
    int32_t _seconds;
};

class DateTime {
public:
    DateTime(uint32_t t = 0);
    DateTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour = 0, uint8_t min = 0, uint8_t sec = 0);
    DateTime(const DateTime& copy);
    DateTime(const char* date, const char* time);
    
    uint16_t year() const { return 2000 + yOff; }
    uint8_t month() const { return m; }
    uint8_t day() const { return d; }
    uint8_t hour() const { return hh; }
    uint8_t minute() const { return mm; }
    uint8_t second() const { return ss; }
    
    uint8_t dayOfTheWeek() const;
    
    uint32_t unixtime(void) const;
    uint32_t secondstime(void) const;
    
    DateTime operator+(const TimeSpan& span);
    DateTime operator-(const TimeSpan& span);
    TimeSpan operator-(const DateTime& right);
    
protected:
    uint8_t yOff; ///< Year offset from 2000
    uint8_t m;    ///< Month 1-12
    uint8_t d;    ///< Day 1-31
    uint8_t hh;   ///< Hours 0-23
    uint8_t mm;   ///< Minutes 0-59
    uint8_t ss;   ///< Seconds 0-59
};

class RTC_DS3231 {
public:
    bool begin(i2c_port_t i2c_num = I2C_NUM_0);
    void adjust(const DateTime& dt);
    DateTime now();
    bool lostPower(void);
    
private:
    i2c_port_t _i2c_num;
    uint8_t read_i2c_register(uint8_t addr);
    void write_i2c_register(uint8_t addr, uint8_t data);
};

#endif // RTCLIB_H
