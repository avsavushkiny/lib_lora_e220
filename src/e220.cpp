#include "e220.h"

// Lookup tables
static const uint32_t baudRateTable[] = {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};
static const uint8_t maxChannel = 83; // E220 supports channels 0-83

//==============================================================================
// Constructor
//==============================================================================
EE220::EBYTE_E220(Stream *s, uint8_t pinM0, uint8_t pinM1, uint8_t pinAux)
    : _serial(s), _pinM0(pinM0), _pinM1(pinM1), _pinAux(pinAux), _save(0), _addressHigh(0), _addressLow(0), _reg0(0), _reg1(0), _channel(0), _reg3(0), _cryptHigh(0), _cryptLow(0), _uartBaudRate(UART_9600), _parityBit(PARITY_8N1), _airDataRate(AIR_2400), _subPacketSize(PKT_200_BYTES), _rssiAmbientNoiseEnable(false), _transmitPower(POWER_22dB), _rssiByteEnable(false), _transmissionMode(MODE_FIXED_DISABLE), _lbtEnable(false), _worTiming(WOR_500), _lastMode(MODE_NOT_SET), _lastError(ErrorCode::OK), _debugPrint(false), _lastRSSI(0), _lastReceiveRSSI(0), _newRSSIAvailable(false), _pinRecoverTime(PIN_RECOVER_DEFAULT), _auxTimeout(AUX_TIMEOUT_DEFAULT)
{
}

//==============================================================================
// Initialization
//==============================================================================
bool EE220::init(uint32_t baudRate)
{
    // Configure pins
    pinMode(_pinAux, INPUT_PULLUP);
    pinMode(_pinM0, OUTPUT);
    pinMode(_pinM1, OUTPUT);

    // Set to normal mode
    setMode(MODE_NORMAL);
    delay(_pinRecoverTime);

    // Read parameters
    bool success = readParameters();
    if (!success)
    {
        _lastError = ErrorCode::COMMUNICATION_FAILED;
        return false;
    }

    _lastError = ErrorCode::OK;
    return true;
}

//==============================================================================
// Mode control
//==============================================================================
void EE220::setMode(MODE_TYPE mode)
{
    // Small delay before changing mode
    delayMicrosecondsSafe(100);

    switch (mode)
    {
    case MODE_NORMAL:
        digitalWrite(_pinM0, LOW);
        digitalWrite(_pinM1, LOW);
        break;

    case MODE_WOR_TRANSMIT:
        digitalWrite(_pinM0, HIGH);
        digitalWrite(_pinM1, LOW);
        break;

    case MODE_WOR_RECEIVE:
        digitalWrite(_pinM0, LOW);
        digitalWrite(_pinM1, HIGH);
        break;

    case MODE_PROGRAM:
    case MODE_DEEP_SLEEP:
        digitalWrite(_pinM0, HIGH);
        digitalWrite(_pinM1, HIGH);
        break;

    default:
        return;
    }

    // Wait for mode to stabilize
    delayMicrosecondsSafe(_pinRecoverTime);

    // Clear buffer to avoid corruption
    clearBuffer();

    // Wait for AUX to be ready
    waitForAux(_auxTimeout);

    _lastMode = mode;
}

bool EE220::isAuxHigh()
{
    return digitalRead(_pinAux) == HIGH;
}

//==============================================================================
// Data transmission
//==============================================================================
bool EE220::sendByte(uint8_t data)
{
    _serial->write(data);
    return waitForAux(_auxTimeout);
}

int EE220::receiveByte()
{
    if (_serial->available())
    {
        return _serial->read();
    }
    return -1;
}

