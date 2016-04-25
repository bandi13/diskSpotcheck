all: diskSpotcheck

diskSpotcheck: diskSpotcheck.cpp
	g++ diskSpotcheck.cpp -o diskSpotcheck -std=c++14 -O4 -Wall

clean:
	rm -f diskSpotcheck
