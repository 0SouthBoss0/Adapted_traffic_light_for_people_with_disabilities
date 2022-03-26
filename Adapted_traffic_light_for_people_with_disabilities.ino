#include <Wire.h>
#include <SPI.h>

// библиотека для работы с RFID/NFC
#include <Adafruit_PN532.h>

// Порт 8, модуль NFC
#define PN532_IRQ   8


// создаём объект для работы со сканером и передаём ему два параметра
// первый — номер пина прерывания
// вторым — число 100
// от Adafruit был программный сброс шилда
// в cканере RFID/NFC 13,56 МГц (Troyka-модуль) этот пин не используется
// поэтому передаём цифру, большая чем любой пин Arduino
Adafruit_PN532 nfc(PN532_IRQ, 100);


const int TIMEOUT_RED = 3000;          // Время горения красного сигнала
const int TIMEOUT_YEL = 1690;          // Время горения желтого сигнала
const int TIMEOUT_GREEN = 2000;        // Время горения зеленого сигнала

const int TIMEOUT_FLASH_GREEN = 500;   // Время мигания зеленого сигнала

const int LED_RED = 9;                 // Порт 9, красный автомобильный сигнал
const int LED_YELLOW = 10;             // Порт 10, желтый автомобильный сигнал
const int LED_GREEN = 11;              // Порт 11, зеленый автомобильный сигнал

const int PED_R = 12;                  // Порт 12, красный пешеходный сигнал
const int PED_G = 7;                   // Порт 7, зеленый пешеходный сигнал

const int BUTTON = 6;                  // Порт 6, кнопка

int gd = 50;                           // Частота обновления 50 мс = 0,05 с

int prelude_sec = 6;                   // Длительность процесса переключения статуса светофора
int cooldown_sec = 4;                  // Задержка для исключения задержки трафика

// Инициализация переменных, необходимых при работе программы
int greendelay = 0;
int cooldown = 0;
int prelude_delay = 0;
int green_sec = 0;

bool prelude_init = false;
bool green_init = false;
bool ped_green_on = false;

int gg = 1;



// Первичная настройка
void setup(void) {
  // установка пинов автомобильного светофора на вывод
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);

  // установка пинов пешеходного светофора на вывод
  pinMode(PED_R, OUTPUT);
  pinMode(PED_G, OUTPUT);

  // установка пина кнопки на ввод
  pinMode(BUTTON, INPUT);

  // установка красного сигнала на пешеходный светофор
  digitalWrite(PED_R, 1);

  // инициализация RFID/NFC сканера
  nfc.begin();
  nfc.SAMConfig();

  // установка числа попыток проверки активации карты
  nfc.setPassiveActivationRetries(1);
}

// Функция для установки значений сигналов пешеходного светофора
void pedestrianLight(bool pg, bool pr) {
  digitalWrite(PED_G, pg);
  digitalWrite(PED_R, pr);
}

// Функция для установки значений сигналов автомобильного светофора
void autoLight(bool r, bool g, bool y) {
  digitalWrite(LED_RED, r);
  digitalWrite(LED_YELLOW, y);
  digitalWrite(LED_GREEN, g);
}

// Функция перевода секунд в миллисекунды в зависимости от частоты обновления
int sdelay(int sec) {
  return (1000 / gd) * sec;
}

// Повторяющаяся функция
void loop(void) {

  // таймер для переменной greendelay
  greendelay--;
  if (greendelay < 0) {
    greendelay = 0;
  }

  // таймер для переменной cooldown
  cooldown--;
  if (cooldown < 0) {
    cooldown = 0;
  }

  //таймер для переменнной prelude_delay
  prelude_delay--;
  if (prelude_delay < 0) {
    prelude_delay = 0;
  }

  uint8_t success;   // буфер для хранения ID карты
  uint8_t uid[8];   // размер буфера карты
  uint8_t uidLength;   // слушаем новые метки

  //попытка считывания NFC
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);



  // активирован NFC
  if (success and cooldown == 0 and !prelude_init) {
    prelude_init = true;
    prelude_delay = sdelay(prelude_sec) - 1;
    green_sec = 10;
    cooldown = sdelay(green_sec) + sdelay(cooldown_sec) + sdelay(prelude_sec);
  }

  // активирована кнопка
  if (digitalRead(BUTTON) and cooldown == 0 and !prelude_init) {
    prelude_init = true;
    prelude_delay = sdelay(prelude_sec) - 1;
    green_sec = 5;
    cooldown = sdelay(green_sec) + sdelay(cooldown_sec) + sdelay(prelude_sec); // 4 sec cooldown
  }

  // Конец прелюдии
  if (prelude_init and prelude_delay == 0) {
    greendelay = sdelay(green_sec);
    prelude_init = false;
  }

  // Включение зелёного пешеходного сигнала
  if (greendelay >= sdelay(3)) {
    ped_green_on = 1;
  }

  // Мигание зеленого пешеходного за 3 с
  if (greendelay > 0 and greendelay < sdelay(3)) {
    ped_green_on = (greendelay / (sdelay(3) / 6)) % 2;
  }

  // Включение жёлтого автомобильного сигнала
  int yy = 1 - (prelude_delay / (sdelay(prelude_sec) / 2));
  if (prelude_delay == 0) {
    yy = 0;
  }

  // Включение зелёного автомобильного сигнала
  if (prelude_init and prelude_delay != 0 and greendelay == 0) {
    gg = (prelude_delay / (sdelay(prelude_sec) / 2)) * ((prelude_delay % (sdelay(prelude_sec) / 2)) / 8) % 2;
  }
  else {
    if (greendelay > 0) {
      gg = 0;
    }
    else {
      gg = 1;
    }

  }

  // Включаем нужный режим горения автомобильного и пешеходного светофора
  autoLight(greendelay > 0, gg, yy);
  pedestrianLight(ped_green_on, not ped_green_on and greendelay == 0);

  //частота обновления
  delay(gd);
}