bool EE220::sendStruct(const void *structure, uint16_t size)
{
    if (!structure || size == 0)
    {
        _lastError = ErrorCode::INVALID_PARAM;
        return false;
    }

    // Debug output
    if (_debugPrint)
    {
        Serial.print(F("["));
        Serial.print(size);
        Serial.print(F("] "));

        const uint8_t *bytes = (const uint8_t *)structure;
        for (uint16_t i = 0; i < size; i++)
        {
            if (bytes[i] < 0x10)
                Serial.print(F("0"));
            Serial.print(bytes[i], HEX);
            Serial.print(F(" "));
        }
        Serial.println();
    }

    // Send data
    size_t written = _serial->write((const uint8_t *)structure, size);

    // Wait for transmission to complete
    bool success = waitForAux(_auxTimeout);

    if (written != size)
    {
        _lastError = ErrorCode::BUFFER_OVERFLOW;
        return false;
    }

    _lastError = ErrorCode::OK;
    return success;
}

bool EE220::receiveStruct(void *structure, uint16_t size)
{
    if (!structure || size == 0)
    {
        _lastError = ErrorCode::INVALID_PARAM;
        return false;
    }

    _newRSSIAvailable = false;

    // Read data
    size_t read = _serial->readBytes((uint8_t *)structure, size);

    // Check for RSSI byte if enabled
    if (_rssiByteEnable)
    {
        uint32_t startTime = millis();
        while ((millis() - startTime) < 5)
        {
            if (_serial->available())
            {
                _lastRSSI = _serial->read();
                _newRSSIAvailable = true;
                RSSIdata = _lastRSSI;
                newRSSIdataAvailable = true;
                break;
            }
        }
    }

    waitForAux(_auxTimeout);

    if (read != size)
    {
        _lastError = ErrorCode::COMMUNICATION_FAILED;
        return false;
    }

    _lastError = ErrorCode::OK;
    return true;
}

void EE220::enableDebugPrint(bool enable)
{
    _debugPrint = enable;
}

//==============================================================================
// RSSI functions
//==============================================================================
bool EE220::getRSSIValues()
{
    if (!_rssiAmbientNoiseEnable)
    {
        _lastError = ErrorCode::INVALID_PARAM;
        return false;
    }

    if (_lastMode != MODE_NORMAL && _lastMode != MODE_WOR_TRANSMIT)
    {
        _lastError = ErrorCode::INVALID_PARAM;
        return false;
    }

    uint8_t command[] = {0xC0, 0xC1, 0xC2, 0xC3, 0x00, 0x02};

    if (!sendStruct(command, sizeof(command)))
    {
        return false;
    }

    delay(50);

    uint8_t response[5];
    if (_serial->readBytes(response, 5) != 5)
    {
        _lastError = ErrorCode::COMMUNICATION_FAILED;
        return false;
    }

    _lastRSSI = response[3];
    _lastReceiveRSSI = response[4];
    RSSIdata = _lastRSSI;
    RSSIlastReceive = _lastReceiveRSSI;

    waitForAux(4000);

    _lastError = ErrorCode::OK;
    return true;
}

int16_t EE220::calculateRSSIInDBm(uint8_t rssiData)
{
    return -(256 - (int16_t)rssiData);
}

//==============================================================================
// Parameter setters
//==============================================================================
void EE220::setAddress(uint16_t address)
{
    _addressHigh = (address >> 8) & 0xFF;
    _addressLow = address & 0xFF;
}

void EE220::setAddressHigh(uint8_t value)
{
    _addressHigh = value;
}

void EE220::setAddressLow(uint8_t value)
{
    _addressLow = value;
}

void EE220::setChannel(uint8_t channel)
{
    if (channel > maxChannel)
    {
        _lastError = ErrorCode::INVALID_PARAM;
        return;
    }
    _channel = channel;
    _lastError = ErrorCode::OK;
}

void EE220::setUARTBaudRate(uint8_t rate)
{
    if (rate > 7)
    {
        _lastError = ErrorCode::INVALID_PARAM;
        return;
    }
    _uartBaudRate = rate;
    buildREG0();
    _lastError = ErrorCode::OK;
}

void EE220::setParityBit(uint8_t parity)
{
    if (parity > 2)
    {
        _lastError = ErrorCode::INVALID_PARAM;
        return;
    }
    _parityBit = parity;
    buildREG0();
    _lastError = ErrorCode::OK;
}

