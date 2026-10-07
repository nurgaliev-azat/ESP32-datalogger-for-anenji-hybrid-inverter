#ifndef ANENJI_REGISTERS_H
#define ANENJI_REGISTERS_H

#include <Arduino.h>

enum RegisterType {
  REG_SHORT,
  REG_USHORT,
  REG_FLAGS32,
  REG_ASCII
};

struct RegisterDefinition {
  uint16_t address;
  const char* name;
  const char* title;
  RegisterType type;
  float scale;
  const char* unit;
};

const RegisterDefinition ANENJI_REGISTERS[] = {
  {100, "Fault Code",          "Код аварии",            REG_FLAGS32, 1.0f,  ""},
  {108, "Warning Code",        "Код предупреждения",    REG_FLAGS32, 1.0f,  ""},
  {201, "Working Mode",        "Режим работы",          REG_USHORT,  1.0f,  ""},
  {202, "Grid Voltage",        "Напряжение сети",       REG_SHORT,   0.1f,  "В"},
  {203, "Grid Frequency",      "Частота сети",          REG_SHORT,   0.01f, "Гц"},
  {204, "Grid Power",          "Мощность сети",         REG_SHORT,   1.0f,  "Вт"},
  {205, "Inv Voltage",         "Напряжение инвертора",  REG_SHORT,   0.1f,  "В"},
  {206, "Inv Current",         "Ток инвертора",         REG_SHORT,   0.1f,  "А"},
  {207, "Inv Frequency",       "Частота инвертора",     REG_SHORT,   0.01f, "Гц"},
  {208, "Inv Power",           "Мощность инвертора",    REG_SHORT,   1.0f,  "Вт"},
  {209, "Charge Power",        "Мощность заряда",       REG_SHORT,   1.0f,  "Вт"},
  {210, "Load Voltage",        "Напряжение нагрузки",   REG_SHORT,   0.1f,  "В"},
  {211, "Load Current",        "Ток нагрузки",          REG_SHORT,   0.1f,  "А"},
  {212, "Load Frequency",      "Частота нагрузки",      REG_SHORT,   0.01f, "Гц"},
  {213, "Load Power",          "Мощность нагрузки",     REG_SHORT,   1.0f,  "Вт"},
  {214, "Apparent Power",      "Полная мощность",       REG_SHORT,   1.0f,  "ВА"},
  {215, "Battery Voltage",     "Напряжение батареи",    REG_SHORT,   0.1f,  "В"},
  {216, "Battery Current",     "Ток батареи",           REG_SHORT,   0.1f,  "А"},
  {217, "Battery Power",       "Мощность батареи",      REG_SHORT,   1.0f,  "Вт"},
  {219, "PV Voltage",          "Напряжение PV",         REG_SHORT,   0.1f,  "В"},
  {220, "PV Current",         "Ток PV",                 REG_SHORT,   0.1f,  "А"},
  {223, "PV Power",           "Мощность PV",            REG_SHORT,   1.0f,  "Вт"},
  {224, "PV Charge Power",    "Мощность заряда PV",     REG_SHORT,   1.0f,  "Вт"},
  {225, "Load Percent",       "Загрузка",              REG_SHORT,   0.01f, "%"},
  {226, "DC-DC Temperature",  "Температура DCDC",      REG_SHORT,   1.0f,  "°C"},
  {227, "Inv Temperature",    "Температура инвертора",  REG_SHORT,   1.0f,  "°C"},
  {229, "Battery Percent",    "Заряд батареи",         REG_USHORT,  1.0f,  "%"},
  {232, "Battery Current 2",  "Ток батареи 2",         REG_SHORT,   0.1f,  "А"},
  {233, "Inv Charge Current", "Ток заряда инвертора",  REG_SHORT,   0.1f,  "А"},
  {234, "PV Charge Current",  "Ток заряда PV",         REG_SHORT,   0.1f,  "А"}
};

const size_t ANENJI_REGISTER_COUNT =
  sizeof(ANENJI_REGISTERS) / sizeof(ANENJI_REGISTERS[0]);

inline const char* workingModeName(uint16_t mode) {
  switch (mode) {
    case 0: return "Включено";
    case 1: return "Режим ожидания";
    case 2: return "Основной режим";
    case 3: return "Автономная работа";
    case 4: return "Байпас от сети к нагрузке";
    case 5: return "Зарядка";
    case 6: return "Авария";
    default: return "Неизвестно";
  }
}

inline const char* faultName(uint8_t bit) {
  switch (bit) {
    case 1: return "Перегрев DCDC";
    case 2: return "Перенапряжение батареи";
    case 4: return "Короткое замыкание выхода";
    case 5: return "Перенапряжение инвертора";
    case 6: return "Перегрузка выхода";
    case 7: return "Перенапряжение шины";
    case 9: return "Переток PV";
    case 10: return "Перенапряжение PV";
    case 11: return "Переток батареи";
    case 12: return "Переток инвертора";
    case 13: return "Низкое напряжение шины";
    case 15: return "Высокая составляющая DC";
    case 17: return "Ошибка нулевого смещения выхода";
    case 18: return "Ошибка нулевого смещения инвертора";
    case 19: return "Ошибка нулевого смещения батареи";
    case 20: return "Ошибка нулевого смещения PV";
    case 21: return "Низкое напряжение инвертора";
    case 22: return "Защита от отрицательной мощности";
    case 23: return "Потеря ведущего устройства";
    case 24: return "Ошибка синхронизации";
    case 26: return "Несовместимые версии";
    default: return "Неизвестная авария";
  }
}

inline const char* warningName(uint8_t bit) {
  switch (bit) {
    case 0: return "Потеря перехода через ноль";
    case 1: return "Ненормальная форма сети";
    case 2: return "Перенапряжение сети";
    case 3: return "Низкое напряжение сети";
    case 4: return "Высокая частота сети";
    case 5: return "Низкая частота сети";
    case 6: return "Низкое напряжение PV";
    case 7: return "Перегрев";
    case 8: return "Низкое напряжение батареи";
    case 9: return "Батарея не подключена";
    case 10: return "Перегрузка";
    case 11: return "Уравнительный заряд";
    case 12: return "Глубокий разряд батареи";
    case 13: return "Ограничение мощности";
    case 14: return "Заблокирован вентилятор";
    case 15: return "Недостаточно энергии PV";
    case 16: return "Прерывание параллельной связи";
    case 17: return "Несовместимый режим";
    case 18: return "Разница напряжений батарей";
    default: return "Неизвестное предупреждение";
  }
}

#endif