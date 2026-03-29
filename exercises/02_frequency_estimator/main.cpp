// =============================================================================
//  Challenge 02 — Frequency Estimator
// =============================================================================
//
//  Virtual hardware:
//    ADC Ch 0  →  io.analog_read(0)      Process sensor signal (0–4095)
//    OUT reg 3 →  io.write_reg(3, …)     Frequency estimate in centiHz
//                                        e.g. write_reg(3, 4733) = 47.33 Hz
//
//  Goal:
//    Measure the frequency of the signal on ADC channel 0 and publish your
//    estimate continuously via register 3.
//
//  Read README.md before starting.
// =============================================================================

// =============================================================================
// Author: Lucas Teodoro
//
// In this algorithm, hysteresis was implemented to detect the
// rising edges of the signal and reduce noise as much as possible.
//
// With this approach, the period of the signal is calculated at
// each rising edge, and then the frequency is obtained.
//
// To improve accuracy, a moving average was implemented using
// a circular buffer.
//
// A validation step was also added to discard period values
// outside the expected range, which helps to make the frequency
// estimator more robust.
//
// =============================================================================

#include <trac_fw_io.hpp>
#include <cstdio>

#include <trac_fw_io.hpp>
#include <cstdio>

#define BUFFER_SIZE 6

struct CircularBuffer
{
    uint32_t data[BUFFER_SIZE];
    uint32_t sum;
    uint8_t index;
    uint8_t count;
};

//Function to perform the operation on the circular buffer (moving average)
uint32_t buffer_push(CircularBuffer* buf, uint32_t value)
{
    buf->sum -= buf->data[buf->index];

    buf->data[buf->index] = value;

    buf->sum += value;

    buf->index++;
    if (buf->index >= BUFFER_SIZE)
        buf->index = 0;

    if (buf->count < BUFFER_SIZE)
        buf->count++;

    return buf->sum / buf->count;
}

int main()
{
    trac_fw_io_t io;
    
    //To use hysteresis, it is necessary to set the upper and lower thresholds
    //To define these thresholds, it was collected some samples of the raw signal
    //to analyze its behavior. After implementing the algorithm,
    //some small adjustments were made to achieve the expected result
    //in the simulator.
    const int TH_HIGH = 3000;
    const int TH_LOW  = 1500;
    const uint32_t MIN_PERIOD_MS = 10; //Minimum valid period     
    const uint32_t MAX_PERIOD_MS = 1000; //Maximum valid  period     

    //Flags to control
    bool currentState = false;
    bool previousState = false;
    bool firstEdge = true;

    uint32_t previousTime = 0;
    uint32_t period=0; 
    uint32_t  freq=0;
    uint32_t  avgFreq=0;

    CircularBuffer buf = {};

    const uint32_t SAMPLE_TIME_MS = 10;
    uint32_t lastSampleTime = 0;
    uint32_t currentSampleTime = 0;

    while (true)
    {
        uint32_t currentSampleTime = io.millis();

        //To control the sample rate
        if (currentSampleTime - lastSampleTime >= SAMPLE_TIME_MS)
        {
            lastSampleTime = currentSampleTime;

            int value = io.analog_read(0);

            //Histerese
            if(!currentState && value > TH_HIGH){
                currentState = true; //rising
            }
            else if (currentState && value < TH_LOW){
                currentState = false; //falling
            }

            //When the start of the period is detected (rising edge),
            //the frequency estimation process starts.
            if (!previousState && currentState)
            {
                uint32_t currentTime = io.millis();

                if(firstEdge){
                    firstEdge=false;
                }else{

                    period = currentTime - previousTime;

                    //Validation to discard invalid values
                    if(period >= MIN_PERIOD_MS && period <= MAX_PERIOD_MS){
                        freq= 100000 /period; //Frequency calculation
                        avgFreq = buffer_push(&buf, freq); //Moving average calculation
                        io.write_reg(3, avgFreq); //Write to register
                    } 
                } 
            previousTime=io.millis(); 
            }
            previousState = currentState;
        }
    }
}