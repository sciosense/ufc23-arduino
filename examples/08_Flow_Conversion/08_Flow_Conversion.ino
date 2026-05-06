/*
    Flow Conversion

    This example calculates the flow in litres per hour using transducer dimensions that need to be adjusted for your application.
    The speed of sound in water is taken as a constant. It is best practice to place a sensor on the flow and measure its temperature, calculating the speed of sound from it.
    Due to differences in transducer construction the zero flow value of the Time-of-Flight difference will not be exactly zero. For more accuracy, that value should be read and removed.
    The fluid will not present a constant velocity throught the full transducer cross-section. Due to this, a calibration point at the expected flow rate increases significantly the accuracy.
    The configuration was chosen for an Audiowell HS0014-000 transducer in water.
*/

#include <Arduino.h>
#include <SPI.h>
#include <ScioSense_UFC23.h>
#include <ufc23_adaptive_filter.h>

// Interface parameters
#define PIN_INTN    4                               // Define pin for interrupt
#define PIN_CS      5                               // Define pin for chip select
#define SPI_SPEED   1000000                         // Speed of the SCLK on the SPI interface

#define UFC23_NS_TO_S                   0.000000001 // Conversion from nanoseconds to seconds
#define UFC23_M3_TO_L                   1000.0      // Conversion from m3 to litres
#define UFC23_HOUR_TO_SECONDS           3600.0      // Conversion from hours to seconds
#define UFC23_PI                        3.1415      // Value of the constant pi

#define WATER_SOUND_SPEED_M_S           1480.0      // Speed of sound in meters per second. It is best to calculate it from the water temperature

#define DISTANCE_BETWEEN_TRANSDUCERS_M  0.062       // Distance between the upstream and downstream transducers in meters
#define TRANSDUCER_CROSS_SECTION_M2     0.000113    // Cross section area of the transducer at the point where the ultrasound waves travel in meters squared
#define ANGLE_TRANSDUCERS_FLOW_DEGREES  0           // Angle in degrees between the path of the ultrasound and the direction of the flow in degrees

static UFC23 ufc23;
Ufc23Filter ufc23Filter;

float tofAvgUp[UFC23_AMOUNT_BUNDLES_MAX], tofAvgDn[UFC23_AMOUNT_BUNDLES_MAX];

float conversionTof2Flow;

void setup()
{
    Serial.begin(9600);

    Serial.println("Starting UFC23 08_Flow_Conversion demo on Arduino...");

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
        0x000017AF,     // A4
        0x0000B100,     // A5
        0x00001249,     // A6
        0x000194F4,     // A7
        0x00000000,     // A8
        0x04900000,     // A9
        0xC00D0034,     // AA
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

    float conversionTof2Speed   = WATER_SOUND_SPEED_M_S * WATER_SOUND_SPEED_M_S * cosf(ANGLE_TRANSDUCERS_FLOW_DEGREES * UFC23_PI / 180) * UFC23_NS_TO_S / ( 2.0 * DISTANCE_BETWEEN_TRANSDUCERS_M ) ;
    float conversionSpeed2Flow  = TRANSDUCER_CROSS_SECTION_M2 * UFC23_HOUR_TO_SECONDS * UFC23_M3_TO_L;
    conversionTof2Flow          = conversionTof2Speed * conversionSpeed2Flow;
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
                float difTofNs = tofAvgUp[0] - tofAvgDn[0];
                float filteredDifTofNs = ufc23Filter.ApplyFilter(difTofNs);
                
                float unFilteredFlowLPH = conversionTof2Flow * difTofNs;
                float filteredFlowLPH   = conversionTof2Flow * filteredDifTofNs;

                Serial.print("UnfilteredFlow[LPH]:");
                Serial.print(unFilteredFlowLPH);
                Serial.print(",FilteredFlow[LPH]:");
                Serial.println(filteredFlowLPH);
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