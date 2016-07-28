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
	cout << "Usage: " << progName << " [r|w|a]" << endl;
	cout << "'r' for read test" << endl;
	cout << "'w' for write test" << endl;
	cout << "'a' for both" << endl;
}

//write test
//currently I write 1024*1024 random char
//future version would be take an argument for size, and take another argument for number of file to generate
void write_test( const char* path ) {
	ofstream myFile;
	clock_t t;
	float time;

	srand('a');
	for( int j = 1; j <= 9; j++ ) {
		std::ostringstream fname;
		fname << path << "test" << (uint16_t)j;
		t = clock();
		myFile.open( fname.str() );
		for(int i = 0; i < j*1024; i ++ ) myFile << rand();
		myFile.close();
		t = clock() - t;

		time = (float)t/CLOCKS_PER_SEC;
		cout << "write " << fname.str() <<" in " << time << " seconds" << endl;
	}

}

//read test
//current version can only support hard coded file name
//future version would take an argument to read specific file
void read_test(const char* path) {
	ifstream myFile;
	clock_t t;
	int i;
	string fname = "";
	float time;
	int id = 1;

	for( int j = 0; j < 5; j++ ) {
		std::ostringstream fname;
		fname << path << "test" << (uint16_t)(id+2*j);

		t = clock();
		myFile.open( fname.str() );
		while (myFile >> i ) {
			if(i != rand()) { cerr << "Invalid file written: " << fname.str() << endl; return; }
		}
		myFile.close();
		t = clock() - t;

		time = (float)t/CLOCKS_PER_SEC;
		cout << "read  " << fname.str() <<" in " << time << " seconds" << endl;
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

