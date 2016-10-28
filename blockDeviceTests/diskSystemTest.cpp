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
#include "diskSystemTest_tests.h"
using namespace std;

template<typename T>
static double toMB(T val) { return (double)val / (1024*1024); }

//usage syntax
void usage(char *progName) {
	cout << "Usage: " << progName << " <OPTIONS>* <testFilePath>" << endl;
	cout << "OPTIONS:" << endl;
	cout << "\t-c <sizeInMB>  => Do random reads until 'sizeInMB' has been read to clear caches" << endl;
	cout << "\t-w <minutes>   => Write for 'minutes' minutes" << endl;
	cout << "\t-r <minutes>   => Read for 'minutes' minutes" << endl;
	cout << "\t-p <percent>   => Set the percent of disk to be accessed" << endl;
	cout << "\t-P <percent>   => Update 'percent'% of the access locations" << endl;
	cout << "\t-t <num>       => Use 'num' threads in transactions" << endl;
	cout << "\t-b             => Set BUFFERED file access mode" << endl;
	cout << "\t-u             => Set UNBUFFERED file access mode (default)" << endl;
	cout << "\t-d             => Set DIRECT file access mode" << endl;
	cout << "\t-s <seconds>   => Sleep until 'seconds' seconds" << endl;
	cout << "\t-T             => Test THROUGHPUT" << endl;
	cout << "\t-R <numChunks> => Test RESPONSETIME (default)" << endl;
	cout << "Note: Multiple options can be passed multiple times. Such as " << progName << " -w 10 -r 10 -p 10.5 -r 10" << endl;
	cout << "Note: Percent can be 0 to 100 inclusive, but I would not go past 80%. The way the locations get generated may take a long time to complete." << endl;
	cout << "Chunk size = " << CHUNK_SIZE << endl;
}

int main( int argc, char* argv[] ) {
	int opt;

	if(argc < 3) { usage(argv[0]); return 0; }
	{
		FileUnbuffered file(argv[argc-1]);
		if (file.getSize() == 0) { cerr << "Can't open file: " << argv[argc-1] << endl; return 0; }
	}

	uint8_t numThreads = 10;
	double percent = 10;
	Test::File_t type = Test::FILE_UNBUFFERED;
	std::unique_ptr<Test> test = make_unique<Test_Throughput>(argv[argc-1]);
	test->generateLocs(percent);

	while ((opt = getopt(argc-1, argv, "c:w:r:p:P:t:buds:TR:")) != -1) {
		switch (opt) {
			case 'c': {
					uint8_t seconds = atoi(optarg);
					cout << "Cache clear for " << (int)seconds << "s..." << flush;
					auto sizeRead = test->cacheClear(std::chrono::steady_clock::now() + std::chrono::seconds(seconds));
					if(sizeRead == -1) { cerr << "Failed cache clear" << endl; return 1; }
					cout << "done: " << toMB(sizeRead) << "MB read" << endl;
				}
				break;
			case 'w': {
					uint8_t minutes = atoi(optarg);
					cout << "Write test for " << (int)minutes << "min..." << flush;
					cout << "done: " << test->do_testString(std::chrono::steady_clock::now() + std::chrono::minutes(minutes),false, numThreads,type) << endl;
				}
				break;
			case 'r': {
					uint8_t minutes = atoi(optarg);
					cout << "Read test for " << (int)minutes << "min..." << flush;
					cout << "done: " << test->do_testString(std::chrono::steady_clock::now() + std::chrono::minutes(minutes),true, numThreads,type) << endl;
				}
				break;
			case 'p': {
					percent = atof(optarg);
					cout << "Regenerating locations to " << percent << "%..." << flush;
					if((percent < 0) || (percent > 100)) { cerr << "Percent value should be between 0 and 100: " << optarg << endl; break; }
					test->generateLocs(percent);
					cout << "done: " << endl;
				}
				break;
			case 'P': {
					double percentIn = atof(optarg);
					if((percent < 0) || (percent > 100)) { cerr << "Percent value should be between 0 and 100: " << optarg << endl; break; }
					percent = percentIn;
					cout << "Adjusting locations to " << percent << "%..." << flush;
					test->updateLocs(percent);
					cout << "done: " << endl;
				}
				break;
			case 't': numThreads = atoi(optarg); break;
			case 'b': type = Test::FILE_BUFFERED; break;
			case 'u': type = Test::FILE_UNBUFFERED; break;
			case 'd': type = Test::FILE_DIRECT; break;
			case 's': {
					int seconds = atoi(optarg);
					cout << "Sleep for " << (int)seconds << "s..." << flush;
					std::this_thread::sleep_for(std::chrono::seconds(seconds));
					cout << "done" << endl;
				}
				break;
			case 'T':
				cout << "Setting test: Throughput..." << flush;
				test = make_unique<Test_Throughput>(argv[argc-1]);
				test->generateLocs(percent);
				cout << "done" << endl;
				break;
			case 'R': {
					uint8_t numChunks = atoi(optarg);
					cout << "Setting test: ResponseTime with " << (int)numChunks << " chunks..." << flush;
					test = make_unique<Test_ResponseTime>(argv[argc-1],numChunks);
					test->generateLocs(percent);
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
