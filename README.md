# diskSpotcheck
Tests the storage device for correctness in reads and writes by issuing random writes with random but deterministic data. Then it reads back those random sectors. It selects random sectors based on a seed given as the input to the function. I use this in testing my storage devices for validity as well as speed.

>>> WARNING: This is a destructive test! You *will* lose your data! Only do this on devices you don't care about the contents of. <<<

To compile:
g++ diskSpotcheck.cpp -o diskSpotcheck -std=c++11

To run:
./diskSpotcheck /dev/sda
