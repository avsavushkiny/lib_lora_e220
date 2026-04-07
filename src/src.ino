#include "E220.h"

// Подключение:
// E220 VCC -> 3.3V/5V
// E220 GND -> GND
// E220 RX  -> D4 (SoftwareSerial TX)
// E220 TX  -> D3 (SoftwareSerial RX)
// E220 M0  -> D7
// E220 M1  -> D6
// E220 AUX -> D5

#define PIN_M0  7
#define PIN_M1  6
#define PIN_AUX 5
#define PIN_RX  3 // Подключается к TX модуля
#define PIN_TX  4 // Подключается к RX модуля

E220 lora(PIN_M0, PIN_M1, PIN_AUX, PIN_RX, PIN_TX);

E220Config config;

void setup()
{
    Serial.begin(115200);
    Serial.println("Инициализация E220...");

    if (!lora.begin(9600))
    {
        Serial.println("Ошибка инициализации модуля!");
        while (1)
            ;
    }

    // Получаем текущую конфигурацию
    if (lora.getConfig(config))
    {
        Serial.println("Текущая конфигурация:");
        Serial.print("Адрес: 0x");
        Serial.println(config.address, HEX);
        Serial.print("Канал: ");
        Serial.println(config.channel);
    }

    // Настраиваем модуль (опционально)
    config.address = 0x1234;
    config.channel = 23;
    config.uartBaud = BAUD_9600;
    config.airRate = AIR_2_4K;
    config.power = POWER_22dBm;
    config.fixedTransmission = false;
    config.enableRSSI = false;
    config.enableLBT = false;

    if (lora.configure(config))
    {
        Serial.println("Конфигурация обновлена!");
    }

    Serial.println("Готов к работе!");
}

void loop()
{
    // Проверяем полученные данные
    if (lora.available())
    {
        String msg = lora.receiveString();
        Serial.print("Получено: ");
        Serial.println(msg);

        // // Отправляем ответ
        // lora.sendString(msg);
    }

    // Отправляем данные по Serial
    if (Serial.available())
    {
        String msg = Serial.readString();
        if (lora.sendString(msg))
        {
            Serial.println("Отправлено!");
        }
        else
        {
            Serial.println("Ошибка отправки!");
        }
    }
}