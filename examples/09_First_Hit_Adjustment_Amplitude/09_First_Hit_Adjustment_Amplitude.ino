/*
    First Hit Adjustment Amplitude

    This example shows how to adjust the first hit level by keeping track of the pulse amplitude.
    To compensate for changes in atenuation, the first hit level is scaled with an interpolation of the amplitudes
*/

#include <Arduino.h>
#include <SPI.h>
#include <ScioSense_UFC23.h>

#define PIN_INTN        4           // Define pin for interrupt
#define PIN_CS          5           // Define pin for chip select
#define SPI_SPEED       1000000     // Speed of the SCLK on the SPI interface

#define FHL_AMP_RATIO   0.75        // Ratio between the first hit threshold level and the first hit amplitude

static UFC23 ufc23;

float tofAvgUp[UFC23_AMOUNT_BUNDLES_MAX], tofAvgDn[UFC23_AMOUNT_BUNDLES_MAX];
UFC23_AMP_V_TypeDef ampUp[UFC23_AMOUNT_BUNDLES_MAX], ampDn[UFC23_AMOUNT_BUNDLES_MAX];

float measuredAmplitudes[3];
float peakPositions[]   = {0.0, 1.0, 2.0};
uint8_t amountPointsFit = 3;

typedef struct {
    float m;                       // Slope of the linear fitting
    float c;                       // Intercept of the linear fitting
} LinearModel;

LinearModel FitLinear(float x[], float y[], uint8_t n) {
    float sum_x = 0, sum_y = 0, sum_xy = 0, sum_xx = 0;
    
    for( uint8_t i = 0; i < n; i++ )
    {
        sum_x   += x[i];
        sum_y   += y[i];
        sum_xy  += x[i] * y[i];
        sum_xx  += x[i] * x[i];
    }
    
    float denominator = (n * sum_xx - sum_x * sum_x);
    float m = (n * sum_xy - sum_x * sum_y) / denominator;
    float c = (sum_y - m * sum_x) / n;
    
    return (LinearModel){m, c};
}

uint8_t WaitOnInterrupt(unsigned long timeoutMs);

void setup()
{
    Serial.begin(9600);

    Serial.println("Starting UFC23 09_First_Hit_Adjustment_Amplitude demo on Arduino...");

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
        0xC0030034,     // AA
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

            // Print the received amplitudes after the programable gain amplifier
            uint8_t amountAmplitudeMeasurementsV = ufc23.getAmplitudeMeasurementResultsAfterPgaV(ampUp, ampDn);
            if( amountAmplitudeMeasurementsV )
            {
                // Calculating new first hit level
                measuredAmplitudes[0] = ampUp[0].AMPL1;
                measuredAmplitudes[1] = ampUp[0].AMPL2;
                measuredAmplitudes[2] = ampUp[0].AMPL3;

                LinearModel fit = FitLinear(peakPositions, measuredAmplitudes, amountPointsFit);
                float estimatedFirstPeakUp = fit.c;
                float newFHLUp = estimatedFirstPeakUp * FHL_AMP_RATIO / UFC23_FHL_LSB_V;
                
                measuredAmplitudes[0] = ampDn[0].AMPL1;
                measuredAmplitudes[1] = ampDn[0].AMPL2;
                measuredAmplitudes[2] = ampDn[0].AMPL3;

                fit = FitLinear(peakPositions, measuredAmplitudes, amountPointsFit);
                float estimatedFirstPeakDn = fit.c;
                float newFHLDn = estimatedFirstPeakDn * FHL_AMP_RATIO / UFC23_FHL_LSB_V;

                // Printing the current state of the ratio between the target and measured ratios between the first hit level and the first hit amplitude
                float thresholdZCLUp = (float)(ufc23.Param.CR_B1.C_USM_FHL_UP) * UFC23_FHL_LSB_V;
                float thresholdZCLDn = (float)(ufc23.Param.CR_B1.C_USM_FHL_DN) * UFC23_FHL_LSB_V;
                Serial.print("\tTargetAmpRatio:");
                Serial.print(FHL_AMP_RATIO);
                Serial.print("\tAmpRatioUp1:");
                Serial.print(thresholdZCLUp / estimatedFirstPeakUp);
                Serial.print("\tAmpRatioUp2:");
                Serial.print(thresholdZCLDn / estimatedFirstPeakDn);

                // Update the configuration and restart measurements
                ufc23.Param.CR_B1.C_USM_FHL_UP = floor(newFHLUp);
                ufc23.Param.CR_B1.C_USM_FHL_DN = floor(newFHLDn);
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