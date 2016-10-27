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
#include <assert.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <ctime>
#include <algorithm>
#include <unistd.h>
#include <string.h>
#include <memory>
#include <chrono>
#include <future>
#include <set>
#include "../File.h"
using namespace std;

#define CHUNK_SIZE 4096

template<typename T>
static double toMB(T val) { return (double)val / (1024*1024); }

//usage syntax
void usage(char *progName) {
	cout << "Usage: " << progName << " <OPTIONS>* <testFilePath>" << endl;
	cout << "OPTIONS:" << endl;
	cout << "\t-c <sizeInMB>=> Do random reads until 'sizeInMB' has been read to clear caches" << endl;
	cout << "\t-w <minutes> => Write for 'minutes' minutes" << endl;
	cout << "\t-r <minutes> => Read for 'minutes' minutes" << endl;
	cout << "\t-p <percent> => Set the percent of disk to be accessed" << endl;
	cout << "\t-P <percent> => Update 'percent'% of the access locations" << endl;
	cout << "\t-t <num>     => Use 'num' threads in transactions" << endl;
	cout << "\t-b           => Set BUFFERED file access mode" << endl;
	cout << "\t-u           => Set UNBUFFERED file access mode" << endl;
	cout << "\t-d           => Set DIRECT file access mode" << endl;
	cout << "\t-s <seconds> => Sleep until 'seconds' seconds" << endl;
	cout << "Note: Multiple options can be passed multiple times. Such as " << progName << " -w 10 -r 10 -p 10.5 -r 10" << endl;
	cout << "Note: Percent can be 0 to 100 inclusive, but I would not go past 80%. The way the locations get generated may take a long time to complete." << endl;
	cout << "Chunk size = " << CHUNK_SIZE << endl;
}

typedef struct {
	off64_t offset;
	uint8_t numChunks; // 8bits * CHUNK_SIZE makes max TX size 1MB
} TXLocs_t;
inline bool operator<(const TXLocs_t& lhs, const TXLocs_t& rhs) { return lhs.offset < rhs.offset; }

struct voidPtrDeleter { void operator()(void *p) { free(p); } };

int64_t do_file(File *file, const vector<TXLocs_t>& locations, const uint32_t maxChunks, const std::chrono::steady_clock::time_point endTime, bool isRead) {
	void *testMem;
	if(posix_memalign(&testMem, 4096, maxChunks*CHUNK_SIZE)) { cerr << "Failed aligning memory" << strerror(errno) << endl; return -1; }
	unique_ptr<void,voidPtrDeleter> testPtr(testMem); // make smart ptr remember to free the memory
	if(file->getSize() == 0) { cerr << "error opening file" << endl; return -1; }
	std::ranlux48_base rngGen(rand());
	uint64_t vectIdx;
	uint64_t chunksWritten = 0;
	while(std::chrono::steady_clock::now() < endTime) {
		vectIdx = rngGen() % locations.size();
		if(isRead) {
			if (file->read((char*)testMem, (ssize_t)locations[vectIdx].numChunks * CHUNK_SIZE, locations[vectIdx].offset) != ((ssize_t)locations[vectIdx].numChunks * CHUNK_SIZE))
				{ cerr << "error: " << strerror(errno) << endl; return -1; }
		} else {
			if (file->write((char*)testMem, (ssize_t)locations[vectIdx].numChunks * CHUNK_SIZE, locations[vectIdx].offset) != ((ssize_t)locations[vectIdx].numChunks * CHUNK_SIZE))
				{ cerr << "error: " << strerror(errno) << endl; return -1; }
		}
		chunksWritten += locations[vectIdx].numChunks;
	}
	return chunksWritten;
}

typedef enum { FILE_DIRECT, FILE_BUFFERED, FILE_UNBUFFERED } File_t;
uint64_t do_test( const char* path, const vector<TXLocs_t>& locations, const uint32_t maxChunks, const std::chrono::steady_clock::time_point endTime, bool isRead, uint16_t numThread, File_t type ) {
	std::vector<std::future<int64_t>> procs;
	switch(type) {
		case FILE_DIRECT:
			for(uint16_t i = 0; i < numThread; i++ ) procs.push_back(std::async(std::launch::async,[&]() { FileDirect myFile(path); return do_file(&myFile,locations,maxChunks,endTime,isRead); } ));
			break;
		case FILE_BUFFERED:
			for(uint16_t i = 0; i < numThread; i++ ) procs.push_back(std::async(std::launch::async,[&]() { FileBuffered myFile(path); return do_file(&myFile,locations,maxChunks,endTime,isRead); } ));
			break;
		case FILE_UNBUFFERED:
			for(uint16_t i = 0; i < numThread; i++ ) procs.push_back(std::async(std::launch::async,[&]() { FileUnbuffered myFile(path); return do_file(&myFile,locations,maxChunks,endTime,isRead); } ));
			break;
	}
	int64_t curRet;
	uint64_t total = 0;
	for(uint16_t i = 0; i < numThread; i++ ) {
		curRet = procs[i].get();
		if(curRet == -1) return 0;
		total += curRet;
	}
	return total;
}

