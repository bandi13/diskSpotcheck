//read and write test for File System on RAIDX
//Guanlai Li
//liguanlai@gmail.com

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
using namespace std;

#define NUMBUFFERS 10
#define NUM_FILES 20


//usage syntax
void usage(char *progName) {
    cout << "Usage: " << progName << " <r|w|a> <testFilePath>" << endl;
    cout << "'r' for read test" << endl;
    cout << "'w' for write test" << endl;
    cout << "'a' for both" << endl;
}

//write test
bool write_test( const char* path, vector<unique_ptr<char>>& base ) {
    // write to file
    for(uint16_t i = 0; i < NUM_FILES; i++ ) {
        //compose file name
        std::ostringstream fname;
        fname << path << "/test" << i;
        //record start time
        auto startT = std::chrono::steady_clock::now();
        cout << "now write " << fname.str().c_str() << endl;
        int fd = open(fname.str().c_str(), O_RDWR | O_CREAT | O_LARGEFILE | O_DIRECT | O_SYNC | O_TRUNC, S_IRUSR | S_IWUSR);
        if(fd<0){ cerr << "error when open file: " << strerror(errno) << endl; return true;}
        //write file, the size of file would be (i+1)*4 MB, for example, test99 would be 400MB
        for(int j = 0; j <= i; j++) {//***change i to get larger file***
            srand(j);
            for(int k = 0; k < 1024; k++) {
                int wt = write(fd,base[rand()%NUMBUFFERS].get(), 4096);
                if(wt != 4096) { cerr << "error when write file " << wt << " " << strerror(errno) << endl; return true; }
            }
        }
        //print finish confirmation and speed of writing
        if(close(fd)<0) {cerr << "error when close file after write" << endl; return true;}
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds >(std::chrono::steady_clock::now() - startT).count() / 1000.0;
        cout << "success write " << (i+1)*4 << " MB at " << (i+1)*4/duration << " MB/s" << endl;
    }
    return false;
}


//read test
bool read_test(const char* path, vector<unique_ptr<char>>& base) {
    for( int i = 0; i < NUM_FILES/4; i++ ) {
        std::ostringstream fname;
        uint16_t fileNum = rand() % NUM_FILES;
        fname << path << "/test" << fileNum;
        auto startT = std::chrono::steady_clock::now();
        cout << "now validate " << fname.str().c_str() << endl;
        int fd = open(fname.str().c_str(), O_RDWR | O_LARGEFILE | O_DIRECT);
        if(fd<0){ cerr << "error when open file: " << strerror(errno) << endl; return true;}
        void *testStr;
        if(posix_memalign(&testStr, 4096, 4096)) { cerr << "Failed aligning memory" << strerror(errno) << endl; return true; }
        for(int j = 0; j <= fileNum; j++) {
            srand(j);
            for(int k = 0; k < 1024; k++) {
                int wt = read(fd,testStr, 4096);
                if(wt != 4096) { cerr << "error when read file " << wt << " " << strerror(errno) << endl; return true; }
                if(strncmp((char *)testStr, (char *)base[rand()%NUMBUFFERS].get(),4096)!=0){cerr << "error validate" << endl; return true;}
            }
        }
        if(close(fd)<0) {cerr << "error when close file after read" << endl; return true;}
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds >(std::chrono::steady_clock::now() - startT).count() / 1000.0;
        cout << "success read validation " << (fileNum+1)*4 << " MB at " << (fileNum+1)*4/duration << " MB/s" << endl;
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
    
    //prepair the base string trunk
    vector<unique_ptr<char>>base;
    for(int i = 0; i < NUMBUFFERS; i++) {
        srand(i);
        char *testStr;
        if(posix_memalign((void **)&testStr, 4096, 4096)) { cerr << "Failed aligning memory" << strerror(errno) << endl; return 1; }
        for(int j = 0; j < 4096; j++) ((char *)testStr)[j] = 'a' + rand()%26;
        base.emplace_back(unique_ptr<char>(testStr));
    }
    
    if(doWrite) {
        if(write_test(argv[argc-1], base)) { cerr << "Failed write test" << endl; return 1; }
    }
    
    if(doRead) {
        if(read_test(argv[argc-1], base)) { cerr << "Failed read test" << endl; return 1; }
    }
    
    cout << "Test completed successfully" << endl;
    return 0;
}

