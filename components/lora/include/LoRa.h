#ifndef LORA_H
#define LORA_H

#include <stdint.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"

class LoRaClass {
public:
    LoRaClass();
    
    int begin(long frequency);
    void end();
    
    int beginPacket(int implicitHeader = false);
    int endPacket(bool async = false);
    
    int parsePacket(int size = 0);
    int packetRssi();
    float packetSnr();
    long packetFrequencyError();
    
    size_t write(uint8_t byte);
    size_t write(const uint8_t *buffer, size_t size);
    
    int available();
    int read();
    
    void idle();
    void sleep();
    
    void setTxPower(int level, int outputPin = 0);
    void setFrequency(long frequency);
    void setSpreadingFactor(int sf);
    void setSignalBandwidth(long sbw);
    void setCodingRate4(int denominator);
    void setPreambleLength(long length);
    void setSyncWord(int sw);
    void enableCrc();
    void disableCrc();
    void enableInvertIQ();
    void disableInvertIQ();
    
    void setOCP(uint8_t mA); // Over Current Protection control
    
    void setGain(uint8_t gain); // Set LNA gain
    
    // deprecated
    void crc() { enableCrc(); }
    void noCrc() { disableCrc(); }
    
    byte random();
    
    void setPins(int ss = -1, int reset = -1, int dio0 = -1);
    void setSPI(spi_device_handle_t spi);
    void setSPIFrequency(uint32_t frequency);
    
    void dumpRegisters();
    
    void onReceive(void(*callback)(int));
    void onTxDone(void(*callback)());
    
    void receive(int size = 0);
    void receivedCallback(void);
    
private:
    void explicitHeaderMode();
    void implicitHeaderMode();
    
    void handleDio0Rise();
    bool isTransmitting();
    
    int getSpreadingFactor();
    long getSignalBandwidth();
    
    void setLdoFlag();
    
    uint8_t readRegister(uint8_t address);
    void writeRegister(uint8_t address, uint8_t value);
    uint8_t singleTransfer(uint8_t address, uint8_t value);
    
    static void onDio0Rise();
    
private:
    spi_device_handle_t _spi;
    int _ss;
    int _reset;
    int _dio0;
    long _frequency;
    int _packetIndex;
    int _implicitHeaderMode;
    void (*_onReceive)(int);
    void (*_onTxDone)();
};

extern LoRaClass LoRa;

#endif
