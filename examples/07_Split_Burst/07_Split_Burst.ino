/*
    Split Burst 

    This example introduces a split burst on the pulse generation. Then, it identifies the location of the burst on the received data.
    For identification, the time between each hit and the previous one is evaluated. If the difference exceeds the TOF_SHIFT_THRESHOLD_NS value it is marked as the phase shift point.

    This phase shift can be used for aligning the upstream and downstream measurements if one of them crosses the First Hit level at a different pulse number.
    It also allows to measure accurately the total Time-of-Flight, instead of just working with the difference.

    The serial baudrate was chosen to be the same as in the other examples, nonetheless it is not fast enough to send all the data in each cycle. Please consider increasing it.
    The configuration was chosen for an Audiowell HS0014-000 transducer in water.
*/

#include <Arduino.h>
#include <SPI.h>
#include <ScioSense_UFC23.h>

#define PIN_INTN    4                   // Define pin for interrupt
#define PIN_CS      5                   // Define pin for chip select
#define SPI_SPEED   1000000             // Speed of the SCLK on the SPI interface

#define TOF_SHIFT_THRESHOLD_NS  10.0    // How much the time between zero crossings have to change for it to be considered a product of the split burst

static UFC23 ufc23;

uint8_t amountHitsUp[UFC23_AMOUNT_TOF_HITS_MEAS], amountHitsDn[UFC23_AMOUNT_TOF_HITS_MEAS];
float tofAvgUp[UFC23_AMOUNT_TOF_HITS_MEAS], tofAvgDn[UFC23_AMOUNT_TOF_HITS_MEAS];
float tofHitsUp[UFC23_AMOUNT_TOF_HITS_MEAS], tofHitsDn[UFC23_AMOUNT_TOF_HITS_MEAS];

uint8_t totalAmountHits;
uint8_t nominalIdxPhaseShift;
uint8_t tofSumHits;
uint8_t tofMultihitStart;

void setup()
{
    // Increase the baudrate to capture every measurement from the sensor
    Serial.begin(9600);

    Serial.println("Starting UFC23 07_Split_Burst demo on Arduino...");

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
        0x0002140E,     // AB
        0x00000000,     // AC
        0x0808B00E,     // AD
        0x46301024,     // AE
        0x0FFFFFFF,     // AF
        0x00014268,     // B0
        0x20412424,     // B1
        0x00000000      // B2
    };

    ufc23.setConfigurationRegisters(configRegisters);

    tofMultihitStart        = ufc23.Param.CR_B0.C_TOF_MULTIHIT_START;
    tofSumHits              = ufc23.Param.CR_B0.C_TOF_MULTIHIT_NO;
    totalAmountHits         = ufc23.Param.CR_B0.C_TOF_HIT_NO;
    nominalIdxPhaseShift    = ufc23.Param.CR_AB.C_FBG_FBSP;

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
        if( ufc23.update() == RESULT_OK )
        {
            // Print the averaged hit sums
            uint8_t amountMultiHitMeas = ufc23.getAverageHitNs(tofAvgUp, tofAvgDn);
            if( amountMultiHitMeas )
            {
                // Print the individual hits
                if( ufc23.getIndividualTofHitsNs(tofHitsUp, tofHitsDn, amountHitsUp, amountHitsDn) == RESULT_OK )
                {
                    uint8_t amountHitsAvgTofStep = tofSumHits;
                    uint8_t minAmountReceivedHits = amountHitsUp[0];
                    if( amountHitsDn[0] < amountHitsUp[0] )
                    {
                        minAmountReceivedHits = amountHitsDn[0];
                    }
                    if( minAmountReceivedHits < tofSumHits )
                    {
                        amountHitsAvgTofStep = minAmountReceivedHits;
                    }

                    float avgTofStepUp = ( tofHitsUp[amountHitsAvgTofStep - 1] - tofHitsUp[0] ) / (float)(amountHitsAvgTofStep - 1);
                    float avgTofStepDn = ( tofHitsDn[amountHitsAvgTofStep - 1] - tofHitsDn[0] ) / (float)(amountHitsAvgTofStep - 1);

                    uint8_t phaseShiftIdxUp = 0;
                    for( uint8_t hitIdx = 1; hitIdx < amountHitsUp[0]; hitIdx++ )
                    {
                        if( abs(tofHitsUp[hitIdx] - tofHitsUp[hitIdx - 1] - avgTofStepUp) > (TOF_SHIFT_THRESHOLD_NS) )
                        {
                            phaseShiftIdxUp = hitIdx;
                            break;
                        }
                    }
                    uint8_t phaseShiftIdxDn = 0;
                    for( uint8_t hitIdx = 1; hitIdx < amountHitsDn[0]; hitIdx++ )
                    {
                        if( abs(tofHitsDn[hitIdx] - tofHitsDn[hitIdx - 1] - avgTofStepDn) > (TOF_SHIFT_THRESHOLD_NS) )
                        {
                            phaseShiftIdxDn = hitIdx;
                            break;
                        }
                    }

                    float stepsFromAvgToStart = (float)(tofSumHits - 1) / 2.0 + (float)tofMultihitStart;
                    float stepsCorrectionFromPhaseUp = (float)(nominalIdxPhaseShift - phaseShiftIdxUp);
                    float stepsCorrectionFromPhaseDn = (float)(nominalIdxPhaseShift - phaseShiftIdxDn);

                    float correctedTofUp = tofAvgUp[0] - (stepsFromAvgToStart + stepsCorrectionFromPhaseUp) * avgTofStepUp;
                    float correctedTofDn = tofAvgDn[0] - (stepsFromAvgToStart + stepsCorrectionFromPhaseDn) * avgTofStepDn;

                    Serial.print("AvgTofUp[ns]:");
                    Serial.print(tofAvgUp[0]);
                    Serial.print(",CorrectedTofUs[ns]:");
                    Serial.print(correctedTofUp);
                    Serial.print(",AvgTofDn[ns]:");
                    Serial.print(tofAvgDn[0]);
                    Serial.print(",CorrectedTofDs[ns]:");
                    Serial.print(correctedTofDn);
                    Serial.print(",TofDiff[ns]:");
                    Serial.println(correctedTofUp - correctedTofDn);

                    Serial.print("Hit:0,HitUp[ns]:");
                    Serial.print(tofHitsUp[0]);
                    Serial.print(",DeltaUp[ns]:0.00,HitDn[ns]:");
                    Serial.print(tofHitsDn[0]);
                    Serial.println(",DeltaDn[ns]:0.00");
                    for( uint8_t hitIdx = 1; hitIdx < amountHitsUp[0]; hitIdx++ )
                    {
                        Serial.print("Hit:");
                        Serial.print(hitIdx);
                        Serial.print(",HitUp[ns]:");
                        Serial.print(tofHitsUp[hitIdx]);
                        Serial.print(",DeltaUp[ns]:");
                        Serial.print(tofHitsUp[hitIdx] - tofHitsUp[hitIdx-1]);
                        Serial.print(",HitDn[ns]:");
                        Serial.print(tofHitsDn[hitIdx]);
                        Serial.print(",DeltaDn[ns]:");
                        Serial.print(tofHitsDn[hitIdx] - tofHitsDn[hitIdx-1]);
                        
                        if( hitIdx == phaseShiftIdxUp )
                        {
                            Serial.print(",PhaseShiftUp");
                        }
                        if( hitIdx == phaseShiftIdxDn )
                        {
                            Serial.print(",PhaseShiftDn");
                        }
                        Serial.println("");
                    }
                }
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