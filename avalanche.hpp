#pragma once
#include <list>
#include <set>
#include <map>
#include <algorithm>
#include <random>
#include <sstream>
#include <memory>
#include <vector>
#include <iostream>
#include <unordered_map>

#include <boost/functional/hash.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>

#include "tsl/ordered_map.h"
#include "parameters.hpp"

using UUID = boost::uuids::uuid;

// TODO: move semantics
struct Tx
{
    boost::uuids::uuid id;
    int data;
    std::list<UUID> parents;
    int chit;
    int confidence;

    Tx(int data, std::list<UUID> parents, int chit = 1, int confidence = 0)
        : id(boost::uuids::random_generator()()),
          data(data), parents(std::move(parents)), chit(chit),
          confidence(confidence)
    {
    }

    Tx(Tx const &tx)
        : id(tx.id), data(tx.data), parents(tx.parents),
          chit(tx.chit), confidence(tx.confidence)
    {
    }

    Tx(Tx const &&tx)
        : id(std::move(tx.id)), data(tx.data), parents(std::move(tx.parents)),
          chit(tx.chit), confidence(tx.confidence)
    {
    }

    bool operator<(Tx const &tx) const
    {
        return id < tx.id;
    }

    bool operator==(Tx const &tx) const
    {
        return id == tx.id && data == tx.data &&
               parents == tx.parents &&
               chit == tx.chit && confidence == tx.confidence;
    }

    Tx &operator=(Tx const &tx)
    {
        id = std::move(tx.id);
        data = std::move(tx.data);
        parents = std::move(tx.parents);
        chit = std::move(tx.chit);
        confidence = std::move(tx.confidence);
        return *this;
    }

    friend std::ostream &operator<<(std::ostream &out, Tx const &tx)
    {
        out << "T(id=" << boost::uuids::to_string(tx.id).substr(0, 5) << ", data=" << tx.data
            << ", parents=[";
        for (auto const &x : tx.parents)
            out << boost::uuids::to_string(x).substr(0, 5) << ",";
        out << "], chit=" << tx.chit << ", confidence=" << tx.confidence << ")";
        return out;
    }

    std::string to_string() const;
};

inline std::string Tx::to_string() const
{
    std::ostringstream ss;
    ss << *this;
    return ss.str();
}

struct ConflictSet
{
    Tx pref, last;
    size_t count, size;
};

class Network;

class Node
{
public:
    Node(int id, Parameters const &params,
         Network *network, Tx &genesis)
        : node_id(id), params(params), network(network), genesis_tx(genesis)
    {
    }

    std::list<Tx> parent_selection();

    // create a new transaction on.
    // for the sake of simulation, will call
    // receive transaction on that same node.
    Tx create_tx(int);

    void receive_tx(Node &, Tx);

    virtual void protocol_loop()
    {
        std::cout << "Node(" << node_id << ") network=0x"
                  << std::hex << network << std::dec
                  << " genesis=" << genesis_tx << std::endl;
    }

private:
    std::set<Tx> parent_set(Tx);
    bool is_prefered(Tx);
    bool is_strongly_prefered(Tx);
    bool is_accepted(Tx);

    int node_id;
    Parameters params;
    Network *network;
    Tx genesis_tx;
    // std::unordered_map<UUID, Tx, boost::hash<UUID>> transactions;
    tsl::ordered_map<UUID, Tx, boost::hash<UUID>> transactions;
    std::set<UUID> queried, accepted;
    std::map<int, ConflictSet> conflicts;
    std::map<UUID, std::set<Tx>> parent_sets;
};

class Network
{
public:
    Network(Parameters const &params)
        : params(params), rng(params.seed), tx(-1, {}, 1)
    {
        for (auto i = 0; i <= params.num_nodes; i++)
            nodes.push_back(std::make_shared<Node>(Node(i, params, this, tx)));
    }

    void run()
    {
        for (auto &n : nodes)
            n->protocol_loop();
    }

    // private:
    Parameters params;
    std::mt19937_64 rng;
    Tx tx; // genesis tx
    std::vector<std::shared_ptr<Node>> nodes;
    Network(Network const &) = delete;
    Network &operator=(Network const &) = delete;
};
