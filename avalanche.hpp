#pragma once
#include <list>
#include <random>
#include <sstream>
#include <memory>
#include <vector>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>

#include "parameters.hpp"

// TODO: move semantics
struct Transaction
{
    boost::uuids::uuid id;
    int data;
    std::list<boost::uuids::uuid> parents;
    int chit;
    int confidence;

    Transaction()
        : id(boost::uuids::random_generator()()), data(-1), chit(1), confidence(0)
    {
    }

    Transaction(Transaction const &tx)
        : id(tx.id), data(tx.data), parents(tx.parents),
          chit(tx.chit), confidence(tx.confidence)
    {
    }

    Transaction(Transaction const &&tx)
        : id(std::move(tx.id)), data(tx.data), parents(std::move(tx.parents)),
          chit(tx.chit), confidence(tx.confidence)
    {
    }

    bool operator==(Transaction const &tx) const
    {
        return id == tx.id && data == tx.data &&
               parents == tx.parents &&
               chit == tx.chit && confidence == tx.confidence;
    }

    Transaction &operator=(Transaction const &tx)
    {
        id = std::move(tx.id);
        data = std::move(tx.data);
        parents = std::move(tx.parents);
        chit = std::move(tx.chit);
        confidence = std::move(tx.confidence);
        return *this;
    }

    friend std::ostream &operator<<(std::ostream &out, Transaction const &tx)
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

inline std::string Transaction::to_string() const
{
    std::ostringstream ss;
    ss << *this;
    return ss.str();
}

struct ConflictSet
{
    Transaction pref, last;
    int count = 0, size = 0;
};

class Network;

class Node
{
public:
    Node(int id, Parameters const &params,
         Network *network, Transaction &genesis)
        : node_id(id), params(params), network(network), genesis_tx(genesis)
    {
    }

    virtual void protocol_loop()
    {
        std::cout << "Node(" << node_id << ") network=0x"
                  << std::hex << network << std::dec
                  << " genesis=" << genesis_tx << std::endl;
    }

private:
    int node_id;
    Parameters params;
    Network *network;
    Transaction genesis_tx;
};

class Network
{
public:
    Network(Parameters const &params)
        : params(params), rng(params.seed), tx()
    {
        for (auto i = 0; i <= params.num_nodes; i++)
            nodes.push_back(std::make_shared<Node>(Node(i, params, this, tx)));
    }

    void run()
    {
        for (auto &n : nodes)
            n->protocol_loop();
    }

private:
    Parameters params;
    std::mt19937_64 rng;
    Transaction tx; // genesis transaction
    std::vector<std::shared_ptr<Node>> nodes;
    Network(Network const &) = delete;
    Network &operator=(Network const &) = delete;
};
