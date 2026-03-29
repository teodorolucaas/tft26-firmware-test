// =============================================================================
//  Exercise 03 — I2C Sensors (Bit-bang)
// =============================================================================
//
//  Virtual hardware:
//    P8 (SCL)  →  io.digital_write(8, …) / io.digital_read(8)
//    P9 (SDA)  →  io.digital_write(9, …) / io.digital_read(9)
//
//  PART 1 — TMP64 temperature sensor at I2C address 0x48
//    Register 0x0F  WHO_AM_I   — 1 byte  (expected: 0xA5)
//    Register 0x00  TEMP_RAW   — 4 bytes, big-endian int32_t, milli-Celsius
//
//  PART 2 — Unknown humidity sensor (same register layout, address unknown)
//    Register 0x0F  WHO_AM_I   — 1 byte
//    Register 0x00  HUM_RAW    — 4 bytes, big-endian int32_t, milli-percent
//
//  Goal (Part 1):
//    1. Implement an I2C master via bit-bang on P8/P9.
//    2. Read WHO_AM_I from TMP64 and confirm the sensor is present.
//    3. Read TEMP_RAW in a loop and print the temperature in °C every second.
//    4. Update display registers 6–7 with the formatted temperature string.
//
//  Goal (Part 2):
//    5. Scan the I2C bus (addresses 0x08–0x77) and print every responding address.
//    6. For each unknown device found, read its WHO_AM_I and print it.
//    7. Add the humidity sensor to the 1 Hz loop: read HUM_RAW and print %RH.
//
//  Read README.md before starting.
// =============================================================================

// =============================================================================
//  Exercise 03 — I2C Sensors (Bit-bang)
// =============================================================================

// =============================================================================
// Author: Lucas Teodoro
//
// This algorithm implements a class responsible for handling
// I2C communication using bit-bang.
//
// The class is responsible for:
// - Method to initialize the hardware (SDA and SCL with pull-up resistors)
// - Method to generate start and stop conditions
// - Methods to write and read data on the bus
// - Method to read 1-byte and 4-byte registers, handling the full
//   I2C sequence (start, write, restart, read, stop)
//
// To find the address of the humidity sensor, a scan method was
// implemented. It scans all addresses in the defined range and
// checks the response from the slave (ACK). For each address, the
// result is printed, and for detected devices, the WHO_AM_I value
// is read.
//
// Since the temperature sensor address was already known, a logic
// was added: the first address found different from the temperature
// sensor is stored as the humidity sensor address.
//
// In the main method, the I2C communication is initialized, and the
// humidity sensor address is found. Then, the WHO_AM_I of both sensors
// is printed on the display and validated.
//
// Finally, inside the main loop, a condition was implemented to update
// the values at a rate of 1 Hz. Inside this condition, the temperature 
// and humidity are read and printed on the display.
// =============================================================================

#include <trac_fw_io.hpp>
#include <cstdio>
#include <cstdint>

#include <trac_fw_io.hpp>
#include <cstdint>

//To avoid modifying the simulator core, the class was created inside main.cpp.
//The class implements the I2C methods.
class SoftI2C {
private:
    trac_fw_io_t& io;
    uint8_t scl;
    uint8_t sda;
    uint32_t delay_ms;

    void d();

public:
    SoftI2C(trac_fw_io_t& hw, uint8_t sclPin, uint8_t sdaPin, uint32_t delayTime);

    void begin();
    void start();
    void stop();

    bool writeByte(uint8_t value);
    bool scanHum(uint8_t& value);
    uint8_t readByte(bool ack);

    bool readReg8(uint8_t addr, uint8_t reg, uint8_t& value);
    bool readReg32(uint8_t addr, uint8_t reg, int32_t& value);
};

SoftI2C::SoftI2C(trac_fw_io_t& hw, uint8_t sclPin, uint8_t sdaPin, uint32_t delayTime)
    : io(hw), scl(sclPin), sda(sdaPin), delay_ms(delayTime) {}

