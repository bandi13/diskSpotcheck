//read and write test for RAIDX
#include<iostream>
#include<sstream>
#include<fstream>
using namespace std;

//usage syntax
void usage(char *progName) {
	cout << "Usage: " << progName << " <r|w|a> <testFilePath>" << endl;
	cout << "'r' for read test" << endl;
	cout << "'w' for write test" << endl;
	cout << "'a' for both" << endl;
}

#define NUM_FILES 100

//write test
//currently I write n*1024 random char, where 'n' is a random number in range [0,1024]
//future version would be take an argument for size, and take another argument for number of file to generate
bool write_test( const char* path ) {
	ofstream myFile;

	for(uint16_t j = 0; j < NUM_FILES; j++ ) {
		std::ostringstream fname;
		fname << path << "/test" << j;
		int maxSize = (rand()%1024)*1024;
		myFile.open( fname.str(), std::ofstream::out | std::ofstream::trunc );
		if(!myFile.is_open()) { cerr << "Can't open file for write: " << fname.str() << endl; return true; }
		srand(j);
		while(maxSize--) myFile << rand() << endl;
		myFile.close();
	}
	cout << "Wrote " << NUM_FILES << " files" << endl;
	return false;
}

//read test
//current version can only support hard coded file name
//future version would take an argument to read specific file
bool read_test(const char* path) {
	ifstream myFile;
	int i;
	uint count;

	for( int j = 0; j < NUM_FILES/4; j++ ) {
		std::ostringstream fname;
		uint16_t fileNum = rand() % NUM_FILES;
		fname << path << "/test" << fileNum;

		myFile.open( fname.str() );
		count = 0;
		if(!myFile.is_open()) { cerr << "Can't open file for read: " << fname.str() << endl; return true; }
		int newNum;
		srand(fileNum);
		while (myFile >> i) {
			newNum = rand();
			if(i != newNum) { cerr << "Invalid file written: " << fname.str() << '(' << i << "!=" << newNum << ") at " << count << "th value" << endl; return true; } else count++;
		}
		myFile.close();
		cout << "Read(" << fname.str() << ") " << count << " values" << endl;
	}
	return false;
}

int main( int argc, char* argv[] ) {
	if(argc != 3) { usage(argv[0]); return 1; }
	bool doRead = false, doWrite = false;

	if(string(argv[1]) == "r") doRead = true;
	else if(string(argv[1]) == "w") doWrite = true;
	else if(string(argv[1]) == "a") { doRead = true; doWrite = true; }
	else { usage(argv[0]); return 1; }

	if(doWrite) {
		if(write_test(argv[argc-1])) { cerr << "Failed write test" << endl; return 1; }
	}

	if(doRead) {
		if(read_test(argv[argc-1])) { cerr << "Failed read test" << endl; return 1; }
	}

	cout << "Test completed successfully" << endl;
	return 0;
}

