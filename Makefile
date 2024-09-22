all:
	gcc -c  ./xerect.c  -std=c99 -pedantic -Wall -Wextra -Wno-variadic-macros -g -O2 -o xerect.o -lX11
	g++ -O2 -std=c++11 -o xerect ./ocr.cpp ./xerect.o  -lrt -I/usr/include/tesseract/ -L/usr/lib/ -llept  -ltesseract -larchive -lX11
clean:
	rm -rf *.o
	rm -rf xerect