void SoftI2C::d() {
    io.delay(delay_ms);
}

//Method responsible for initializing the I2C hardware
void SoftI2C::begin() {
    //Configure pullup on SDA and SCL
    io.set_pullup(scl, true);
    io.set_pullup(sda, true);
    //Set SCL and SDA high.
    io.digital_write(scl, 1);
    io.digital_write(sda, 1);
    d();
}

//Method that implements the start condition, where SCL stays high and SDA goes low first
void SoftI2C::start() {
    io.digital_write(sda, 1);
    io.digital_write(scl, 1);
    d();
    //First, SDA falls while SCL is HIGH to start.
    io.digital_write(sda, 0);
    d();

    io.digital_write(scl, 0);
    d();
}

//Method that implements the stop condition, where SDA goes high while SCL is still high
void SoftI2C::stop() {
    //To stop, SDA rises while SCL high
    io.digital_write(sda, 0);
    d();

    io.digital_write(scl, 1);
    d();

    io.digital_write(sda, 1);
    d();
}

//Method that implements the logic to write one byte
bool SoftI2C::writeByte(uint8_t value) {
    uint8_t mask = 0x80;

    //Send the data 
    for (int i = 0; i < 8; i++) {
        io.digital_write(sda, (value & mask) != 0);

        io.digital_write(scl, 1);
        d();

        io.digital_write(scl, 0);
        d();

        mask >>= 1;
    }

    io.digital_write(sda, 1); //Release SDA so slave can drive ACK
    io.digital_write(scl, 1);
    d();

    bool ack = !io.digital_read(sda); //Read the response from slave (We expect ack == 0)

    io.digital_write(scl, 0);
    d();

    return ack;
}

//Function responsible for scanning the bus, printing every address, 
//and returning the first address found different from 0x48 (temperature sensor)
bool SoftI2C::scanHum(uint8_t& value){

    bool found=false;

    for (uint8_t addr = 0x08; addr <= 0x77; addr++)
    {
        uint8_t who=0;
        start();
        bool ok = writeByte(addr << 1);//Test every address
        
        std::printf("Addr = 0x%02X Value: %d\n", addr,ok);
        if(ok){
            //Find device and print the WHO_AM_I value
            std::printf("Device found at 0x%02X\n", addr);
            readReg8(addr, 0x0F, who);
            std::printf("WHO_AM_I Device found= 0x%02X\n", who);

            if (addr != 0x48 && !found) //Store the first unknown address and different from 0x48
            {
                value = addr;
                found = true;
                std::printf("Unknown sensor stored: 0x%02X\n", addr);
            }
        }
        stop();
    }
    return found;

}

//Method that implements the logic to read one byte, with a flag to
//indicate to the slave if more data will be requested
uint8_t SoftI2C::readByte(bool ack) {
    uint8_t value = 0;

    //Release SDA
    io.digital_write(sda, 1);
    d();

    //Read the 8 bits 
    for (int i = 0; i < 8; i++) {
        value <<= 1;

        io.digital_write(scl, 1);
        d();

        if (io.digital_read(sda)) {
            value += 1;
        }

        io.digital_write(scl, 0);
        d();
    }

    //Stop or read more data
    if (ack) io.digital_write(sda, 0);
    else     io.digital_write(sda, 1);

    d();

    io.digital_write(scl, 1);
    d();

    io.digital_write(scl, 0);
    d();

    io.digital_write(sda, 1);
    d();

    return value;
}

//Procedure to read one byte. This method implements the full process:
//start, write, restart, read, and stop.
bool SoftI2C::readReg8(uint8_t addr, uint8_t reg, uint8_t& value) {
    start();

    if (!writeByte(addr << 1)) {
        stop();
        return false;
    }

    if (!writeByte(reg)) {
        stop();
        return false;
    }

    start();

    if (!writeByte((addr << 1) + 1)) {
        stop();
        return false;
    }

    value = readByte(false);
    stop();
    return true;
}

