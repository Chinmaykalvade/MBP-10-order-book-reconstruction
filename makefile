# Compiler & Flags
CXX := g++
CXXFLAGS := -std=c++17 -O3 -Wall

# Executable name
TARGET := reconstruction

all: $(TARGET)

$(TARGET): main.cpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) main.cpp

clean:
	rm -f $(TARGET) *.o *.exe output.csv
