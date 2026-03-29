// =============================================================================
//  Exercise 01 — Parts Counter
// =============================================================================
//
//  Virtual hardware:
//    SW 0        →  io.digital_read(0)        Inductive sensor input
//    Display     →  io.write_reg(6, …)        LCD debug (see README for format)
//                   io.write_reg(7, …)
//
//  Goal:
//    Count every part that passes the sensor and show the total on the display.
//
//  Read README.md before starting.
// =============================================================================

// =============================================================================
// Author: Lucas Teodoro
//
// This solution uses an interrupt triggered by an edge change on the input pin.
// Each time the interrupt occurs, a flag is set to indicate that a new event
// must be processed. The state of the pin is also captured at that moment.
//
// In the main loop, the event is processed. A control logic is implemented to
// avoid counting more than once for the same object. In addition, a debounce
// mechanism is used to prevent noise from being considered as a valid event.
//
// This is done by enforcing a minimum time between a falling edge and the next
// rising edge, ensuring more reliable counting.
// =============================================================================

#include <trac_fw_io.hpp>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <atomic>

//Protect variables shared between ISR and main
static std::atomic<bool> eventPending{false};
static std::atomic<bool> eventState{false};

static trac_fw_io_t* g_io = nullptr;

void on_interrupt()
{
    eventState = g_io->digital_read(0); //Capture state
    eventPending = true;
}

// Function responsible for printing on the display
void printToDisplay(trac_fw_io_t& io, uint8_t regA, uint8_t regB, const char* buf)
{
    uint32_t rA = 0, rB = 0;
    std::memcpy(&rA, buf + 0, 4);
    std::memcpy(&rB, buf + 4, 4);
    io.write_reg(regA, rA);
    io.write_reg(regB, rB);
}

int main()
{
    trac_fw_io_t io;
    g_io = &io;

    //Configure interrupt
    io.attach_interrupt(0, on_interrupt, InterruptMode::CHANGE);

    uint32_t count = 0;
    char buf[9] = {};

    bool lastState = io.digital_read(0);
    bool lock = false;

    uint32_t lowStartTime = 0;
    const uint32_t rearmLowMs = 40; //Time to avoid debounce

    std::snprintf(buf, sizeof(buf), "%8u", count);
    printToDisplay(io, 6, 7, buf);

    while (true)
    {
        uint32_t now = io.millis();

        //Check if there is a pending event
        if (eventPending)
        {
            eventPending = false; //Clear pending event

            bool currentState = eventState;

            //Check if it's a rising edge
            if (!lastState && currentState)
            {
                if (!lock)
                {
                    count++;
                    lock = true; //Lock for new count

                    //Print variable on the display
                    std::snprintf(buf, sizeof(buf), "%8u", count); 
                    printToDisplay(io, 6, 7, buf);
                }
            }

            //Check if it's a falling edge
            if (lastState && !currentState)
            {
                //Store the initial time of the falling edge
                lowStartTime = now;
            }

            lastState = currentState;
        }

        //Debounce logic
        //Check if it is a falling edge and if a count has already been made
        if (!lastState && lock)
        {
            //Check if it meets the minimum time
            if ((now - lowStartTime) >= rearmLowMs)
            {
                lock = false; //Unlock to allow a new count 
            }
        }

        io.delay(1);
    }
}