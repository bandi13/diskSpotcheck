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
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>
#include<string.h>
#include<memory>
#include<chrono>
using namespace std;

#define NUMBUFFERS 10

int main( int argc, char* argv[] ) {
    long size = atoi(argv[1]);
    
    vector<unique_ptr<char>>base;
    for(int i = 0; i < NUMBUFFERS; i++) {
        srand(i);
				char *testStr;
				if(posix_memalign((void **)&testStr, 4096, 4096)) { cerr << "Failed aligning memory" << strerror(errno) << endl; return 1; }
        for(int j = 0; j < 4096; j++) ((char *)testStr)[j] = 'a' + rand()%26;
				base.emplace_back(unique_ptr<char>(testStr));
    }

		auto startT = std::chrono::steady_clock::now();
    int fd = open("test", O_RDWR | O_CREAT | O_LARGEFILE | O_DIRECT | O_SYNC | O_TRUNC, S_IRUSR | S_IWUSR);
    if(fd<0){ cerr << "error when open file: " << strerror(errno) << endl; return 1;}
    
    for(int i = 0; i < size; i++) {
        srand(i);
        for(int j = 0; j < 1024; j++) {
            int wt = write(fd,base[rand()%NUMBUFFERS].get(), i*4096);
            if(wt != 4096) { cerr << "error when write file " << wt << " " << strerror(errno) << endl; return 1; }
        }
    }

    cout << "success write " << size*4 << " MB file" << endl;
    if(close(fd)<0) {cerr << "error when close file after write" << endl; return 1;}
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds >(std::chrono::steady_clock::now() - startT).count() / 1000.0;
    cout << "write speed " << size*4/duration << " MB/s" << endl;

		startT = std::chrono::steady_clock::now();
    fd = open("test", O_RDWR | O_LARGEFILE | O_DIRECT);
    if(fd<0){ cerr << "error when open file: " << strerror(errno) << endl; return 1;}
		void *testStr;
		if(posix_memalign(&testStr, 4096, 4096)) { cerr << "Failed aligning memory" << strerror(errno) << endl; return 1; }
    for(int i = 0; i < size; i++) {
        srand(i);
        for(int j = 0; j < 1024; j++) {
						int wt = read(fd,testStr, 4096);
						//cout << wt << " " << string((char *) testStr) << endl;
            //cout << "i="<< i << ", j=" << j << endl;
            if(strncmp((char *)testStr, (char *)base[rand()%NUMBUFFERS].get(),4096)!=0){cerr << "error validate" << endl; return 1;}
        }
    }
    cout << "success validation" << endl;
    if(close(fd)<0) {cerr << "error when close file after read" << endl; return 1;}
		duration = std::chrono::duration_cast<std::chrono::milliseconds >(std::chrono::steady_clock::now() - startT).count() / 1000.0;
    cout << "read speed " << size*4/duration << " MB/s" << endl;

    return 0;
}

