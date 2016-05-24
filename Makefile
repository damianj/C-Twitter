CC=gcc
CXX=g++
CFLAGS=-Wall -pedantic -ljansson -loauth -lcurl
OBJ=mfs.o

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

%.o: %.cxx
	$(CXX) -c -o $@ $< $(CFLAGS)

all: twitter_scraper

mfs: $(OBJ)
	gcc -o twitter_scraper twitter_scraper.o $(CFLAGS)

clean:
	rm -f *.o twitter_scraper
