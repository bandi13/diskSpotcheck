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

struct voidPtrDeleter { void operator()(void *p) { free(p); } };

class Test {
public:
	Test(const char *fileName) {
		FileBuffered file(fileName);
		if(file.getSize() == 0) { cerr << "error opening file" << endl; return; }
		fname = fileName;
		fSize = file.getSize();
	}
	~Test() {}
	virtual int64_t do_file(File *file, const std::chrono::steady_clock::time_point endTime, bool isRead) = 0;
	typedef enum { FILE_DIRECT, FILE_BUFFERED, FILE_UNBUFFERED } File_t;
	virtual std::string do_testString(const std::chrono::steady_clock::time_point endTime, bool isRead, uint16_t numThread, File_t type) = 0;
	virtual void generateLocs(double percentUtil) = 0;
	virtual void updateLocs(double percentChange) = 0;
	int64_t cacheClear(const std::chrono::steady_clock::time_point endTime) {
		FileBuffered file(fname.c_str());
		if(file.getSize() == 0) { cerr << "Can't open file: " << fname << endl; return -1; }
		const unsigned int maxChunks = 15;
		void *testMem;
		if(posix_memalign(&testMem, 4096, maxChunks*CHUNK_SIZE)) { cerr << "Failed aligning memory" << strerror(errno) << endl; return -1; }
		unique_ptr<void,voidPtrDeleter> testPtr(testMem); // make smart ptr remember to free the memory

		std::ranlux48_base rngGen(rand());
		uint64_t sizeRead = 0;
		while(std::chrono::steady_clock::now() < endTime) {
			off64_t offset = rngGen() % file.getSize();
			offset -= offset % CHUNK_SIZE; // snap to mem boundary
			uint8_t numChunks = rngGen() % maxChunks;

			if(file.read((char*)testMem,numChunks*CHUNK_SIZE,offset) != (numChunks*CHUNK_SIZE)) {
				cerr << "Error in cache clearing read: " << strerror(errno) << endl;
				return -1;
			}
			sizeRead += numChunks*CHUNK_SIZE;
		}

		return sizeRead;
	}
protected:
	std::string fname;
	uint64_t fSize;

	uint64_t do_test(const std::chrono::steady_clock::time_point endTime, bool isRead, uint16_t numThread, File_t type ) {
		std::vector<std::future<int64_t>> procs;
		switch(type) {
			case FILE_DIRECT:
				for(uint16_t i = 0; i < numThread; i++ ) procs.push_back(std::async(std::launch::async,[&]() { FileDirect myFile(fname.c_str()); return do_file(&myFile,endTime,isRead); } ));
				break;
			case FILE_BUFFERED:
				for(uint16_t i = 0; i < numThread; i++ ) procs.push_back(std::async(std::launch::async,[&]() { FileBuffered myFile(fname.c_str()); return do_file(&myFile,endTime,isRead); } ));
				break;
			case FILE_UNBUFFERED:
				for(uint16_t i = 0; i < numThread; i++ ) procs.push_back(std::async(std::launch::async,[&]() { FileUnbuffered myFile(fname.c_str()); return do_file(&myFile,endTime,isRead); } ));
				break;
		}
		int64_t curRet;
		uint64_t total = 0;
		for(uint16_t i = 0; i < numThread; i++ ) {
			curRet = procs[i].get();
			if(curRet == -1) return 0;
			total += curRet;
		}
		return total / numThread;
	}
};

class Test_Throughput : public Test {
public:
	Test_Throughput(const char *fileName) : Test(fileName) { }

