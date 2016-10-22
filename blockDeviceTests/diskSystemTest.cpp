// Written by: Fekete Andras 2016
#include<iostream>
#include<sstream>
#include<fstream>
#include<cstdlib>
#include<cstdint>
#include<string>
#include<ctime>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>
#include<string.h>
#include<memory>
#include<chrono>
#include<assert.h>
#include<future>
#include<random>
#include <set>
#include "../File.h"
using namespace std;

#define CHUNK_SIZE 4096

//usage syntax
void usage(char *progName) {
	cout << "Usage: " << progName << " <OPTIONS>* <testFilePath>" << endl;
	cout << "OPTIONS:" << endl;
	cout << "\t-w <minutes> => Write for 'minutes' minutes" << endl;
	cout << "\t-r <minutes> => Read for 'minutes' minutes" << endl;
	cout << "\t-p <percent> => Set the percent of disk to be accessed" << endl;
	cout << "\t-t <num>     => Use 'num' threads in transactions" << endl;
	cout << "\t-b           => Set BUFFERED file access mode" << endl;
	cout << "\t-u           => Set UNBUFFERED file access mode" << endl;
	cout << "\t-d           => Set DIRECT file access mode" << endl;
	cout << "Note: Multiple options can be passed multiple times. Such as " << progName << " -w 10 -r 10 -p 10 -r 10" << endl;
	cout << "Chunk size = " << CHUNK_SIZE << endl;
}

typedef struct {
	off64_t offset;
	uint16_t numChunks; // 16bits * CHUNK_SIZE makes max TX size 256MB
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
uint32_t updateLocs(uint8_t percentUtil, vector<TXLocs_t> &locations, char *filename) {
	FileUnbuffered file(filename);
	if(file.getSize() == 0) { cerr << "Can't open file: " << filename << endl; return -1; }

	std::set<TXLocs_t> newSet;
	std::ranlux48_base rngGen(rand());
	uint64_t totalSize = 0;
	uint64_t desiredSize = file.getSize() * percentUtil / 100;
	TXLocs_t nextLoc;
	nextLoc.offset = rngGen() % file.getSize();
	nextLoc.offset -= nextLoc.offset % CHUNK_SIZE; // snap to mem boundary
	nextLoc.numChunks = rngGen();
	newSet.insert(nextLoc);
	uint32_t maxChunks = nextLoc.numChunks;
	while(totalSize < desiredSize) {
		nextLoc.offset = rngGen() % file.getSize();
		nextLoc.offset -= nextLoc.offset % CHUNK_SIZE; // snap to mem boundary
		auto it = newSet.upper_bound(nextLoc);
		auto itBefore = it;
		itBefore--;
		// Overlapping times: taken from http://stackoverflow.com/questions/325933/determine-whether-two-date-ranges-overlap
		if((itBefore->offset <= nextLoc.offset) && (itBefore->offset + itBefore->numChunks * CHUNK_SIZE >= nextLoc.offset)) nextLoc.offset = itBefore->offset + itBefore->numChunks * CHUNK_SIZE;
		nextLoc.numChunks = rngGen();
		if(it->offset <= nextLoc.offset + nextLoc.numChunks*CHUNK_SIZE) nextLoc.numChunks = (it->offset - nextLoc.offset) / CHUNK_SIZE;

		if(nextLoc.numChunks) { newSet.insert(nextLoc); totalSize += nextLoc.numChunks * CHUNK_SIZE; if(maxChunks < nextLoc.numChunks) maxChunks = nextLoc.numChunks; }
	}

	locations.clear();
	for(auto iter : newSet) locations.push_back(iter);
	return maxChunks;
}

template<typename T>
static double toMB(T val) { return (double)val / (1024*1024); }

int main( int argc, char* argv[] ) {
	int opt;

	if(argc < 3) { usage(argv[0]); return 0; }

	vector<TXLocs_t> locations;
	uint32_t maxChunks = updateLocs(10, locations,argv[argc-1]);
	uint8_t numThreads = 10;
	File_t type = FILE_UNBUFFERED;

	while ((opt = getopt(argc-1, argv, "w:r:p:t:bud")) != -1) {
		switch (opt) {
			case 'w': {
					uint8_t minutes = atoi(optarg);
					auto startT = std::chrono::steady_clock::now();
					auto numData = do_test(argv[argc-1],locations,maxChunks,std::chrono::steady_clock::now() + std::chrono::minutes(minutes),false, numThreads,type);
					auto duration = std::chrono::duration_cast<std::chrono::milliseconds >(std::chrono::steady_clock::now() - startT).count() / 1000.0;
					if(numData == 0) { cerr << "Failed write test" << endl; return 1; }
					cout << "Write test complete: " << duration << "s " << toMB(numData*CHUNK_SIZE) << "MB = " << (toMB(numData*CHUNK_SIZE) / duration) << "MB/s" << endl;
				}
				break;
			case 'r': {
					uint8_t minutes = atoi(optarg);
					auto startT = std::chrono::steady_clock::now();
					auto numData = do_test(argv[argc-1],locations,maxChunks,std::chrono::steady_clock::now() + std::chrono::minutes(minutes),true, numThreads,type);
					auto duration = std::chrono::duration_cast<std::chrono::milliseconds >(std::chrono::steady_clock::now() - startT).count() / 1000.0;
					if(numData == 0) { cerr << "Failed read test" << endl; return 1; }
					cout << "Read test complete: " << duration << "s " << toMB(numData*CHUNK_SIZE) << "MB = " << (toMB(numData*CHUNK_SIZE) / duration) << "MB/s" << endl;
				}
				break;
			case 'p': {
					uint8_t percent = atoi(optarg);
					if(percent > 100) { cerr << "Value should be between 0 and 100: " << optarg << endl; break; }
					maxChunks = updateLocs(percent,locations,argv[argc-1]);
				}
				break;
			case 't': numThreads = atoi(optarg); break;
			case 'b': type = FILE_BUFFERED; break;
			case 'u': type = FILE_UNBUFFERED; break;
			case 'd': type = FILE_DIRECT; break;
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

