#include <list>
#include <random>
#include <iostream>
#include <boost/format.hpp>

#include "cxxopts.hpp"
#include "avalanche.hpp"

using namespace std;

extern Parameters parse_options(int, char **);

int main(int argc, char **argv)
{
   Parameters p = parse_options(argc, argv);

   Network net(p);

   auto &n1 = net.nodes[0];
   uniform_real_distribution<double> next_double(0.0, 1.0);
   TxSet c1, c2;

   // simulate a client
   for (auto i = 0; i < p.num_transactions; i++)
   {

      // pic a random node.
      std::uniform_int_distribution<int> dist(0, net.nodes.size() - 1);
      auto &n = net.nodes[dist(net.rng)];
      // auto &n = net.nodes[N[i % N.size()]];

      // send a transaction
      c1.insert(n->onGenerateTx(i));

      if (next_double(net.rng) < p.double_spend_ratio)
      {
         // generate a double spend
         auto d = uniform_int_distribution<int>(0, i)(net.rng);
         cout << "double spend of " << d << endl;
         auto nodes = net.nodes;
         shuffle(nodes.begin(), nodes.end(), net.rng);
         auto &n2 = nodes.front();
         c2.insert(n2->onGenerateTx(d));
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

   // we check that either one or none of two
   // conflicting transactions have been accepted,
   // but not both! (double spending).
   map<int, vector<TxPtr>> conflict_sets;
   for (auto &t : c1)
      conflict_sets[t->data].push_back(t);
   for (auto &t : c2)
      conflict_sets[t->data].push_back(t);
   for (auto &[v, l] : conflict_sets)
      if (l.size() == 2)
      {
         auto tx1_anynode{[&, l = l]() {
            for (auto &n : net.nodes)
               if (n->isAccepted(l[0]))
                  return true;
            return false;
         }()};
         auto tx2_anynode{[&, l = l]() {
            for (auto &n : net.nodes)
               if (n->isAccepted(l[1]))
                  return true;
            return false;
         }()};
         assert(!(tx1_anynode && tx2_anynode));
         cout << "double spend: data=" << v << " Txs = ";
         if (tx1_anynode)
            cout << "[" << l[0]->strid << "] ";
         else
            cout << l[0]->strid;
         if (tx2_anynode)
            cout << " [" << l[1]->strid << "]";
         else
            cout << l[1]->strid;
         cout << endl;
      }
}
