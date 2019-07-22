CXX = clang++ -std=c++17
# CXX = g++-9 -std=c++17
CXXFLAGS = -g -Wall
all: zks.exe

zks.exe: main.o avalanche.o
	$(CXX) -o zks.exe main.o avalanche.o 

avalanche.o: avalanche.cpp avalanche.hpp parameters.hpp
	$(CXX) $(CXXFLAGS) -c avalanche.cpp

main.o: main.cpp avalanche.hpp parameters.hpp
	$(CXX) $(CXXFLAGS) -c main.cpp

clean:
	$(RM) *.o zks.exe