	int64_t do_file(File *file, const std::chrono::steady_clock::time_point endTime, bool isRead) override {
		if (file->getSize() == 0) { cerr << "error opening file" << endl; return -1; }
		std::ranlux48_base rngGen(rand());
		uint64_t vectIdx;
		uint64_t chunksWritten = 0;
		auto startTime = std::chrono::steady_clock::now();
		while (std::chrono::steady_clock::now() < endTime) {
			vectIdx = rngGen() % locations.size();
			if (isRead) {
				if (file->read((char *) testPtr.get(), (ssize_t) locations[vectIdx].numChunks * CHUNK_SIZE, locations[vectIdx].offset) != ((ssize_t) locations[vectIdx].numChunks * CHUNK_SIZE))
					{ cerr << "error: " << strerror(errno) << endl; return -1; }
			} else {
				if (file->write((char *) testPtr.get(), (ssize_t) locations[vectIdx].numChunks * CHUNK_SIZE, locations[vectIdx].offset) != ((ssize_t) locations[vectIdx].numChunks * CHUNK_SIZE))
					{ cerr << "error: " << strerror(errno) << endl; return -1; }
			}
			chunksWritten += locations[vectIdx].numChunks;
		}
		return (chunksWritten*CHUNK_SIZE) / (std::chrono::duration_cast<std::chrono::milliseconds >(std::chrono::steady_clock::now() - startTime).count() / 1000.0);
	}

	virtual std::string do_testString(const std::chrono::steady_clock::time_point endTime, bool isRead, uint16_t numThread, File_t type) {
		auto ret = do_test(endTime,isRead,numThread,type);
		if(ret) return ret + "MB/s";
		return "Failed";
	}

	void generateLocs(double percentUtil) override {
		std::set<TXLocs_t> newSet;
		std::ranlux48_base rngGen(1);
		uint64_t totalSize = 0;
		uint64_t desiredSize = fSize * percentUtil / 100;
		while (totalSize < desiredSize) totalSize += addLocToSet(newSet, rngGen, fSize) * CHUNK_SIZE;

		refreshLocs(newSet);

//	for(auto iter : newSet) cout << '(' << iter.offset << ',' << (int)iter.numChunks << ')' << endl;
//	cout << "Desired %: " << (int)percentUtil << " actual %: " << 100*totalSize / fileSize << endl;
	}

	void updateLocs(double percentChange) override {
		std::set<TXLocs_t> newSet;
		std::ranlux48_base rngGen(rand());
		auto it = locations.begin();
		while (it != locations.end()) {
			if ((uint64_t) 100 * rand() / RAND_MAX < percentChange) {
				while (addLocToSet(newSet, rngGen, fSize) == 0); // try until we add one
			} else newSet.insert(*it);
			it++;
		}

		refreshLocs(newSet);
	}

protected:
	class TXLocs_t {
		public:
			off64_t offset;
			uint8_t numChunks; // 8bits * CHUNK_SIZE makes max TX size 1MB
			bool operator<(const TXLocs_t& rhs) const { return this->offset < rhs.offset; }
	};
	std::vector<TXLocs_t> locations;
	uint8_t maxChunks = 0;
	unique_ptr<void, voidPtrDeleter> testPtr; // make smart ptr remember to free the memory

	virtual uint8_t addLocToSet(std::set<TXLocs_t> &newSet, std::ranlux48_base &rngGen, uint64_t fileSize) {
		TXLocs_t nextLoc;
		nextLoc.offset = rngGen() % fileSize;
		nextLoc.offset -= nextLoc.offset % CHUNK_SIZE; // snap to mem boundary
		nextLoc.numChunks = rngGen();
		if (newSet.size()) {
			auto it = newSet.upper_bound(nextLoc);
			auto itBefore = it;
			itBefore--;
			// Overlapping times: taken from http://stackoverflow.com/questions/325933/determine-whether-two-date-ranges-overlap
			if ((itBefore->offset <= nextLoc.offset) && (itBefore->offset + itBefore->numChunks * CHUNK_SIZE > nextLoc.offset))
				nextLoc.offset = itBefore->offset + itBefore->numChunks * CHUNK_SIZE;
			if (it->offset <= nextLoc.offset + nextLoc.numChunks * CHUNK_SIZE)
				nextLoc.numChunks = (it->offset - nextLoc.offset) / CHUNK_SIZE;
		}
		if (nextLoc.numChunks) newSet.insert(nextLoc);
		return nextLoc.numChunks;
	}

