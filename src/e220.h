#pragma once

#include "Arduino.h"
#include <Stream.h>

// Timing constants (in milliseconds)
#define PIN_RECOVER_MIN 10
#define PIN_RECOVER_DEFAULT 15
#define AUX_TIMEOUT_DEFAULT 1000
#define COMPLETE_TASK_DELAY 20

// Modes
enum MODE_TYPE : uint8_t
{
    MODE_NORMAL = 0,
    MODE_WOR_TRANSMIT = 1,
    MODE_WOR_RECEIVE = 2,
    MODE_PROGRAM = 3,
    MODE_DEEP_SLEEP = 4,
    MODE_NOT_SET = 0xFF
};

// Program commands
enum PROGRAM_COMMAND_TYPE : uint8_t
{
    CMD_WRITE_SAVE = 0xC0,
    CMD_READ_CONFIG = 0xC1,
    CMD_WRITE_TEMP = 0xC2,
    CMD_RETURNED = 0xC1
};

// REG0 - UART Baud Rates
enum UART_BAUD_RATE : uint8_t
{
    UART_1200 = 0b000,
    UART_2400 = 0b001,
    UART_4800 = 0b010,
    UART_9600 = 0b011,
    UART_19200 = 0b100,
    UART_38400 = 0b101,
    UART_57600 = 0b110,
    UART_115200 = 0b111
};

// REG0 - Parity Bits
enum PARITY_BIT : uint8_t
{
    PARITY_8N1 = 0b00,
    PARITY_8O1 = 0b01,
    PARITY_8E1 = 0b10
};

// REG0 - Air Data Rates
enum AIR_DATA_RATE : uint8_t
{
    AIR_2400 = 0b010,
    AIR_4800 = 0b011,
    AIR_9600 = 0b100,
    AIR_19200 = 0b101,
    AIR_38400 = 0b110,
    AIR_62500 = 0b111
};

// REG1 - Sub Packet Sizes
enum SUB_PACKET_SIZE : uint8_t
{
    PKT_200_BYTES = 0b00,
    PKT_128_BYTES = 0b01,
    PKT_64_BYTES = 0b10,
    PKT_32_BYTES = 0b11
};

// REG1 - Transmit Power (22dB module)
enum TRANSMIT_POWER : uint8_t
{
    POWER_22dB = 0b00,
    POWER_17dB = 0b01,
    POWER_13dB = 0b10,
    POWER_10dB = 0b11
};

// REG3 - RSSI Byte Enable
enum RSSI_BYTE_MODE : uint8_t
{
    RSSI_BYTE_ENABLE = 0b0,
    RSSI_BYTE_DISABLE = 0b1
};

// REG3 - Transmission Mode
enum TRANSMISSION_MODE : uint8_t
{
    MODE_FIXED_DISABLE = 0b0,
    MODE_FIXED_ENABLE = 0b1
};

// REG3 - LBT Enable
enum LBT_MODE : uint8_t
{
    LBT_DISABLE = 0b0,
    LBT_ENABLE = 0b1
};

// REG3 - WOR Timing (ms)
enum WOR_TIMING : uint8_t
{
    WOR_500 = 0b000,
    WOR_1000 = 0b001,
    WOR_1500 = 0b010,
    WOR_2000 = 0b011,
    WOR_2500 = 0b100,
    WOR_3000 = 0b101,
    WOR_3500 = 0b110,
    WOR_4000 = 0b111
};

// Error codes
enum class ErrorCode : uint8_t
{
    OK = 0,
    TIMEOUT,
    INVALID_PARAM,
    COMMUNICATION_FAILED,
    BUFFER_OVERFLOW
};

class EE220
{
public:
    // Constructor
    EE220(Stream *s, uint8_t pinM0 = 4, uint8_t pinM1 = 5, uint8_t pinAux = 6);

    // Initialization
    bool init(uint32_t baudRate = 9600);

    // Mode control
    void setMode(MODE_TYPE mode = MODE_NORMAL);
    bool isAuxHigh();

    // Data transmission
    bool sendByte(uint8_t data);
    int receiveByte();
    bool sendStruct(const void *structure, uint16_t size);
    bool receiveStruct(void *structure, uint16_t size);
    void enableDebugPrint(bool enable);

