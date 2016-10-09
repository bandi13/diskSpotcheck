/* Test program created by: Fekete, Andras
	 Copyright 2016
	 This program writes a set of random byte sequences in random locations on
	 the nbd disk and then reads them back to make sure they're correctly written.

	 This program is free software: you can redistribute it and/or modify
	 it under the terms of the GNU General Public License as published by
	 the Free Software Foundation, either version 3 of the License, or
	 (at your option) any later version.

	 This program is distributed in the hope that it will be useful,
	 but WITHOUT ANY WARRANTY; without even the implied warranty of
	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	 GNU General Public License for more details.

	 You should have received a copy of the GNU General Public License
	 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#define _LARGEFILE64_SOURCE
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <inttypes.h>
#include <errno.h>
#include <string.h>
#include <chrono>
#include <memory>
#include <vector>

using namespace std;

#include <fstream>
static void dropSystemCache() {
	// Clear cache for benchmarking
	sync();
	std::ofstream ofs("/proc/sys/vm/drop_caches");
	ofs << "3" << std::endl;
	ofs.close();
}

double doPass(std::string &diskPath, char c, uint64_t maxLoc, size_t bufSize, bool readOnly, uint32_t locCnt) {
	std::unique_ptr<char[]> raiiBuf = std::make_unique<char[]>(bufSize);
	char *buf = raiiBuf.get();
	std::vector<uint64_t> locs(locCnt);

	dropSystemCache();
	srand(c);
	for(uint64_t i = 0; i < bufSize; i++) buf[i] = rand();
	maxLoc -= bufSize; // make sure we don't accidentally try to write off the end of the file
	locs[0] = 0; // make sure we get the beginning
	locs[locCnt - 1] = maxLoc; // make sure we get the end
	for(uint64_t i = 1; i < locCnt - 1; i++) {
		locs[i] = (((float)rand() / RAND_MAX) * (maxLoc - locs[i-1] - bufSize)) / ((locCnt - 2)/4) + locs[i-1] + bufSize; // set up the location to be written relative to the last one
	}
	cout << "Starting test of char=" << c << endl;
	auto startT = std::chrono::steady_clock::now();
	int fd;
	if((fd = open(diskPath.c_str(),O_RDWR|O_LARGEFILE)) == -1) return -1;
	if(!readOnly) {
		for(uint64_t i = 0; i < locCnt; i++) {
			//		cout << i << ": Writing to " << locs[i] << endl;
			lseek64(fd,locs[i],SEEK_SET);
			if(write(fd,buf,bufSize) != (int)bufSize) { cerr << "Didn't complete a write of " << bufSize << " * '" << c << "' at " << locs[i] << " because " << strerror(errno) << endl; return -1; }
		}
		if(syncfs(fd) == -1) { cerr << "Sync error: " << strerror(errno) << endl; return -3; }
		dropSystemCache();
	}
	for(uint64_t i = 0; i < locCnt; i++) {
		//		cout << i << ": Reading from " << locs[i] << endl;
		lseek64(fd,locs[i],SEEK_SET);
		if(read(fd,buf,bufSize) != (int)bufSize) { cerr << "Didn't complete a read of " << bufSize << " * '" << c << "' at " << locs[i] << " because " << strerror(errno) << endl; return -3; }
		srand(c);
		char rng;
		for(uint64_t j = 0; j < bufSize; j++)
			if(buf[j] != (rng = rand())) {
				cerr << "Verification of write/read failed at location " << locs[i] << ", offset=" << j << endl;
				cerr << "  expected=";
				for(uint64_t k = 0; k < bufSize; k++) { cerr << (int)rng << ','; rng = rand(); }
				cerr << endl;
				cerr << "       got=";
				for(uint64_t k = 0; k < bufSize; k++) cerr << (int)buf[k] << ',';
				cerr << endl;
				return -4;
			}
	}
	close(fd);
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds >(std::chrono::steady_clock::now() - startT).count() / 1000.0;
	double speed = ((double)(bufSize*locCnt)/duration)/(1024*1024);
	cout << "Test completed in " << duration << " seconds. Speed= " << speed << " MB/s." << endl;
	return speed;
}

#define doUsage(errStream) { cerr << errStream << endl << "Usage: " << argv[0] << " [-d <device=/dev/nbd0>] [-s <diskSizeInMB=auto>] [-b <bufSizeInKB=64>] [-l <locCount=1000>] [-p <numPasses=3>] [-h] [-r]" << endl; return -1; }
int main(int argc, char *argv[]) {
	int opt;
	bool readOnly = false;
	size_t diskSize = 0;
	size_t bufSize = 64*1024;
	uint32_t locCnt = 1000;
	uint8_t numPasses = 3;
	std::string diskPath = "/dev/nbd0";
	while ((opt = getopt(argc, argv, "b:d:s:l:p:rh")) != -1) {
		switch (opt) {
			case 'b': bufSize = (size_t)atoi(optarg) * 1024; break;
			case 'd': diskPath = optarg; break;
			case 's': diskSize = (size_t)atoi(optarg) * 1024 * 1024; break;
			case 'l': locCnt = (uint32_t)atoi(optarg); break;
			case 'p': numPasses = (uint8_t)atoi(optarg); break;
			case 'r': readOnly = true; break;
			case 'h': doUsage("Help requested"); return -1;
			default:  doUsage("Unknown argument"); return -1;
		}
	}
	if(optind < argc) { doUsage("Unknown argument: " << argv[optind]); return -1; }
	if(locCnt == 0) doUsage("locCount must be non-zero");
	if(numPasses == 0) doUsage("numPasses must be non-zero");
	if(numPasses > 24) doUsage("numPasses must be less than 24...because I said so.");
	{
		int fd;
		if((fd = open(diskPath.c_str(),O_RDWR|O_LARGEFILE)) == -1) doUsage("Error opening " << diskPath << ": " << strerror(errno));
		if(diskSize == 0) {
			struct stat fd_stat;
			if(fstat(fd,&fd_stat)) doUsage("Error getting the stats on the file: " << strerror(errno));
			if(S_ISBLK(fd_stat.st_mode)) ioctl(fd,BLKGETSIZE64, &diskSize);
			else {
				cout << "File stats: blksize=" << fd_stat.st_blksize << " size=" << fd_stat.st_size << endl;
				diskSize = fd_stat.st_size;
			}
		}
		close(fd);
	}

	cout << "Setting diskSize=" << diskSize / (1024*1024.0) << "MB, bufSize=" << bufSize << endl;

	if(diskSize < bufSize) doUsage( "DiskSize<"<<(uint64_t)bufSize<<", we can't deal with that.");

	if(readOnly) cout << "Will be reading " << bufSize * locCnt / (1024*1024.0) << "MB" << endl;
	else cout << "Will be writing+reading " << bufSize * locCnt / (1024*1024.0) << "MB" << endl;

	double curSpeed, totSpeed = 0;
	auto startT = std::chrono::steady_clock::now();
	if(readOnly) {
		if((totSpeed = doPass(diskPath,'a'+numPasses-1,diskSize,bufSize,readOnly,locCnt)) < 0) { cerr << "Failed a test" << endl; return -1; }
	} else {
		for(int i = 0; i < numPasses; i++) {
			if((curSpeed = doPass(diskPath,'a'+i,diskSize,bufSize,readOnly,locCnt)) < 0) { cerr << "Failed a test" << endl; return -1; }
			totSpeed += curSpeed;
		}
	}
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds >(std::chrono::steady_clock::now() - startT).count() / 1000.0;
	cout << "All tests completed in " << duration << " seconds. Average speed=" << (totSpeed / numPasses) << "MB/s." << endl;
	return 0;
}
