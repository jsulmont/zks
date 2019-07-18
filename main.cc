#include <list>
#include <iostream>
#include <random>

#include "avalanche.hpp"

using namespace std;

int main()
{
   Parameters p;
   Network net(p);

   auto &n1 = net.nodes[0];
   uniform_real_distribution<double> next_double(0.0, 1.0);
   list<Tx> c1, c2;

   // simulate a client
   for (auto i = 0; i < p.num_transactions; i++)
   {
      // pic a random node.
      std::uniform_int_distribution<int> dist(0, net.nodes.size() - 1);
      auto &n = net.nodes[dist(net.rng)];

      // send a transaction
      c1.push_back(n->create_tx(i));

      // random double spend
      if (next_double(net.rng) < p.double_spend_ratio)
      {
         auto d = uniform_int_distribution<int>(0, i)(net.rng);
         cout << "double spend of " << d << endl;
         auto nodes = net.nodes;
         shuffle(nodes.begin(), nodes.end(), net.rng);
         auto &n2 = nodes.front();
         c2.push_back(n2->create_tx(d));
      }

      net.run();

      cout << i << ":  " << n->fraction_accepted() << endl;
   }
}
