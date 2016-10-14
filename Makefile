all: diskSpotcheck filesystemTest fst

%: %.cpp
	g++ $< -o $@ -std=c++14 -O4 -Wall -lpthread

clean:
	rm -f diskSpotcheck filesystemTest fst
