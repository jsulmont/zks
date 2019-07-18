#include <list>
#include <iostream>
#include "avalanche.hpp"

using namespace std;
int main()
{
   Parameters p;
   Network net(p);

   auto &n1 = net.nodes[0];
   list<Tx> c1, c2;

   // simulate a client
   for (auto i = 0; i < p.num_transactions; i++)
   {
      // pic a random node.
      std::uniform_int_distribution<int> dist(0, net.nodes.size() - 1);
      auto &n = net.nodes[dist(net.rng)];
      n->protocol_loop();
   }
}
