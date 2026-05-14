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

// Для таймера
unsigned long previousTime = 0;
const long interval = 10000; // 10

// ---------- ROVER functions ----------

const int in2 = 10; // m1
const int in1 = 9;  // m1

const int in4 = 12; // m2
const int in3 = 11; // m2

void setupPinRover()
{
    pinMode(in1, 0x1); pinMode(in2, 0x1); // m1
    pinMode(in3, 0x1); pinMode(in4, 0x1); // m2
}

void testMotorRover()
{
    digitalWrite(in1, 0x1); digitalWrite(in2, 0x0); // backward m1
    digitalWrite(in4, 0x1); digitalWrite(in3, 0x0); // backward m2
    delay(5000);
    
    digitalWrite(in1, 0x0); digitalWrite(in2, 0x1); // forward m1
    digitalWrite(in4, 0x0); digitalWrite(in3, 0x1); // forward m2
    delay(5000);
}

void forwardRover()
{
    digitalWrite(in1, 0x0); digitalWrite(in2, 0x1); // forward m1
    digitalWrite(in4, 0x0); digitalWrite(in3, 0x1); // forward m2
}

void backwardRover()
{
    digitalWrite(in1, 0x1); digitalWrite(in2, 0x0); // backward m1
    digitalWrite(in4, 0x1); digitalWrite(in3, 0x0); // backward m2
}

void leftRover()
{

}

void rightRover()
{

}

void stopRover()
{
    digitalWrite(in1, 0x0); digitalWrite(in2, 0x0); // backward m1
    digitalWrite(in4, 0x0); digitalWrite(in3, 0x0); // backward m2
}
// ---------- END ROVER functions ---------

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
    config.address = 0x1234; // Адрес
    config.channel = 23;     // Канал
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

    // --------- rover ---------
    setupPinRover();
    // testMotorRover();
}

void loop()
{
    // // Отправляем SAY HELLO каждые 10 секунд
    // unsigned long currentTime = millis();

    // if (currentTime - previousTime >= interval) {
    //     sendSayHello();
    //     previousTime = currentTime;
    // }
    
    // Проверяем полученные данные
    if (lora.available())
    {
        String msg = lora.receiveString();
        Serial.print("R: ");
        Serial.println(msg);

        // // Отправляем ответ
        // lora.sendString(msg);

        if (msg == "W\n")
        {
            forwardRover(); delay(1000); msg = ""; stopRover();
        }
        if (msg == "S\n")
        {
            backwardRover(); delay(1000); msg = ""; stopRover();
        }
    }

    // Отправляем данные по Serial
    if (Serial.available())
    {
        String msg = Serial.readString();
        if (lora.sendString(msg))
        {
            // Serial.println("SEND");
        }
        else
        {
            // Serial.println("ERROR");
        }
    }
}

bool sendSayHello()
{
    lora.sendStringWithAddress("Hello", 0x1234, 23);
}

