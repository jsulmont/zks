#include <list>
#include <iostream>
#include <random>
#include "boost/format.hpp"
#include <boost/log/trivial.hpp>

#include "avalanche.hpp"

using namespace std;

int main()
{
   Parameters p;
   Network net(p);
   cout << "GENESIS=" << net.tx_genesis.strid << endl;
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
      c1.push_back(n->onGenerateTx(i));

      // random double spend
      if (next_double(net.rng) < p.double_spend_ratio)
      {
         auto d = uniform_int_distribution<int>(0, i)(net.rng);
         cout << "double spend of " << d << endl;
         auto nodes = net.nodes;
         shuffle(nodes.begin(), nodes.end(), net.rng);
         auto &n2 = nodes.front();
         c2.push_back(n2->onGenerateTx(d));
      }

      net.run();

      if (p.dump_dags)
      {
         ostringstream ss;
         ss << boost::format("node-0-%03d.dot") % i;
         n1->dumpDag(ss.str());
      }
      cout << i << ":  " << n1->fractionAccepted() << endl;
#if 0
      for (auto &N : net.nodes)
         for (auto it = N->transactions.rbegin(); it < N->transactions.rend(); it++)
         {
            cerr << "R:" << i << " N:" << N->node_id << " " << it->second << endl;
         }
#endif
   }
}