//Procedure to read 4 bytes. This process is similar to the
//single-byte read function, but it signals to the readByte
//function that the first three bytes should send ACK,
//indicating that more data will be read.
//
//The last byte sends NACK to finish the read. After that,
//bit manipulation is used to combine the 4 bytes into a
//single 32-bit value.
bool SoftI2C::readReg32(uint8_t addr, uint8_t reg, int32_t& value) {
    start();

    if (!writeByte(addr << 1)) {
        stop();
        return false;
    }

    if (!writeByte(reg)) {
        stop();
        return false;
    }

    start();

    if (!writeByte((addr << 1) + 1)) {
        stop();
        return false;
    }
    
    uint8_t b0 = readByte(true);
    uint8_t b1 = readByte(true);
    uint8_t b2 = readByte(true);
    uint8_t b3 = readByte(false); //The function parameter indicates that this is the last byte to be read 

    stop();

    //Builds the 4-byte variable
    value =
        ((int32_t)b0 << 24) |
        ((int32_t)b1 << 16) |
        ((int32_t)b2 << 8)  |
        ((int32_t)b3);

    return true;
}

//Function responsible for printing on the display
void printToDisplay(trac_fw_io_t& io, uint8_t regA, uint8_t regB, const char* buf)
{
    uint32_t rA = 0, rB = 0;

    std::memcpy(&rA, buf + 0, 4);
    std::memcpy(&rB, buf + 4, 4);

    io.write_reg(regA, rA);
    io.write_reg(regB, rB);
}

int main() {

    trac_fw_io_t io;
    SoftI2C i2c(io, 8, 9, 1);

    char buf[9] = {};
    uint8_t whoTemp = 0, whoHum = 0;
    uint8_t addrTemp = 0x48, addrHum = 0;
    int32_t tempRaw = 0, humRaw = 0;
    uint8_t tempConnected = 0, humConnected = 0;

    i2c.begin();

    printToDisplay(io, 6, 7, "Booting");
    i2c.scanHum(addrHum); //Find the address of the humidity sensor
    
    //Print the WHO_AM_I on the console
    i2c.readReg8(addrTemp, 0x0F, whoTemp);
    std::printf("WHO_AM_I Temp= 0x%02X\n", whoTemp);
    i2c.readReg8(addrHum, 0x0F, whoHum);
    std::printf("WHO_AM_I Hum= 0x%02X\n", whoHum);

    //Print the WHO_AM_I on the display
    std::snprintf(buf, sizeof(buf), "0x%02X", whoTemp);
    printToDisplay(io, 6, 7, buf);
    std::snprintf(buf, sizeof(buf), "0x%02X", whoHum);
    printToDisplay(io, 4, 5, buf);
    
    if(whoTemp==0xA5){
        tempConnected=1;
        std::printf("Temperature sensor is connected\n");
    }

    if(whoHum==0x62){
        humConnected=1;
        std::printf("Humidity sensor is connected\n");
    }
   
    uint64_t previous = io.millis();

    while (true) {

        //To keep a frequency of 1 Hz
        if((io.millis()-previous)>1000){
            //Read temperature and print it
            if (i2c.readReg32(addrTemp, 0x00, tempRaw) && tempConnected) {
                float tempC = tempRaw / 1000.0f;
                //std::printf("Temp: %.2f C\n", tempC);
                std::snprintf(buf, sizeof(buf), "%8.3fC", tempC);
                printToDisplay(io, 6, 7, buf);
            }

            //Read humidity and print it
            if (i2c.readReg32(addrHum, 0x00, humRaw) && humConnected) {
                float hum = humRaw / 1000.0f;
                //std::printf("Hum: %.2f %%\n", hum);
                std::snprintf(buf, sizeof(buf), "%7.3f%%", hum);
                printToDisplay(io, 4, 5, buf);
            }
            previous=io.millis();
        }  

        io.delay(10);
    }
}