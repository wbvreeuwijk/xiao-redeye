#include "RedEye.h"
#include <TimerTC3.h>

char time = 0;

volatile int state = STATE_IDLE;

volatile int subCycle = 0;

volatile int pulse = 0;
volatile int startBit = 0;
volatile int eccHalfBit = 0;
volatile int dataHalfBit = 0;
volatile byte eccByte = 0;
volatile byte eccMissingBits = 0;
volatile byte dataByte = 0;
volatile byte dataMissingBits = 0;
volatile boolean prevPulseDetected = false;

/*
    Calculate the checksum for the received bytes
*/
static byte calcECC(byte data)
{
    byte ret = 0x00;
    if ((data & 0x01) != 0)
        ret ^= 0x03;
    if ((data & 0x02) != 0)
        ret ^= 0x05;
    if ((data & 0x04) != 0)
        ret ^= 0x06;
    if ((data & 0x08) != 0)
        ret ^= 0x09;
    if ((data & 0x10) != 0)
        ret ^= 0x0A;
    if ((data & 0x20) != 0)
        ret ^= 0x0C;
    if ((data & 0x40) != 0)
        ret ^= 0x0E;
    if ((data & 0x80) != 0)
        ret ^= 0x07;
    return ret;
}

/*
    Process halfbit
*/
byte getHalfBit(byte data, int halfBit)
{
    return data = data << 1 | ((halfBit % 2) == 0 ? 1 : 0);
}

/*
    Process halfbits that are part of the frame
*/
void pulseProcess()
{
    // Make sure that the timing is correct
    // digitalWrite(txPin, true);
    if (subCycle == 3)
    {
        TimerTc3.setPeriod(cycleTime + 1);
        subCycle = 0;
    }
    else
    {
        TimerTc3.setPeriod(cycleTime);
        subCycle++;
    }

    // Did we get a pulse in this halbit
    boolean pulseDetected = pulse >= 6;
    if (pulseDetected)
        pulse = 0;

    // Start halfbit processing
    if (state == STATE_START)
    {
        // Count the start halfbits
        if (pulseDetected)
            startBit++;

        // After three start halfbits we start on the ECC code
        if (startBit >= 3)
        {
            state = STATE_ECC;
            eccByte = 0;
            eccMissingBits = 0;
            eccHalfBit = 0;
        }
    }
    else if (state == STATE_ECC)
    {
        // Check for a puls in this half bit period
        // If we have a pulse in the first (even) halfbit
        // We receive a one otherwise a zero
        if (pulseDetected)
            eccByte = getHalfBit(eccByte, eccHalfBit);
        else if (!prevPulseDetected)
            eccMissingBits |= 1 << int(eccHalfBit / 2);
        eccHalfBit++;
        // Go to data part of the frame after 8 halfbits
        if (eccHalfBit == 8)
        {
            state = STATE_DATA;
            dataHalfBit = 0;
            dataMissingBits = 0;
            dataByte = 0;
        }
    }
    else if (state == STATE_DATA)
    {
        // Check for data halfbits
        if (pulseDetected)
            dataByte = getHalfBit(dataByte, dataHalfBit);
        else if (!prevPulseDetected)
            dataMissingBits |= 1 << int(dataHalfBit / 2);
        dataHalfBit++;
        // After 16 halfbits we have a data byte
        // Check if the ECC matches the calculated ECC
        // TODO: Check missing bits against ECC and correct
        if (dataHalfBit == 16)
        {
            state = STATE_IDLE;
            startBit = 0;
            if (eccByte == calcECC(dataByte))
            {
                Serial.write(dataByte);
            }
            TimerTc3.stop();
        }
    }
    prevPulseDetected = pulseDetected;
}
/*
    Simply count incoming pulses and start time when first frame pulse is detected.
*/
void pulseCount()
{
    pulse++;
    if (state == STATE_IDLE)
    {
        TimerTc3.start();
        state = STATE_START;
    }
}

/*
    Setup serial output en define timers and interrupts
*/
void setup()
{
    // Setup serial output
    Serial.begin(115200);
    while (!Serial)
        ;

    //Serial.println("Start");
    // Setup Pins
    pinMode(rxPin, INPUT_PULLUP);
    pinMode(txPin, OUTPUT);
    digitalWrite(txPin, HIGH);

    // Set pulse processing timer
    TimerTc3.initialize(cycleTime);

    TimerTc3.attachInterrupt(pulseProcess);
    TimerTc3.stop();

    // Set pulse counter interrupt
    attachInterrupt(digitalPinToInterrupt(rxPin), pulseCount, FALLING);
}

void loop()
{
    // Nothing to do
}
