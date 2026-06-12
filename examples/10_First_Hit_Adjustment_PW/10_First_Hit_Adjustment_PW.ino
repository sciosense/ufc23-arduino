/*
    First Hit Adjustment PW

    This example shows how to adjust the first hit level by keeping track of the first hit pulse widths.
    To compensate for changes in atenuation, the pulse width ratio of the first hit will be kept constant
*/

#include <Arduino.h>
#include <SPI.h>
#include <ScioSense_UFC23.h>

#define PIN_INTN        4           // Define pin for interrupt
#define PIN_CS          5           // Define pin for chip select
#define SPI_SPEED       1000000     // Speed of the SCLK on the SPI interface

#define PW_TARGET       0.75        // Target ratio between the first hit that crosses the threshold and the first full pulse
#ifndef HALF_PI
    #define HALF_PI     1.570796    // pi / 2
#endif

static UFC23 ufc23;

float tofAvgUp[UFC23_AMOUNT_BUNDLES_MAX], tofAvgDn[UFC23_AMOUNT_BUNDLES_MAX];
UFC23_PW_Ps_TypeDef pwRatioUp[UFC23_AMOUNT_BUNDLES_MAX], pwRatioDn[UFC23_AMOUNT_BUNDLES_MAX];

float targetPWRatioCosine;
float inverseFHLLsb_V;
float thresholdZCLUp;
float thresholdZCLDn;

uint8_t WaitOnInterrupt(unsigned long timeoutMs);

float CosineApproximation(float x)
{
    // Taylor approximation of cosine with less than 2% error in [0, pi/2]
    // cos(x) ~ 1 - x2 / 2 + x4 / 24
    float sqX = x * x;
    return 1.0 - sqX * 0.5 + sqX * sqX * 0.041667;
}

void setup()
{
    Serial.begin(9600);

    Serial.println("Starting UFC23 10_First_Hit_Adjustment_PW demo on Arduino...");

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
        0x0000001C,     // A0
        0x00000FF1,     // A1
        0x000006DB,     // A2
        0x00000010,     // A3
        0x0000178F,     // A4
        0x0000B100,     // A5
        0x00001249,     // A6
        0x000194F4,     // A7
        0x00000000,     // A8
        0x04900000,     // A9
        0xC0090034,     // AA
        0x0000140E,     // AB
        0x00000000,     // AC
        0x0808B00E,     // AD
        0x46301024,     // AE
        0x0FFFFFFF,     // AF
        0x0001424E,     // B0
        0x0C412424,     // B1
        0x00000000      // B2
    };

    ufc23.setConfigurationRegisters(configRegisters);

    // Measure the High Speed Oscillator frequency
    float hsoFreqMHz[UFC23_AMOUNT_BUNDLES_MAX];
    ufc23.getHighSpeedOscillatorFrequencyMhz(hsoFreqMHz);
    Serial.print("High Speed Clock Frequency: ");
    Serial.print(hsoFreqMHz[0]);
    Serial.println(" MHz");

    inverseFHLLsb_V = 1.0 / UFC23_FHL_LSB_V;
    targetPWRatioCosine = CosineApproximation(HALF_PI * PW_TARGET);
    thresholdZCLUp = (float)(ufc23.Param.CR_B1.C_USM_FHL_UP) * UFC23_FHL_LSB_V;
    thresholdZCLDn = (float)(ufc23.Param.CR_B1.C_USM_FHL_DN) * UFC23_FHL_LSB_V;

    if( ufc23.writeConfig() == RESULT_OK )
    {
        Serial.println("Configuration properly written for measurements");
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
            }

            // Print the received pulse widths
            uint8_t amountPwRatioMeasurements = ufc23.getPulseWidthMeasurementResultsRatio(pwRatioUp, pwRatioDn);
            if( amountPwRatioMeasurements )
            {
                Serial.print("\tTargetPWRatio:");
                Serial.print(PW_TARGET);
                Serial.print("\tPWRatioUp1:");
                Serial.print(pwRatioUp[0].PW1_FHL);
                Serial.print("\tPWRatioDn1:");
                Serial.print(pwRatioDn[0].PW1_FHL);
                
                thresholdZCLUp = thresholdZCLUp * targetPWRatioCosine / CosineApproximation(HALF_PI * pwRatioUp[0].PW1_FHL);
                thresholdZCLDn = thresholdZCLDn * targetPWRatioCosine / CosineApproximation(HALF_PI * pwRatioDn[0].PW1_FHL);

                float newFHLUp = thresholdZCLUp * inverseFHLLsb_V;
                float newFHLDn = thresholdZCLDn * inverseFHLLsb_V;
                
                ufc23.Param.CR_B1.C_USM_FHL_UP = floor(newFHLUp);
                ufc23.Param.CR_B1.C_USM_FHL_DN = floor(newFHLDn);

                // Update the configuration and restart measurements
                ufc23.updateConfiguration();

                uint8_t successRewritingConfig = 0;
                if( ufc23.writeConfig() == RESULT_OK )
                {
                    if( ufc23.startMeasurement() == RESULT_OK )
                    {
                        successRewritingConfig = 1;
                    }
                }
                if( !successRewritingConfig )
                {
                    Serial.println("Error! Configuration couldn't be changed properly");
                }
            }
            Serial.println("");
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

uint8_t WaitOnInterrupt(unsigned long timeoutMs)
{
    uint8_t success = 0;
    uint32_t startMs   = millis();
    uint8_t timeoutElapsed = 0;

    while( (digitalRead(PIN_INTN)) && (!timeoutElapsed) )
    {
        timeoutElapsed = ( (millis() - startMs) > timeoutMs );
    }

    if( !timeoutElapsed )
    {
        success = 1;
    }
    
    return success;
}