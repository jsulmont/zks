CXX = clang++
CXXFLAGS = -std=c++17 -g
all: zks.exe

zks.exe: main.o
	$(CXX) -o zks.exe main.o

main.o: main.cc avalanche.h parameters.h
	$(CXX) $(CXXFLAGS) -c main.cc

clean:
	$(RM) *.o zks.exe

