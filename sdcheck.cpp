#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <random>
#include <cstdint>
#include <cassert>
#include <cstring>
#include <unistd.h>

using namespace std;

/**
 * This generator is used to generate the initial stream of values
 * that is written to the SD card
 */
static mt19937_64 randomGen;
/**
 * This generator is seeded with the same seed as randomGen.
 * It is used to generate a reference stream when reading back the data
 */
static mt19937_64 refGen;

/**
 * Generate a pseudo-random seed
 */
static uint64_t generateSeed() {
    random_device rand;
    //random_device generates unsigned ints, which might not be 64 bits
    if(sizeof(unsigned int) == sizeof(uint64_t)) {
        return rand();
    } else if(sizeof(unsigned int) == sizeof(uint32_t)) {
        //Combine two 32-bit random numbers into 64 bits
        return ((uint64_t)rand()) | (((uint64_t)rand()) << 32);
    }
}

/**
 * Fill a memory with values from a RNG
 */
static void fillMemRandom(char* mem, size_t size, std::mt19937_64& rng) {
    assert(size % 8 == 0);
    for (uint64_t i = 0; i < size; i += 8) {
        uint64_t rand = rng();
        memcpy(mem + i, &rand, 8);
    }
}


int main(int argc, char** argv) {
    //Check arguments
    if(argc < 2) {
        cerr << "Usage: " << argv[0] << " </dev/sdx>" << endl;
    }
    //Seed RNGs
    uint64_t seed = generateSeed();
    randomGen.seed(seed);
    refGen.seed(seed);
    //Open file to read/write to
    int fd = open(argv[1], O_RDWR | O_SYNC);
    if(fd == -1) {
        cerr << "Failed to open file: " << strerror(errno) << endl;
        return 1;
    }
    //Write 4k blocks until the write fails
    const size_t bufsize = 128*512;
    char writeBuffer[bufsize];
    uint64_t writePosition = 0;
    while(true) {
        fillMemRandom(writeBuffer, bufsize, randomGen);
        //We use the POSIX pwrite API instead of libc files for performance reasons here
        if(pwrite(fd, writeBuffer, bufsize, writePosition) == -1) {
            cerr << "Write failed at " << (writePosition / (1024*1024)) << " MiB (block " 
                 << (writePosition / 512) << "): "
                 << strerror(errno) << endl;
            break;
        }
        //Statistics
        if(writePosition % (1024*1024 * 10) == 0) {
            cout << "Wrote " << (writePosition / (1024*1024)) << " MiB..." << endl;
        }
        //Advance write offset
        writePosition += bufsize;
    }
    //Flush file so that data is not cached but physically present on device
    fsync(fd);
    close(fd);
    cout << "Write finished - reading written data" << endl;
    //Reopen buffer
    fd = open(argv[1], O_RDWR);
    if(fd == -1) {
        cerr << "Failed to reopen file: " << strerror(errno) << endl;
        return 1;
    }
    //Read back data
    char readBuffer[bufsize];
    uint64_t compareErrorCount = 0; //Unit: blocks (of size bufsize)
    uint64_t readErrorCount = 0;
    for (uint64_t readPosition = 0; readPosition < writePosition; readPosition += bufsize) {
        //Generate the same random number stream
        fillMemRandom(writeBuffer, bufsize, refGen);
        //Read the actual data
        if(pread(fd, readBuffer, bufsize, readPosition) == -1) {
            cerr << "Read failed at " << (readPosition / (1024*1024)) << " MiB: "
                 << strerror(errno) << endl;
            readErrorCount++;
            continue;
        }
        //Statistics
        if(readPosition % (1024*1024 * 10) == 0) {
            cout << "Read " << (readPosition / (1024*1024)) << " MiB..." << endl;
        }
        //Compare memoriess
        if(memcmp(readBuffer, writeBuffer, bufsize) != 0) {
            //cerr << "Compare error at position " << readPosition << endl;
            compareErrorCount++;
        }
    }
    /**
     * Print statistics
     */
    cout << "\n\n### RESULTS ###" << endl;
    cout << "Apparent size of SD card: " << (writePosition / (1024*1024)) << "MiB\n"
         << "Read errors: " << readErrorCount
         << "\nCompare errors: " << compareErrorCount << " of " << (writePosition / bufsize) << endl;
    //Cleanup
    close(fd);  
    return 0;
}