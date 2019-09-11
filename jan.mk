CXX = clang++
CXXFLAGS = -std=c++17 -g
all: zks.exe

zks.exe: main.o avalanche.o
	$(CXX) -o zks.exe main.o avalanche.o

main.o: main.cpp avalanche.hpp parameters.hpp
	$(CXX) $(CXXFLAGS) -c main.cpp

avalanche.o: avalanche.cpp avalanche.hpp
	$(CXX) $(CXXFLAGS) -c avalanche.cpp

clean:
	$(RM) *.o zks.exe

