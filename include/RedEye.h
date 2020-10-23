#include <Arduino.h>

const int STATE_IDLE = 0;
const int STATE_START = 1;
const int STATE_ECC = 2;
const int STATE_DATA = 4;
const int STATE_STOP = 5;

const int txPin = 1;
const int rxPin = 2;
const int ledPin = 13;
const int cycleTime = 427;
const int BUFFER_SIZE = 1024;

static const byte H[] = {0b01111000,
                         0b11100110,
                         0b11010101,
                         0b10001011};