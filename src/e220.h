#ifndef E220_H
#define E220_H

#include <SoftwareSerial.h>

// Режимы работы модуля
enum E220Mode
{
    MODE_NORMAL = 0b00, // Режим 0: прозрачная передача
    MODE_WOR_TX = 0b01, // Режим 1: WOR передатчик
    MODE_WOR_RX = 0b10, // Режим 2: WOR приёмник
    MODE_CONFIG = 0b11  // Режим 3: конфигурация (спящий режим)
};

// Скорость UART
enum UARTBaudRate
{
    BAUD_1200 = 0b000,
    BAUD_2400 = 0b001,
    BAUD_4800 = 0b010,
    BAUD_9600 = 0b011,
    BAUD_19200 = 0b100,
    BAUD_38400 = 0b101,
    BAUD_57600 = 0b110,
    BAUD_115200 = 0b111
};

// Скорость эфира
enum AirDataRate
{
    AIR_2_4K = 0b000,
    AIR_4_8K = 0b001,
    AIR_9_6K = 0b010,
    AIR_19_2K = 0b011,
    AIR_38_4K = 0b100,
    AIR_62_5K = 0b101
};

// Мощность передатчика (для E220-900T22D)
enum TransmitPower
{
    POWER_22dBm = 0b00,
    POWER_17dBm = 0b01,
    POWER_13dBm = 0b10,
    POWER_10dBm = 0b11
};

// Структура конфигурации модуля
struct E220Config
{
    uint16_t address;       // Адрес модуля (0-65535)
    uint8_t channel;        // Канал (0-80)
    UARTBaudRate uartBaud;  // Скорость UART
    AirDataRate airRate;    // Скорость эфира
    TransmitPower power;    // Мощность передатчика
    bool fixedTransmission; // Фиксированная передача (true = адресная)
    bool enableRSSI;        // Добавлять RSSI к данным
    bool enableLBT;         // Слушать перед передачей
};

class E220
{
public:
    // Конструктор: пины M0, M1, AUX и SoftwareSerial
    E220(uint8_t m0Pin, uint8_t m1Pin, uint8_t auxPin,
         uint8_t rxPin, uint8_t txPin);

    // Инициализация модуля
    bool begin(uint32_t baudRate = 9600);

    // Установка режима работы
    void setMode(E220Mode mode);

    // Получение текущего режима
    E220Mode getMode();

    // Проверка готовности модуля (AUX)
    bool isReady();

    // Отправка данных
    bool sendData(const uint8_t *data, size_t len);
    bool sendString(const String &message);
    bool sendStringWithAddress(const String &message, uint16_t destAddress, uint8_t destChannel);

    // Приём данных
    int available();
    size_t receiveData(uint8_t *buffer, size_t maxLen);
    String receiveString();

    // Получение RSSI последнего пакета
    int8_t getLastRSSI();

    // Настройка модуля (должно быть в режиме CONFIG)
    bool configure(const E220Config &config);
    bool getConfig(E220Config &config);

    // Сброс модуля
    void reset();

    // Чтение версии модуля
    uint32_t getVersion();

private:
    uint8_t _m0Pin, _m1Pin, _auxPin;
    SoftwareSerial *_serial;
    E220Mode _currentMode;
    int8_t _lastRSSI;

    void setModePins(E220Mode mode);
    void waitForAUX();
    bool sendCommand(const uint8_t *cmd, uint8_t cmdLen, uint8_t *response, uint8_t respLen);
    uint8_t calculateCRC(const uint8_t *data, uint8_t len);
};

#endif