uint8_t addLocToSet(std::set<TXLocs_t> &newSet, std::ranlux48_base &rngGen, uint64_t fileSize) {
	TXLocs_t nextLoc;
	nextLoc.offset = rngGen() % fileSize;
	nextLoc.offset -= nextLoc.offset % CHUNK_SIZE; // snap to mem boundary
	nextLoc.numChunks = rngGen();
	if(newSet.size()) {
		auto it = newSet.upper_bound(nextLoc);
		auto itBefore = it;
		itBefore--;
		// Overlapping times: taken from http://stackoverflow.com/questions/325933/determine-whether-two-date-ranges-overlap
		if ((itBefore->offset <= nextLoc.offset) && (itBefore->offset + itBefore->numChunks * CHUNK_SIZE > nextLoc.offset)) nextLoc.offset = itBefore->offset + itBefore->numChunks * CHUNK_SIZE;
		if (it->offset <= nextLoc.offset + nextLoc.numChunks * CHUNK_SIZE) nextLoc.numChunks = (it->offset - nextLoc.offset) / CHUNK_SIZE;
	}
	if(nextLoc.numChunks) newSet.insert(nextLoc);
	return nextLoc.numChunks;
}

uint8_t generateLocs(double percentUtil, vector<TXLocs_t> &locations, uint64_t fileSize) {
	std::set<TXLocs_t> newSet;
	std::ranlux48_base rngGen(1);
	uint64_t totalSize = 0;
	uint64_t desiredSize = fileSize * percentUtil / 100;
	uint8_t maxChunks = 0;
	uint8_t lastNumChunks;
	while(totalSize < desiredSize) {
		lastNumChunks = addLocToSet(newSet,rngGen,fileSize);
		totalSize += lastNumChunks * CHUNK_SIZE;
		if(maxChunks < lastNumChunks) maxChunks = lastNumChunks;
	}

	locations.clear();
	locations.reserve(newSet.size());
	for(auto iter : newSet) locations.push_back(iter);

//	for(auto iter : newSet) cout << '(' << iter.offset << ',' << (int)iter.numChunks << ')' << endl;
//	cout << "Desired %: " << (int)percentUtil << " actual %: " << 100*totalSize / fileSize << endl;
	return maxChunks;
}

uint8_t updateLocs(double percentChange, vector<TXLocs_t> &locations, uint64_t fileSize) {
	std::set<TXLocs_t> newSet;
	std::ranlux48_base rngGen(rand());
	uint8_t maxChunks = 0;
	uint8_t lastNumChunks;
	auto it = locations.begin();
	while(it != locations.end()) {
		if((uint64_t)100 * rand() / RAND_MAX < percentChange) {
			while((lastNumChunks = addLocToSet(newSet,rngGen,fileSize)) == 0); // try until we add one
		} else { lastNumChunks = it->numChunks; newSet.insert(*it); }
		if(maxChunks < lastNumChunks) maxChunks = lastNumChunks;
		it++;
	}

	locations.clear();
	locations.reserve(newSet.size());
	for(auto iter : newSet) locations.push_back(iter);

	return maxChunks;
}

int64_t cacheClear(const char *filename, const std::chrono::steady_clock::time_point endTime) {
	FileBuffered file(filename);
	if(file.getSize() == 0) { cerr << "Can't open file: " << filename << endl; return -1; }
	unsigned int maxChunks = 15;
	void *testMem;
	if(posix_memalign(&testMem, 4096, maxChunks*CHUNK_SIZE)) { cerr << "Failed aligning memory" << strerror(errno) << endl; return -1; }
	unique_ptr<void,voidPtrDeleter> testPtr(testMem); // make smart ptr remember to free the memory

	std::ranlux48_base rngGen(rand());
	uint64_t sizeRead = 0;
	while(std::chrono::steady_clock::now() < endTime) {
		TXLocs_t nextLoc;
		nextLoc.offset = rngGen() % file.getSize();
		nextLoc.offset -= nextLoc.offset % CHUNK_SIZE; // snap to mem boundary
		nextLoc.numChunks = rngGen() % maxChunks;

		if(file.read((char*)testMem,nextLoc.numChunks*CHUNK_SIZE,nextLoc.offset) != (nextLoc.numChunks*CHUNK_SIZE)) {
			cerr << "Error in cache clearing read: " << strerror(errno) << endl;
			return -1;
		}
		sizeRead += nextLoc.numChunks*CHUNK_SIZE;
	}

	return sizeRead;
}

