# diskSpotcheck
Tests the storage device for correctness in reads and writes by issuing random writes with random but deterministic data. Then it reads back those random sectors. It selects random sectors based on a seed given as the input to the function. I use this in testing my storage devices for validity as well as speed.

**WARNING**: This is a destructive test! You **will** lose your data! Only do this on devices you don't care about the contents of.

To compile:
cmake . && make

Each utility has it's own help menu. You can get to it by running with no arguments.

This repository is separated to two groups: block-level and filesystem-level tests.

- blockDeviceTests:
    - diskSpotcheck: Issues pseudo-random writes to pseudo-random locations, then reads back those areas by re-seeding the RNG with the same seed.
        - This will write to a randomly generated list of locations randomly generated data. The data written can be very large which allows for a small value to generate large amounts of repeatable data. I use this to test a storage device to make sure it reads back what I've written to it.
        - Another benefit is that you can use it for performance metrics. Rerunning the program produces the same random data, so you can do a before/after comparison
    - diskSystemTest: Emulates what would happen on a real system with particular parameters. Issues reads/writes to a subset of the disk
        - You can have a whole test sequence specified on the command line. For example:
        
            diskSystemTest -t 10 -p 15 -r 1 -s 90 -c 30 -r 1
            
            Would set number of threads to 10, percent of disk area to read to 15%. Then read
            for 1 minute the specified 15% of the disk, sleep for 90 seconds, then attepmt to
            clear the cache for 30 by issuing random reads. Finally, it would read for 1 minute.
    
- filesystemTests:
    - filesystemTest: Written by a master's student to write a bunch of files to a filesystem then see how long it takes to read them out.
    - fst: Similar to filesystemTest, but using DIRECT file access.
