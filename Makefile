CXX = clang++
CXXFLAGS = -std=c++17 -g
all: zks.exe

zks.exe: main.o avalanche.o
	$(CXX) -o zks.exe main.o avalanche.o

avalanche.o: avalanche.cpp avalanche.hpp parameters.hpp
	$(CXX) $(CXXFLAGS) -c avalanche.cpp

main.o: main.cc avalanche.hpp parameters.hpp
	$(CXX) $(CXXFLAGS) -c main.cc

clean:
	$(RM) *.o zks.exe