int main( int argc, char* argv[] ) {
	int opt;

	if(argc < 3) { usage(argv[0]); return 0; }
	uint64_t fileSize = 0;
	{
		FileUnbuffered file(argv[argc-1]);
		if (file.getSize() == 0) { cerr << "Can't open file: " << argv[argc-1] << endl; return 0; }
		fileSize = file.getSize();
	}

	vector<TXLocs_t> locations;
	uint32_t maxChunks = generateLocs(10, locations, fileSize);
	uint8_t numThreads = 10;
	File_t type = FILE_UNBUFFERED;

	while ((opt = getopt(argc-1, argv, "c:w:r:p:P:t:buds:")) != -1) {
		switch (opt) {
			case 'c': {
					uint8_t seconds = atoi(optarg);
					cout << "Cache clear for " << (int)seconds << "s..." << flush;
					auto sizeRead = cacheClear(argv[argc-1],std::chrono::steady_clock::now() + std::chrono::seconds(seconds));
					if(sizeRead == -1) { cerr << "Failed cache clear" << endl; return 1; }
					cout << "done: " << toMB(sizeRead) << "MB read" << endl;
				}
				break;
			case 'w': {
					uint8_t minutes = atoi(optarg);
					cout << "Write test for " << (int)minutes << "min..." << flush;
					auto startT = std::chrono::steady_clock::now();
					auto numData = do_test(argv[argc-1],locations,maxChunks,std::chrono::steady_clock::now() + std::chrono::minutes(minutes),false, numThreads,type);
					auto duration = std::chrono::duration_cast<std::chrono::milliseconds >(std::chrono::steady_clock::now() - startT).count() / 1000.0;
					if(numData == 0) { cerr << "Failed write test" << endl; return 1; }
					cout << "done: " << toMB(numData*CHUNK_SIZE) << "MB / " << duration << "s = " << (toMB(numData*CHUNK_SIZE) / duration) << "MB/s" << endl;
				}
				break;
			case 'r': {
					uint8_t minutes = atoi(optarg);
					cout << "Read test for " << (int)minutes << "min..." << flush;
					auto startT = std::chrono::steady_clock::now();
					auto numData = do_test(argv[argc-1],locations,maxChunks,std::chrono::steady_clock::now() + std::chrono::minutes(minutes),true, numThreads,type);
					auto duration = std::chrono::duration_cast<std::chrono::milliseconds >(std::chrono::steady_clock::now() - startT).count() / 1000.0;
					if(numData == 0) { cerr << "Failed read test" << endl; return 1; }
					cout << "done: " << toMB(numData*CHUNK_SIZE) << "MB / " << duration << "s = " << (toMB(numData*CHUNK_SIZE) / duration) << "MB/s" << endl;
				}
				break;
			case 'p': {
					double percent = atof(optarg);
					cout << "Regenerating locations to " << percent << "%..." << flush;
					if((percent < 0) || (percent > 100)) { cerr << "Percent value should be between 0 and 100: " << optarg << endl; break; }
					maxChunks = generateLocs(percent, locations, fileSize);
					cout << "done: maxChunks = " << (int)maxChunks << endl;
				}
				break;
			case 'P': {
					double percent = atof(optarg);
					cout << "Adjusting locations to " << percent << "%..." << flush;
					if((percent < 0) || (percent > 100)) { cerr << "Percent value should be between 0 and 100: " << optarg << endl; break; }
					maxChunks = updateLocs(percent, locations, fileSize);
					cout << "done: maxChunks = " << (int)maxChunks << endl;
				}
				break;
			case 't': numThreads = atoi(optarg); break;
			case 'b': type = FILE_BUFFERED; break;
			case 'u': type = FILE_UNBUFFERED; break;
			case 'd': type = FILE_DIRECT; break;
			case 's': {
					int seconds = atoi(optarg);
					cout << "Sleep for " << (int)seconds << "s..." << flush;
					std::this_thread::sleep_for(std::chrono::seconds(seconds));
					cout << "done" << endl;
				}
				break;
			default: usage(argv[0]); break;
		}
	}

	if(optind < argc - 1) {
		cout << "Unknown parameter: " << argv[optind] << endl;
		usage(argv[0]);
		return 1;
	}

	cout << "Test completed successfully" << endl;
	return 0;
}