	void refreshLocs(const std::set<TXLocs_t> &newSet) {
		locations.clear();
		locations.reserve(newSet.size());
		maxChunks = 0;
		for (auto iter : newSet) { locations.push_back(iter); if(maxChunks < iter.numChunks) maxChunks = iter.numChunks; }
		void *testMem;
		if (posix_memalign(&testMem, 4096, maxChunks * CHUNK_SIZE)) { cerr << "Failed aligning memory" << strerror(errno) << endl; return; }
		testPtr = std::unique_ptr<void,voidPtrDeleter>(testMem);
	}
};

class Test_ResponseTime : public Test_Throughput {
public:
	Test_ResponseTime(const char *fileName, uint8_t numChunks) : Test_Throughput(fileName), numChunks(numChunks) { }

	int64_t do_file(File *file, const std::chrono::steady_clock::time_point endTime, bool isRead) override {
		if (file->getSize() == 0) { cerr << "error opening file" << endl; return -1; }
		std::ranlux48_base rngGen(rand());
		uint64_t vectIdx = rngGen() % locations.size();
		uint64_t chunksWritten = 0;
		uint64_t numTX = 0;
		auto startTime = std::chrono::steady_clock::now();
		while ((startTime = std::chrono::steady_clock::now()) < endTime) {
			if (isRead) {
				if (file->read((char *) testPtr.get(), (ssize_t) locations[vectIdx].numChunks * CHUNK_SIZE, locations[vectIdx].offset) != ((ssize_t) locations[vectIdx].numChunks * CHUNK_SIZE))
				{ cerr << "error: " << strerror(errno) << endl; return -1; }
			} else {
				if (file->write((char *) testPtr.get(), (ssize_t) locations[vectIdx].numChunks * CHUNK_SIZE, locations[vectIdx].offset) != ((ssize_t) locations[vectIdx].numChunks * CHUNK_SIZE))
				{ cerr << "error: " << strerror(errno) << endl; return -1; }
			}
			chunksWritten += std::chrono::duration_cast<std::chrono::nanoseconds >(std::chrono::steady_clock::now() - startTime).count() / 1000.0;
			numTX++;
			vectIdx = rngGen() % locations.size();
		}
		return chunksWritten / numTX;
	}

	virtual std::string do_testString(const std::chrono::steady_clock::time_point endTime, bool isRead, uint16_t numThread, File_t type) {
		auto ret = do_test(endTime,isRead,numThread,type);
		if(ret) return ret + "us";
		return "Failed";
	}


protected:
	uint8_t numChunks;

	uint8_t addLocToSet(std::set<TXLocs_t> &newSet, std::ranlux48_base &rngGen, uint64_t fileSize) override {
		TXLocs_t nextLoc;
		nextLoc.offset = rngGen() % fileSize;
		nextLoc.offset -= nextLoc.offset % CHUNK_SIZE; // snap to mem boundary
		nextLoc.numChunks = numChunks;
		if (newSet.size()) {
			auto it = newSet.upper_bound(nextLoc);
			auto itBefore = it;
			itBefore--;
			// Overlapping times: taken from http://stackoverflow.com/questions/325933/determine-whether-two-date-ranges-overlap
			if ((itBefore->offset <= nextLoc.offset) && (itBefore->offset + itBefore->numChunks * CHUNK_SIZE > nextLoc.offset))
				nextLoc.offset = itBefore->offset + itBefore->numChunks * CHUNK_SIZE;
			if (it->offset <= nextLoc.offset + nextLoc.numChunks * CHUNK_SIZE) nextLoc.numChunks = 0;
		}
		if (nextLoc.numChunks) newSet.insert(nextLoc);
		return nextLoc.numChunks;
	}
};
