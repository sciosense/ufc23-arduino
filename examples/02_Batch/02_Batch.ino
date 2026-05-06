/*
    Batch

    This example measures 12 measurements (bundles) in one batch.
    The sensor triggers the interrupt when all measurements are ready.
    The configuration was chosen for an Audiowell HS0014-000 transducer in water.
*/

#include <Arduino.h>
#include <SPI.h>
#include <ScioSense_UFC23.h>

#define PIN_INTN    4           // Define pin for interrupt
#define PIN_CS      5           // Define pin for chip select
#define SPI_SPEED   1000000     // Speed of the SCLK on the SPI interface

static UFC23 ufc23;
uint16_t batchNumber;

float tofAvgUp[UFC23_AMOUNT_BUNDLES_MAX], tofAvgDn[UFC23_AMOUNT_BUNDLES_MAX];

void setup()
{
    Serial.begin(9600);

    Serial.println("Starting UFC23 02_Batch demo on Arduino...");

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
        0x00000018,     // A0
        0x00000FF1,     // A1
        0x000006DB,     // A2
        0x00000010,     // A3
        0x000017AF,     // A4
        0x0000B100,     // A5
        0x00001249,     // A6
        0x000194F4,     // A7
        0x00000000,     // A8
        0x04900000,     // A9
        0xC00DC034,     // AA
        0x0000140E,     // AB
        0x00000000,     // AC
        0x0808B00E,     // AD
        0x46301024,     // AE
        0x0FFFFFFF,     // AF
        0x0001424E,     // B0
        0x20412424,     // B1
        0x00000000      // B2
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

    batchNumber = 0;
}

void loop()
{
    if( !digitalRead(PIN_INTN) )
    {
        if( ufc23.update() == RESULT_OK )
        {
            // Print the averaged hit sums
            uint8_t amountBundles = ufc23.getAverageHitNs(tofAvgUp, tofAvgDn);
            for( uint8_t bundleIdx = 0; bundleIdx < amountBundles; bundleIdx++ )
            {
                Serial.print("Batch:");
                Serial.print(batchNumber);
                Serial.print("\tBundle:");
                Serial.print(bundleIdx);
                Serial.print("\tAvgTofUp[ns]:");
                Serial.print(tofAvgUp[bundleIdx]);
                Serial.print("\tAvgTofDn[ns]:");
                Serial.print(tofAvgDn[bundleIdx]);
                Serial.print("\tTofDiff[ns]:");
                Serial.println(tofAvgUp[bundleIdx] - tofAvgDn[bundleIdx]);
            }
            batchNumber++;
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