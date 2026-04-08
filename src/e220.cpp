#include "E220.h"
#include <Arduino.h>

E220::E220(uint8_t m0Pin, uint8_t m1Pin, uint8_t auxPin, uint8_t rxPin, uint8_t txPin)
{
    _m0Pin = m0Pin;
    _m1Pin = m1Pin;
    _auxPin = auxPin;
    _serial = new SoftwareSerial(rxPin, txPin);
    _currentMode = MODE_NORMAL;
    _lastRSSI = 0;
}

bool E220::begin(uint32_t baudRate)
{
    pinMode(_m0Pin, OUTPUT);
    pinMode(_m1Pin, OUTPUT);
    pinMode(_auxPin, INPUT_PULLUP);

    _serial->begin(baudRate);

    // Сброс в нормальный режим
    setMode(MODE_NORMAL);
    delay(100);

    // Ждём готовности модуля
    for (int i = 0; i < 50; i++)
    {
        if (isReady())
        {
            return true;
        }
        delay(50);
    }
    return false;
}

void E220::setModePins(E220Mode mode)
{
    digitalWrite(_m0Pin, (mode & 0x01) ? HIGH : LOW);
    digitalWrite(_m1Pin, (mode & 0x02) ? HIGH : LOW);
}

void E220::setMode(E220Mode mode)
{
    if (_currentMode == mode)
        return;

    setModePins(mode);
    delay(20); // Время на переключение режима

    // Если выходим из конфигурационного режима, нужно больше времени
    if (_currentMode == MODE_CONFIG)
    {
        waitForAUX();
    }

    _currentMode = mode;
}

E220Mode E220::getMode()
{
    return _currentMode;
}

bool E220::isReady()
{
    return digitalRead(_auxPin) == HIGH;
}

void E220::waitForAUX()
{
    unsigned long start = millis();
    while (digitalRead(_auxPin) == LOW)
    {
        if (millis() - start > 2000)
        {
            break; // Таймаут 2 секунды
        }
        delay(1);
    }
}

bool E220::sendData(const uint8_t *data, size_t len)
{
    if (_currentMode != MODE_NORMAL && _currentMode != MODE_WOR_TX)
    {
        setMode(MODE_NORMAL);
        waitForAUX();
    }

    waitForAUX();

    size_t sent = _serial->write(data, len);
    _serial->flush();

    // Ждём завершения передачи
    waitForAUX();

    return sent == len;
}

bool E220::sendString(const String &message)
{
    return sendData((const uint8_t *)message.c_str(), message.length());
}

bool E220::sendStringWithAddress(const String &message, uint16_t destAddress, uint8_t destChannel)
{
    // Формируем префикс: [DD DD CC] где DD - адрес (2 байта), CC - канал (1 байт)
    uint8_t prefix[3];
    prefix[0] = (destAddress >> 8) & 0xFF;  // Старший байт адреса
    prefix[1] = destAddress & 0xFF;         // Младший байт адреса
    prefix[2] = destChannel;                // Канал

    // Отправляем префикс
    if (!sendData(prefix, 3)) {
        return false;
    }
    
    // Отправляем сообщение SAY
    String sayMessage = "SAY " + message;
    return sendString(sayMessage);
}

int E220::available()
{
    if (_currentMode != MODE_NORMAL && _currentMode != MODE_WOR_RX)
    {
        return 0;
    }
    return _serial->available();
}

size_t E220::receiveData(uint8_t *buffer, size_t maxLen)
{
    size_t received = 0;
    unsigned long start = millis();

    while (received < maxLen && (millis() - start) < 100)
    {
        if (_serial->available())
        {
            buffer[received++] = _serial->read();
            start = millis();
        }
    }

    return received;
}

String E220::receiveString()
{
    String result = "";
    unsigned long start = millis();

    while ((millis() - start) < 100)
    {
        if (_serial->available())
        {
            result += (char)_serial->read();
            start = millis();
        }
    }

    return result;
}

int8_t E220::getLastRSSI()
{
    return _lastRSSI;
}

uint8_t E220::calculateCRC(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i++)
    {
        crc ^= data[i];
    }
    return crc;
}

bool E220::sendCommand(const uint8_t *cmd, uint8_t cmdLen, uint8_t *response, uint8_t respLen)
{
    E220Mode previousMode = _currentMode;

    // Переключаемся в режим конфигурации
    if (_currentMode != MODE_CONFIG)
    {
        setMode(MODE_CONFIG);
        waitForAUX();
    }

    // Очищаем буфер
    while (_serial->available())
    {
        _serial->read();
    }

    // Отправляем команду
    _serial->write(cmd, cmdLen);
    _serial->flush();

    // Ждём ответ
    delay(50);

    // Читаем ответ
    uint8_t received = 0;
    unsigned long start = millis();

    while (received < respLen && (millis() - start) < 500)
    {
        if (_serial->available())
        {
            response[received++] = _serial->read();
        }
    }

    // Возвращаемся в предыдущий режим
    if (previousMode != MODE_CONFIG)
    {
        setMode(previousMode);
    }

    return received == respLen && response[0] == 0xC1;
}

bool E220::configure(const E220Config &config)
{
    // Команда: C0 + начальный адрес(00) + длина(09) + параметры
    uint8_t cmd[13] = {
        0xC0, 0x00, 0x09,                                           // Заголовок команды
        (uint8_t)(config.address >> 8),                             // ADDH
        (uint8_t)(config.address & 0xFF),                           // ADDL
        config.channel,                                             // CHAN
        (uint8_t)((config.uartBaud << 2) | (0 << 1) | 0),           // SPED (UART)
        (uint8_t)((config.airRate << 5) | (config.power << 2) | 0), // OPTION
        (uint8_t)((config.fixedTransmission ? 0x01 : 0x00) |
                  (config.enableRSSI ? 0x02 : 0x00) |
                  (config.enableLBT ? 0x10 : 0x00)), // TRANSMISSION_MODE
        0x00, 0x00, 0x00, 0x00                       // Криптография (не используется)
    };

    uint8_t response[13];
    return sendCommand(cmd, 13, response, 13);
}

bool E220::getConfig(E220Config &config)
{
    uint8_t cmd[] = {0xC1, 0x00, 0x09};
    uint8_t response[13];

    if (!sendCommand(cmd, 3, response, 13))
    {
        return false;
    }

    config.address = (response[4] << 8) | response[5];
    config.channel = response[6];
    config.uartBaud = (UARTBaudRate)((response[7] >> 2) & 0x07);
    config.airRate = (AirDataRate)((response[8] >> 5) & 0x07);
    config.power = (TransmitPower)((response[8] >> 2) & 0x03);
    config.fixedTransmission = (response[9] & 0x01) != 0;
    config.enableRSSI = (response[9] & 0x02) != 0;
    config.enableLBT = (response[9] & 0x10) != 0;

    return true;
}

void E220::reset()
{
    setMode(MODE_CONFIG);
    waitForAUX();

    uint8_t cmd[] = {0xC4, 0x00, 0x01}; // Команда сброса
    _serial->write(cmd, 3);
    _serial->flush();

    delay(200);
    setMode(MODE_NORMAL);
}

uint32_t E220::getVersion()
{
    uint8_t cmd[] = {0xC3, 0x00, 0x01};
    uint8_t response[6];

    if (!sendCommand(cmd, 3, response, 6))
    {
        return 0;
    }

    return (response[4] << 8) | response[5];
}