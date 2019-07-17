#include <iostream>
#include "avalanche.hpp"

int main()
{
   Parameters params;
   Network network(params);
   network.run();
}
