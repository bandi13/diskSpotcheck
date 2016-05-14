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

using namespace std;

#include <fstream>
static void dropSystemCache() {
	// Clear cache for benchmarking
	sync();
	std::ofstream ofs("/proc/sys/vm/drop_caches");
	ofs << "3" << std::endl;
	ofs.close();
}

#define LOCCNT 1000
#define NUMPASSES 3
double doPass(int fd, char c, uint64_t maxLoc, size_t bufSize) {
	std::unique_ptr<char[]> raiiBuf = std::make_unique<char[]>(bufSize);
	char *buf = raiiBuf.get();
	uint64_t locs[LOCCNT];

	dropSystemCache();
	srand(c);
	for(uint64_t i = 0; i < bufSize; i++) buf[i] = rand();
	cout << "Starting test of char=" << c << endl;
	maxLoc -= bufSize; // make sure we don't accidentally try to write off the end of the file
	locs[0] = 0; // make sure we get the beginning
	locs[LOCCNT - 1] = maxLoc; // make sure we get the end
	for(uint64_t i = 1; i < LOCCNT - 1; i++) {
		locs[i] = (((float)rand() / RAND_MAX) * (maxLoc - locs[i-1] - bufSize)) / ((LOCCNT - 2)/4) + locs[i-1] + bufSize; // set up the location to be written relative to the last one
	}
	auto startT = std::chrono::steady_clock::now();
	for(uint64_t i = 0; i < LOCCNT; i++) {
		//		cout << i << ": Writing to " << locs[i] << endl;
		lseek64(fd,locs[i],SEEK_SET);
		if(write(fd,buf,bufSize) != (int)bufSize) { cerr << "Didn't complete a write of " << bufSize << " * '" << c << "' at " << locs[i] << " because " << strerror(errno) << endl; return -1; }
	}
	if(syncfs(fd) == -1) { cerr << "Sync error: " << strerror(errno) << endl; return -3; }
	dropSystemCache();
	for(uint64_t i = 0; i < LOCCNT; i++) {
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
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds >(std::chrono::steady_clock::now() - startT).count() / 1000.0;
	double speed = ((double)(bufSize*LOCCNT)/duration)/(1024*1024);
	cout << "Test completed in " << duration << " seconds. Speed=" << speed << "MB/s." << endl;
	return speed;
}

#define doUsage(errStream) { cerr << errStream << endl << "Usage: " << argv[0] << " [-d <device>] [-s <diskSizeInMB>] [-b <bufSizeInKB>] [-h]" << endl; return -1; }
int main(int argc, char *argv[]) {
	int fd;
	int opt;
	size_t diskSize = 0;
	size_t bufSize = 64*1024;
	std::string diskPath = "/dev/nbd0";
	while ((opt = getopt(argc, argv, "b:d:s:h")) != -1) {
		switch (opt) {
			case 'b': bufSize = (size_t)atoi(optarg) * 1024; break;
			case 's': diskSize = (size_t)atoi(optarg) * 1024 * 1024; break;
			case 'd': diskPath = optarg; break;
			case 'h': doUsage("Help requested"); return -1;
			default:  doUsage("Unknown argument"); return -1;
		}
	}
	if((fd = open(diskPath.c_str(),O_RDWR|O_LARGEFILE)) == -1) doUsage("Error opening " << diskPath << ": " << strerror(errno));
	if(diskSize == 0) {
		struct stat fd_stat;
		if(fstat(fd,&fd_stat)) doUsage("Error getting the stats on the file: " << strerror(errno));
		if(S_ISBLK(fd_stat.st_mode)) ioctl(fd,BLKGETSIZE64, &diskSize);
		else {
			cout << "File stats: blksize=" << fd_stat.st_blksize << " blkcnt=" << fd_stat.st_blocks << endl;
			diskSize = fd_stat.st_blksize * fd_stat.st_blocks;
		}
	}

	cout << "Setting diskSize=" << diskSize << ", bufSize=" << bufSize << endl;

	if(diskSize < bufSize) doUsage( "DiskSize<"<<(uint64_t)bufSize<<", we can't deal with that.");

	double curSpeed, totSpeed = 0;
	auto startT = std::chrono::steady_clock::now();
	for(int i = 0; i < NUMPASSES; i++) {
		if((curSpeed = doPass(fd,'a'+i,diskSize,bufSize)) < 0) { cerr << "Failed a test" << endl; return -1; }
		totSpeed += curSpeed;
	}
	close(fd);
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds >(std::chrono::steady_clock::now() - startT).count() / 1000.0;
	cout << "All tests completed in " << duration << " seconds. Average speed=" << (totSpeed / NUMPASSES) << "MB/s." << endl;
	return 0;
}
