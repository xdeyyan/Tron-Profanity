CC=g++
CDEFINES=
SOURCES=Dispatcher.cpp Mode.cpp precomp.cpp profanity.cpp SpeedSample.cpp jsonxx.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=profanity



LDFLAGS=-s -lOpenCL -lcurl -lssl -lcrypto -mcmodel=large
CFLAGS=-c -std=c++11 -Wall -mmmx -O2 -mcmodel=large 


all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

.cpp.o:
	$(CC) $(CFLAGS) $(CDEFINES) $< -o $@

clean:
	rm -rf *.o

