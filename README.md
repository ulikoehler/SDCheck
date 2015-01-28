# SDCheck

SDCheck is a simple command line utility to check arbitrarily-sized SD/microSD cards for defect blocks.
Its primary use is to check if cheap SD cards advertise a size greater than their actual storage capacity.

### Quick start

In order to build SDCheck, you need to have a C++11-compatible compiler and a POSIX-standard compatible system.
In particular, your compiler and C++ standard library needs to support C++11 random number generation while your system API needs to support file descriptor based IO, particularly `pread()` and `pwrite()`.

To build SDCheck, use 
```
make
```

In order to use it, you need to know the device name of your SD card. Double-check that you don't mix up device files (you could overwrite your hard disk) and the SD card contains nothing valuable. SDCheck will destroy
```
sudo ./sdcheck /dev/sde
```
Root access is required due to the access to the raw device file.

### Algorithms

SDCheck uses a constant-space algorithm based on dual random number generator. In order to facilitate fast random number generation, SDCheck uses the C++11 `std::mt19937_64` generator which should be faster on 64-bit systems. However, compatibility methods for 32-bit systems are provided.

Both MT19937 RNGs are seeded by the same value obtained by extracting 64 bits from `std::random_device`, effectively producing a hard-to-predict random number stream assuming sufficient entropy is present in the the system RNG.

The first MT19937 RNG is used to generate a stream of 32kiB blocks that are written to the SD card using the POSIX `pwrite()` API.
Data is written to the SD card until an error occurs. The number of megabytes that could be written is used to estimate the apparent size of the SD card.

After finishing to write the random data stream to the card, it is read back block-by-block while the second RNG produces a stream of reference values. Due to both RNGs being seeded by the same value, this allows SDCheck to compare every single byte on the SD card without having to store the random number stream externally.

### Licensing

SDCheck is distributed under [Apache License v2.0](https://www.apache.org/licenses/LICENSE-2.0) and in the hope that it will be useful.