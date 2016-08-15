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
using namespace std;

int main( int argc, char* argv[] )
{
    stringstream inSize;
    inSize << argv[1];
    long size;
    inSize >> size;
    
    vector<string>base(10,"");
    for(int i = 0; i < 10; i++)
    {
        srand(i);
        for(int j = 0; j < 4096; j++)
        {
            base[i] += 'a' + rand()%26;
        }
    }

    clock_t t;
    float time;
    t = clock();
    
    int fd = open("test", O_RDWR | O_CREAT | O_LARGEFILE | O_DIRECT | O_SYNC | O_TRUNC);
    if(fd<0){ cerr << "error when open file: " << strerror(errno) << endl; return 1;}
    
    for(int i = 0; i < size; i++)
    {
        srand(i);
        for(int j = 0; j < 1024; j++)
        {
            void *testStr;
            if(posix_memalign(&testStr, 4096, 4096)) { cerr << "Failed aligning memory" << strerror(errno) << endl; return 1; }
            strcpy((char *)testStr, base[rand()%10].c_str());
            int wt = write(fd,testStr, 4096);
            if(wt != 4096) { cerr << "error when write file " << wt << " " << strerror(errno) << endl; return 1; }
        }
    }

    cout << "success write " << size*4 << " MB file" << endl;
    int cls = close(fd);
    if(cls<0) {cerr << "error when close file after write" << endl; return 1;}
    t = clock() - t;
    time = (float)t/CLOCKS_PER_SEC;
    cout << "write speed " << size*4/time << " MB/s" << endl;
    
    t = clock();
    fd = open("test", O_RDWR | O_CREAT | O_LARGEFILE | O_DIRECT);
    if(fd<0){ cerr << "error when open file: " << strerror(errno) << endl; return 1;}
    void *testStr;
    int wt = read(fd,testStr, 4096);
    cout << wt << " " << string((char *) testStr) << endl;
    for(int i = 0; i < size; i++)
    {
        srand(i);
        for(int j = 0; j < 1024; j++)
        {
            cout << "i="<< i << ", j=" << j << endl;
            if(strcmp((char *)testStr, base[rand()%10].c_str())!=0){cerr << "error validate" << endl; return 1;}
        }
    }
    cout << "success validation" << endl;
    cls = close(fd);
    if(cls<0) {cerr << "error when close file after read" << endl; return 1;}
    t = clock() - t;
    time = (float)t/CLOCKS_PER_SEC;
    cout << "read in " << time << " seconds" << endl;

    
    return 0;
}

