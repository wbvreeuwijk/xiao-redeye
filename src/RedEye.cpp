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
volatile int_fast16_t eccPulses = 0;
volatile byte dataByte = 0;
volatile int_fast16_t dataPulses = 0;
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
    Calculate Parity
*/
int parity(byte d)
{
    int p = 0;
    while (d > 0)
    {
        p += d & 0x01;
        d >>= 1;
    }
    return p % 2;
}

/* 
    Calculate missing bits
*/
byte getMissing(int_fast16_t b)
{
    byte m = 0;
    while (b > 0)
    {
        m = (m << 1) | !((b & 1) ^ ((b >> 1) & 1));
        b = b >> 2;
    }
    return m;
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
    //if (pulseDetected)
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
            eccPulses = 0;
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
        eccPulses |= ((pulseDetected) ? 1 : 0) << eccHalfBit;
        eccHalfBit++;
        // Go to data part of the frame after 8 halfbits
        if (eccHalfBit == 8)
        {
            state = STATE_DATA;
            dataHalfBit = 0;
            dataPulses = 0;
            dataByte = 0;
        }
    }
    else if (state == STATE_DATA)
    {
        // Check for data halfbits
        if (pulseDetected)
            dataByte = getHalfBit(dataByte, dataHalfBit);
        dataPulses |= ((pulseDetected) ? 1 : 0) << dataHalfBit;
        dataHalfBit++;
        // After 16 halfbits we have a data byte
        // Check if the ECC matches the calculated ECC
        if (dataHalfBit == 16)
        {
            state = STATE_IDLE;
            startBit = 0;
            byte missingECCBits = getMissing(eccPulses);
            byte missingDataBits = getMissing(dataHalfBit);
            // Only check error correction when we have no missing ECC bits
            if (missingECCBits == 0 && missingECCBits == 0)
            { // Complete error free frame
                Serial.write(dataByte);
#ifdef DEBUG
                Serial.println();
#endif
            }
            else if (missingECCBits != 0)
            { // Error in ECC
#ifdef DEBUG
                Serial.print(missingECCBits, BIN);
                Serial.write(":");
                Serial.write(dataByte);
                Serial.println();
#else
                Serial.write(dataByte);
#endif
            }
            else if (missingDataBits != 0)
            {
                while (missingDataBits != 0x00)
                {
                    for (byte i = 0; i < 4; i++)
                    {
                        byte mask = H[i];
                        byte x = missingDataBits & mask;
                        if (parity(x) == 1)
                        {
                            if (parity(dataByte & mask) != ((eccByte >> (3 - i)) & 0x01))
                                dataByte |= x;
                            missingDataBits &= ~x;
                            break;
                        }
                    }
                }
#ifdef DEBUG
                Serial.print(missingDataBits, BIN);
                Serial.write(":");
                Serial.write(dataByte);
                Serial.println();
#else
                Serial.write(dataByte);
#endif
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
