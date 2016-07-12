all: diskSpotcheck filesystemTest

diskSpotcheck: diskSpotcheck.cpp
	g++ diskSpotcheck.cpp -o diskSpotcheck -std=c++14 -O4 -Wall

filesystemTest: filesystemTest.cpp
	g++ filesystemTest.cpp -o filesystemTest -std=c++14 -O4 -Wall


clean:
	rm -f diskSpotcheck filesystemTest
