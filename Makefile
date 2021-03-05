CC = g++
CFLAGS = -lpthread -std=c++11

main: main.cpp
	$(CC) main.cpp -o main $(CFLAGS)

clean:
	rm -f core *.o
