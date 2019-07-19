CXX = clang++ -std=c++17
CXXFLAGS =  -DBOOST_LOG_DYN_LINK  -g -Wall
all: zks.exe

zks.exe: main.o avalanche.o
	$(CXX) -o zks.exe main.o avalanche.o -lboost_log-mt

avalanche.o: avalanche.cpp avalanche.hpp parameters.hpp
	$(CXX) $(CXXFLAGS) -c avalanche.cpp

main.o: main.cc avalanche.hpp parameters.hpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c main.cc

clean:
	$(RM) *.o zks.exe

