//read and write test for RAIDX
//Guanlai Li
//liguanlai@gmail.com

#include<iostream>
#include<sstream>
#include<fstream>
#include<cstdlib>
#include<cstdint>
#include<string>
#include<ctime>
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
void write_test( const char* path ) {
	ofstream myFile;

	for(uint16_t j = 0; j < NUM_FILES; j++ ) {
		std::ostringstream fname;
		fname << path << "/test" << j;
		int maxSize = (rand()%1024)*1024;
		srand(j);
		myFile.open( fname.str() );
		while(maxSize--) myFile << rand();
		myFile.close();
	}
}

//read test
//current version can only support hard coded file name
//future version would take an argument to read specific file
void read_test(const char* path) {
	ifstream myFile;
	int i;

	for( int j = 0; j < NUM_FILES/4; j++ ) {
		std::ostringstream fname;
		uint16_t fileNum = rand() % NUM_FILES;
		fname << path << "/test" << fileNum;

		srand(fileNum);
		myFile.open( fname.str() );
		while (myFile >> i ) if(i != rand()) { cerr << "Invalid file written: " << fname.str() << endl; return; }
		myFile.close();
	}
}

int main( int argc, char* argv[] ) {
	if(argc != 3) { usage(argv[0]); return 1; }

	if(string(argv[1]) == "r") read_test(argv[argc-1]);
	else if(string(argv[1]) == "w") write_test(argv[argc-1]);
	else if(string(argv[1]) == "a") { write_test(argv[argc-1]); read_test(argv[argc-1]); }
	else { usage(argv[0]); return 1; }

	return 0;
}

