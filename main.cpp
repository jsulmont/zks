#include <list>
#include <iostream>
#include <random>
#include "cxxopts.hpp"
#include "boost/format.hpp"
#include <boost/log/trivial.hpp>

#include "avalanche.hpp"

using namespace std;

// vector<int> N{0, 1};
extern Parameters parse_options(int, char **);
int main(int argc, char **argv)
{
   Parameters p = parse_options(argc, argv);

   Network net(p);

   auto &n1 = net.nodes[0];
   uniform_real_distribution<double> next_double(0.0, 1.0);
   list<TxPtr> c1, c2;

   // simulate a client
   for (auto i = 0; i < p.num_transactions; i++)
   {
      // pic a random node.
      std::uniform_int_distribution<int> dist(0, net.nodes.size() - 1);
      auto &n = net.nodes[dist(net.rng)];
      // auto &n = net.nodes[N[i % N.size()]];

      // send a transaction
      c1.push_back(n->onGenerateTx(i));

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
         ss << boost::format("znode-0-%03d.dot") % i;
         n1->dumpDag(ss.str());
      }
      cout << i << ":  " << n1->fractionAccepted() << endl;
   }
}