    // Data availability
    bool available();
    void flush();

    // RSSI
    bool getRSSIValues();
    int16_t calculateRSSIInDBm(uint8_t rssiData);
    uint8_t getLastRSSI() const { return _lastRSSI; }
    uint8_t getLastReceiveRSSI() const { return _lastReceiveRSSI; }
    bool isNewRSSIAvailable() const { return _newRSSIAvailable; }

    // Parameter setters
    void setAddress(uint16_t address);
    void setAddressHigh(uint8_t value);
    void setAddressLow(uint8_t value);
    void setChannel(uint8_t channel);
    void setUARTBaudRate(uint8_t rate);
    void setParityBit(uint8_t parity);
    void setAirDataRate(uint8_t rate);
    void setSubPacketSize(uint8_t size);
    void setRSSIAmbientNoiseEnable(bool enable);
    void setTransmitPower(uint8_t power);
    void setRSSIByteEnable(bool enable);
    void setTransmissionMode(uint8_t mode);
    void setLBTEnable(bool enable);
    void setWORTiming(uint8_t timing);
    void setCrypt(uint16_t value);
    void setDefaultParameters();

    // Parameter getters
    uint16_t getAddress();
    uint8_t getAddressHigh() const { return _addressHigh; }
    uint8_t getAddressLow() const { return _addressLow; }
    uint8_t getChannel() const { return _channel; }
    uint8_t getUARTBaudRate() const { return _uartBaudRate; }
    uint8_t getParityBit() const { return _parityBit; }
    uint8_t getAirDataRate() const { return _airDataRate; }
    uint8_t getSubPacketSize() const { return _subPacketSize; }
    bool getRSSIAmbientNoiseEnable() const { return _rssiAmbientNoiseEnable; }
    uint8_t getTransmitPower() const { return _transmitPower; }
    bool getRSSIByteEnable() const { return _rssiByteEnable; }
    uint8_t getTransmissionMode() const { return _transmissionMode; }
    bool getLBTEnable() const { return _lbtEnable; }
    uint8_t getWORTiming() const { return _worTiming; }

    // Save parameters
    bool saveParameters(PROGRAM_COMMAND_TYPE command = CMD_WRITE_SAVE);

    // Utility
    void printParameters();
    ErrorCode getLastError() const { return _lastError; }

    // Public data members (for backward compatibility)
    uint8_t RSSIdata = 0;
    uint8_t RSSIlastReceive = 0;
    bool newRSSIdataAvailable = false;

private:
// Configuration structure (packed)
#pragma pack(push, 1)
    struct Configuration
    {
        uint8_t command;
        uint8_t startAddress;
        uint8_t length;
        uint8_t addH;
        uint8_t addL;
        uint8_t reg0;
        uint8_t reg1;
        uint8_t channel;
        uint8_t reg3;
    };
#pragma pack(pop)

    // Private methods
    bool readParameters();
    void buildREG0();
    void buildREG1();
    void buildREG3();
    void clearBuffer();
    bool waitForAux(uint32_t timeoutMs);
    void delayMicrosecondsSafe(uint32_t us);

    // Hardware pins
    Stream *_serial;
    uint8_t _pinM0;
    uint8_t _pinM1;
    uint8_t _pinAux;

    // Configuration registers
    uint8_t _save;
    uint8_t _addressHigh;
    uint8_t _addressLow;
    uint8_t _reg0;
    uint8_t _reg1;
    uint8_t _channel;
    uint8_t _reg3;
    uint8_t _cryptHigh;
    uint8_t _cryptLow;

    // Parsed parameters
    uint8_t _uartBaudRate;
    uint8_t _parityBit;
    uint8_t _airDataRate;
    uint8_t _subPacketSize;
    bool _rssiAmbientNoiseEnable;
    uint8_t _transmitPower;
    bool _rssiByteEnable;
    uint8_t _transmissionMode;
    bool _lbtEnable;
    uint8_t _worTiming;

    // State
    MODE_TYPE _lastMode;
    ErrorCode _lastError;
    bool _debugPrint;
    uint8_t _lastRSSI;
    uint8_t _lastReceiveRSSI;
    bool _newRSSIAvailable;

    // Timing
    uint32_t _pinRecoverTime;
    uint32_t _auxTimeout;
};