void EE220::setAirDataRate(uint8_t rate)
{
    _airDataRate = rate;
    buildREG0();
}

void EE220::setSubPacketSize(uint8_t size)
{
    _subPacketSize = size;
    buildREG1();
}

void EE220::setRSSIAmbientNoiseEnable(bool enable)
{
    _rssiAmbientNoiseEnable = enable;
    buildREG1();
}

void EE220::setTransmitPower(uint8_t power)
{
    _transmitPower = power;
    buildREG1();
}

void EE220::setRSSIByteEnable(bool enable)
{
    _rssiByteEnable = enable;
    buildREG3();
}

void EE220::setTransmissionMode(uint8_t mode)
{
    _transmissionMode = mode;
    buildREG3();
}

void EE220::setLBTEnable(bool enable)
{
    _lbtEnable = enable;
    buildREG3();
}

void EE220::setWORTiming(uint8_t timing)
{
    _worTiming = timing;
    buildREG3();
}

void EE220::setCrypt(uint16_t value)
{
    _cryptHigh = (value >> 8) & 0xFF;
    _cryptLow = value & 0xFF;

    setMode(MODE_PROGRAM);
    delay(5);

    _serial->write(CMD_WRITE_SAVE);
    _serial->write(0x06); // Starting address for crypt
    _serial->write(0x02); // Length
    _serial->write(_cryptHigh);
    _serial->write(_cryptLow);

    delay(50);
    _serial->readBytes((uint8_t *)&_cryptHigh, 5);

    waitForAux(4000);
    setMode(MODE_NORMAL);
}

void EE220::setDefaultParameters()
{
    setAddress(0);
    setUARTBaudRate(UART_9600);
    setParityBit(PARITY_8N1);
    setAirDataRate(AIR_2400);
    setSubPacketSize(PKT_200_BYTES);
    setRSSIAmbientNoiseEnable(false);
    setTransmitPower(POWER_22dB);
    setChannel(15);
    setRSSIByteEnable(false);
    setTransmissionMode(MODE_FIXED_DISABLE);
    setLBTEnable(false);
    setWORTiming(WOR_500);
    setCrypt(0);
    saveParameters(CMD_WRITE_SAVE);
}

//==============================================================================
// Parameter getters
//==============================================================================
uint16_t EE220::getAddress()
{
    return (_addressHigh << 8) | _addressLow;
}

//==============================================================================
// Save parameters
//==============================================================================
bool EE220::saveParameters(PROGRAM_COMMAND_TYPE command)
{
    Configuration config;
    config.command = command;
    config.startAddress = 0;
    config.length = 6;
    config.addH = _addressHigh;
    config.addL = _addressLow;
    config.reg0 = _reg0;
    config.reg1 = _reg1;
    config.channel = _channel;
    config.reg3 = _reg3;

    setMode(MODE_PROGRAM);
    delay(5);

    if (!sendStruct(&config, sizeof(config)))
    {
        _lastError = ErrorCode::COMMUNICATION_FAILED;
        setMode(MODE_NORMAL);
        return false;
    }

    // Wait for response
    uint32_t startTime = millis();
    while (_serial->available() == 0 && (millis() - startTime) < 5000)
    {
        delay(1);
    }

    if (!receiveStruct(&config, sizeof(config)))
    {
        _lastError = ErrorCode::COMMUNICATION_FAILED;
        setMode(MODE_NORMAL);
        return false;
    }

    waitForAux(4000);
    setMode(MODE_NORMAL);

    _lastError = ErrorCode::OK;
    return true;
}

//==============================================================================
// Utility functions
//==============================================================================
bool EE220::available()
{
    return _serial->available() > 0;
}

void EE220::flush()
{
    _serial->flush();
}

