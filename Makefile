all: diskSpotcheck filesystemTest fst

%: %.cpp
	g++ $< -o $@ -std=c++14 -O4 -Wall

clean:
	rm -f diskSpotcheck filesystemTest fst
