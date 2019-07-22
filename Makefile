CXX = clang++ -std=c++2a
# CXX = g++-9 -std=c++17
CXXFLAGS = -DBOOST_LOG_DYN_LINK  -g -Wall
all: zks.exe

zks.exe: main.o avalanche.o
	$(CXX) -o zks.exe main.o avalanche.o -lboost_log-mt

avalanche.o: avalanche.cpp avalanche.hpp parameters.hpp
	$(CXX) $(CXXFLAGS) -c avalanche.cpp

main.o: main.cpp avalanche.hpp parameters.hpp
	$(CXX) $(CXXFLAGS) -c main.cpp

clean:
	$(RM) *.o zks.exe