void EE220::printParameters()
{
    Serial.println(F("----------------------------------------"));
    Serial.print(F("Address High: 0x"));
    Serial.println(_addressHigh, HEX);
    Serial.print(F("Address Low:  0x"));
    Serial.println(_addressLow, HEX);
    Serial.print(F("Channel:      "));
    Serial.println(_channel);
    Serial.print(F("REG0:         0x"));
    Serial.println(_reg0, HEX);
    Serial.print(F("REG1:         0x"));
    Serial.println(_reg1, HEX);
    Serial.print(F("REG3:         0x"));
    Serial.println(_reg3, HEX);
    Serial.print(F("UART Baud:    "));
    Serial.println(baudRateTable[_uartBaudRate]);
    Serial.print(F("Air Data Rate:"));
    Serial.println(_airDataRate);
    Serial.print(F("Transmit Power:"));
    Serial.println(_transmitPower);
    Serial.println(F("----------------------------------------"));
}

//==============================================================================
// Private methods
//==============================================================================
bool EE220::readParameters()
{
    Configuration config;
    config.command = CMD_READ_CONFIG;
    config.startAddress = 0;
    config.length = 6;

    setMode(MODE_PROGRAM);

    if (!sendStruct(&config, 3))
    {
        setMode(MODE_NORMAL);
        return false;
    }

    delay(50);

    if (_serial->readBytes((uint8_t *)&config, sizeof(config)) != sizeof(config))
    {
        setMode(MODE_NORMAL);
        return false;
    }

    _save = config.command;
    _addressHigh = config.addH;
    _addressLow = config.addL;
    _reg0 = config.reg0;
    _reg1 = config.reg1;
    _channel = config.channel;
    _reg3 = config.reg3;

    // Parse registers
    _uartBaudRate = (_reg0 & 0b11100000) >> 5;
    _parityBit = (_reg0 & 0b00011000) >> 3;
    _airDataRate = (_reg0 & 0b00000111);

    _subPacketSize = (_reg1 & 0b11000000) >> 6;
    _rssiAmbientNoiseEnable = (_reg1 & 0b00100000) >> 5;
    _transmitPower = (_reg1 & 0b00000011);

    _rssiByteEnable = (_reg3 & 0b10000000) >> 7;
    _transmissionMode = (_reg3 & 0b01000000) >> 6;
    _lbtEnable = (_reg3 & 0b00010000) >> 4;
    _worTiming = (_reg3 & 0b00000111);

    setMode(MODE_NORMAL);

    return _save == CMD_RETURNED;
}

void EE220::buildREG0()
{
    _reg0 = ((_uartBaudRate & 0b111) << 5) |
            ((_parityBit & 0b11) << 3) |
            (_airDataRate & 0b111);
}

void EE220::buildREG1()
{
    _reg1 = ((_subPacketSize & 0b11) << 6) |
            ((_rssiAmbientNoiseEnable ? 1 : 0) << 5) |
            (_transmitPower & 0b11);
}

void EE220::buildREG3()
{
    _reg3 = ((_rssiByteEnable ? 1 : 0) << 7) |
            ((_transmissionMode & 0b1) << 6) |
            ((_lbtEnable ? 1 : 0) << 4) |
            (_worTiming & 0b111);
}

void EE220::clearBuffer()
{
    uint32_t startTime = millis();
    while (_serial->available())
    {
        _serial->read();
        if (millis() - startTime > 500)
        {
            if (_debugPrint)
            {
                Serial.println(F("ClearBuffer timeout"));
            }
            break;
        }
    }
}

bool EE220::waitForAux(uint32_t timeoutMs)
{
    if (_pinAux == 255)
    { // AUX not connected
        delay(COMPLETE_TASK_DELAY);
        return true;
    }

    uint32_t startTime = millis();
    while (digitalRead(_pinAux) == LOW)
    {
        if (millis() - startTime > timeoutMs)
        {
            _lastError = ErrorCode::TIMEOUT;
            return false;
        }
        delay(1);
    }

    delay(COMPLETE_TASK_DELAY);
    return true;
}

void EE220::delayMicrosecondsSafe(uint32_t us)
{
    if (us < 20000)
    {
        delayMicroseconds(us);
    }
    else
    {
        delay(us / 1000);
    }
}