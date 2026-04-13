#include <Arduino.h>
#include <SPI.h>
#include <ScioSense_UFC23.h>

#define PIN_INTN    4     // define pin for interrupt
#define PIN_CS      5     // define pin for chip select
#define SPI_SPEED   1000000

static UFC23 ufc23;

float temp1, temp2;
float tofAvgUp[UFC23_AMOUNT_BUNDLES_MAX], tofAvgDn[UFC23_AMOUNT_BUNDLES_MAX];

void setup()
{
    Serial.begin(9600);

    Serial.println("Starting UFC23 05_Temperature demo on Arduino...");

    SPI.begin();

    pinMode(PIN_CS, OUTPUT);
    ufc23.begin(&SPI, PIN_CS, SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE1));

    if( ufc23.init() )
    {
        Serial.print(ufc23.partIdToString(ufc23.partId));
        Serial.println(" initialized properly");
    }
    else
    {
        Serial.println("UFC23 type not recognized");
    }

    // Single ended configuration
    uint32_t configRegisters[UFC23_AMOUNT_CONFIGURATION_REGISTERS] =
    {
        0x0000001C,     // A0   Interrupt sources
        0x00000FF1,     // A1   Error flag sources
        0x000006DB,     // A2   GPIO
        0x00000010,     // A3   LDO refresh rate
        0x000017AF,     // A4   Timing between US and DS, direction
        0x0000B100,     // A5   Timing between measurements
        0x00011249,     // A6   Which measurements to perform every how many cycle triggers
        0x000194F4,     // A7   HSO clock frequency and temp ports
        0x00000000,     // A8
        0x04900000,     // A9   Reserved
        0xC00F0034,     // AA   Amount multi hits, USM mask, enable of amplitude, pwd, zero cross
        0x0000140E,     // AB   Fire burst frequency and pulses
        0x00000000,     // AC   Reserved
        0x0808B00E,     // AD   Reserved
        0x46301024,     // AE   Resistance matching and PGA
        0x0FFFFFFF,     // AF   Init times
        0x0001424E,     // B0   ToF hits to store, start, etc
        0x20412424,     // B1   Number of peaks for amplitude monitoring
        0x00000000      // B2   Fine USM mask
    };

    ufc23.setConfigurationRegisters(configRegisters);

    // Measure the High Speed Oscillator frequency
    float hsoFreqMHz[UFC23_AMOUNT_BUNDLES_MAX];
    ufc23.getHighSpeedOscillatorFrequencyMhz(hsoFreqMHz);
    Serial.print("High Speed Clock Frequency: ");
    Serial.print(hsoFreqMHz[0]);
    Serial.println(" MHz");

    if( ufc23.writeConfig() == RESULT_OK )
    {
        Serial.println("Configuration properly written");
    }
    else
    {
        Serial.println("Error! Configuration read doesn't match the values written");
    }
    
    if( ufc23.startMeasurement() == RESULT_OK )
    {
        Serial.println("Measurements started");
    }
    else
    {
        Serial.println("Error! Measurements didn't start properly");
    }
}

void loop()
{
    if( !digitalRead(PIN_INTN) )
    {
        uint8_t dataPrinted = 0;

        if( ufc23.update() == RESULT_OK )
        {
            // Print the averaged hit sums
            uint8_t amountMultiHitMeas = ufc23.getAverageHitNs(tofAvgUp, tofAvgDn);
            if( amountMultiHitMeas )
            {
                Serial.print("AvgTofUp[ns]:");
                Serial.print(tofAvgUp[0]);
                Serial.print("\tAvgTofDn[ns]:");
                Serial.print(tofAvgDn[0]);
                Serial.print("\tTofDiff[ns]:");
                Serial.print(tofAvgUp[0] - tofAvgDn[0]);
                Serial.print("\t");
                
                dataPrinted = 1;
            }

            // Print the measured temperatures
            if( ufc23.getTemperaturesSeq1DegC(&temp1, &temp2) )
            {
                Serial.print("Temp1[degC]:");
                Serial.print(temp1);
                Serial.print("\tTemp2[degC]:");
                Serial.print(temp2);

                dataPrinted = 1;
            }

            if( dataPrinted )
            {
                Serial.println("");
            }
        }
        else
        {
            if( ufc23.hasError() )
            {
                char errorStrings[UFC23_AMOUNT_FRONTEND_ERROR_FLAGS][ERROR_STRING_LENGTH];
                uint8_t amountFoundErrors = ufc23.errorToStrings(ufc23.getErrors(), errorStrings);
                for( uint8_t error = 0; error < amountFoundErrors; error++ )
                {
                    Serial.println(errorStrings[error]);
                }
            }
        }
    }
}