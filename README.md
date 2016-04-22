# diskSpotcheck
Tests the storage device for correctness in reads and writes by issuing random writes with random but deterministic data. Then it reads back those random sectors. It selects random sectors based on a seed given as the input to the function. I use this in testing my storage devices for validity as well as speed.
