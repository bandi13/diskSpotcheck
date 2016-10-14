//read and write test for File System on RAIDX
//Guanlai Li & Andras Fekete

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
using namespace std;

#define NUMBUFFERS 10
#define NUM_FILES 20
#define CHUNK_SIZE (1*4096)
#define ONE_MB ((1024*1024) / CHUNK_SIZE)

//usage syntax
void usage(char *progName) {
	cout << "Usage: " << progName << " <OPTIONS>* <testFilePath>" << endl;
	cout << "OPTIONS:" << endl;
	cout << "\tw         => Perform write of all " << NUM_FILES << " files" << endl;
	cout << "\tr <count> => Read random 'count' files" << endl;
	cout << "\tR <n>     => Read 'n'-th test file" << endl;
	cout << "Note: Multiple options can be passed multiple times. Such as " << progName << " -w -r 10 -R 8 -R 8" << endl;
	cout << "Chunk size = " << CHUNK_SIZE << endl;
}

double write_file( const char* path, const vector<unique_ptr<const char>>& base, const uint16_t i) {
	std::ostringstream fname;
	fname << path << "/test" << i;
	cout << "now writing " << fname.str().c_str() << "..." << endl;
	//record start time
	auto startT = std::chrono::steady_clock::now();
	int fd = open(fname.str().c_str(), O_RDWR | O_CREAT | O_LARGEFILE | O_DIRECT | O_SYNC | O_TRUNC, S_IRUSR | S_IWUSR);
	if(fd < 0) { cerr << "error opening file: " << strerror(errno) << endl; return NAN; }
	std::ranlux24_base rngGen(i);
	int fileSizeMB = (i+1)*10;
	for(int j = fileSizeMB*ONE_MB; j; j--) if(write(fd,base[rngGen()%NUMBUFFERS].get(), CHUNK_SIZE) != CHUNK_SIZE) { cerr << "error: " << strerror(errno) << endl; return NAN; }
	//print finish confirmation and speed of writing
	if(close(fd)<0) { cerr << "error closing file after write" << endl; return NAN; }
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds >(std::chrono::steady_clock::now() - startT).count() / 1000.0;
	cout << "successful write of " << fileSizeMB << " MB at " << fileSizeMB / duration << " MB/s" << endl;
	return duration;
}

//write test
bool write_test( const char* path, const vector<unique_ptr<const char>>& base ) {
	std::vector<std::future<double>> procs;
	for(uint16_t i = 0; i < NUM_FILES; i++ ) procs.push_back(std::async(std::launch::async,[&](uint16_t val) { return write_file(path,base,val); },i ));
	for(uint16_t i = 0; i < NUM_FILES; i++ ) if(procs[i].get() == NAN) return true;
	return false;
}

struct voidPtrDeleter { void operator()(void *p) { free(p); } };
//read test
bool read_file(const char* path, const vector<unique_ptr<const char>>& base, const uint16_t fileNum) {
	assert(fileNum < NUM_FILES);
	std::ostringstream fname;
	fname << path << "/test" << fileNum;
	void *testStr;
	if(posix_memalign(&testStr, 4096, CHUNK_SIZE)) { cerr << "Failed aligning memory" << strerror(errno) << endl; return true; }
	unique_ptr<void,voidPtrDeleter> testPtr(testStr); // make smart ptr remember to free the memory
	cout << "now validating " << fname.str().c_str() << "..." << endl;
	auto startT = std::chrono::steady_clock::now();
	int fd = open(fname.str().c_str(), O_RDWR | O_LARGEFILE | O_DIRECT);
	if(fd<0){ cerr << "error opening file: " << strerror(errno) << endl; return true;}
	std::ranlux24_base rngGen(fileNum);
	int numRead;
	size_t fileSize = 0;
	while(true) {
		numRead = read(fd,testStr, CHUNK_SIZE);
		if(numRead != CHUNK_SIZE) {
			if(numRead == 0) break;
			else cerr << "error reading file " << numRead << " " << strerror(errno) << endl; return true;
		}
		if(memcmp(testStr, base[rngGen()%NUMBUFFERS].get(),CHUNK_SIZE)) { cerr << "error validate" << endl; return true; }
		fileSize += numRead;
	}
	if(close(fd)<0) {cerr << "error closing file after read" << endl; return true;}
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds >(std::chrono::steady_clock::now() - startT).count() / 1000.0;
	cout << "successful read validation at " << fileSize/(duration*1024*1024) << " MB/s" << endl;
	return false;
}

bool read_test(const char* path, const vector<unique_ptr<const char>>& base, uint16_t numReads) {
	std::vector<uint16_t> files;
	while(numReads--) files.push_back(rand() % NUM_FILES);
	std::vector<std::future<bool>> procs;
	for(size_t i = 0; i < files.size(); i++) procs.push_back(std::async(std::launch::async, [&](uint16_t val) { return read_file(path,base,val); },i ) );
	for(size_t i = 0; i < files.size(); i++) if(procs[i].get()) return true;
	return false;
}

int main( int argc, char* argv[] ) {
	int opt;

	//prepare the base blocks
	vector<unique_ptr<const char>>base;
	for(int i = 0; i < NUMBUFFERS; i++) {
		srand(i);
		char *testStr;
		if(posix_memalign((void **)&testStr, 4096, CHUNK_SIZE)) { cerr << "Failed aligning memory" << strerror(errno) << endl; return 1; }
		for(int j = 0; j < CHUNK_SIZE; j++) ((char *)testStr)[j] = 'a' + rand()%26;
		base.emplace_back(unique_ptr<const char>(testStr));
	}
	if(argc < 3) { usage(argv[0]); return 0; }

	while ((opt = getopt(argc-1, argv, "wR:r:")) != -1) {
		switch (opt) {
			case 'w': if(write_test(argv[argc-1], base)) { cerr << "Failed write test" << endl; return 1; } break;
			case 'R': {
					uint16_t fileNum = atoi(optarg);
					if(read_file(argv[argc-1], base,fileNum)) { cerr << "Failed read file number: " << fileNum << endl; return 1; }
				}
				break;
			case 'r': {
					uint16_t fileNum = atoi(optarg);
					if(read_test(argv[argc-1], base,fileNum)) { cerr << "Failed read test" << endl; return 1; }
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

