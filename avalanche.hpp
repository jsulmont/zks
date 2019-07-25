#pragma once
#include <set>
#include <map>
#include <list>
#include <random>
#include <memory>
#include <vector>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <unordered_map>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/functional/hash.hpp>
#include <boost/uuid/uuid_generators.hpp>

#include "parameters.hpp"
#include "tsl/ordered_map.h"

using UUID = boost::uuids::uuid;

// TODO: move semantics
struct Tx
{
    boost::uuids::uuid id;
    int data;
    std::list<UUID> parents;
    int chit;
    int confidence;
    std::string strid;

    Tx(int data, std::list<UUID> parents, int chit = 0, int confidence = 0)
        : id(boost::uuids::random_generator()()),
          data(data), parents(std::move(parents)), chit(chit),
          confidence(confidence)
    {
        strid = boost::uuids::to_string(id).substr(0, 5);
    }

    Tx(Tx &tx)
        : id(tx.id), data(tx.data), parents(tx.parents),
          chit(tx.chit), confidence(tx.confidence), strid(tx.strid)
    {
    }

    Tx(Tx &&tx)
        : id(std::move(tx.id)), data(tx.data), parents(std::move(tx.parents)),
          chit(tx.chit), confidence(tx.confidence), strid(tx.strid)
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

    bool operator!=(Tx const &tx) const
    {
        return id != tx.id;
    }

    Tx &operator=(Tx &&tx)
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

using TxPtr = std::shared_ptr<Tx>;
using TxSet = std::set<TxPtr>;

struct ConflictSet
{
    TxPtr pref = 0, last = 0;
    int count, size;

    ConflictSet(ConflictSet &&cs)
        : pref(std::move(cs.pref)), last(std::move(cs.last)), count(std::move(cs.count)), size(std::move(cs.size))
    {
    }

    ConflictSet(const TxPtr &pref, const TxPtr &last, int count = 0, int size = 0)
        : pref(pref), last(last), count(count), size(size) {}

    ConflictSet(const ConflictSet &cs)
        : pref(cs.pref), last(cs.last), count(cs.count), size(cs.size) {}

    ConflictSet &operator=(const ConflictSet &cs)
    {
        pref = cs.pref;
        last = cs.pref;
        count = cs.count;
        size = cs.size;
        return *this;
    }

    ConflictSet &operator=(ConflictSet &&cs)
    {
        pref = std::move(cs.pref);
        last = std::move(cs.pref);
        count = std::move(cs.count);
        size = std::move(cs.size);
        return *this;
    }
    ConflictSet() = delete;
};

class Network;

class Node
{
public:
    Node(int id, Parameters const &params,
         Network *network, Tx &tx_genesis)
        : node_id(id), params(params), network(network),
          genesis(std::make_shared<Tx>(tx_genesis))
    {
        transactions.insert({genesis->id, genesis});
        queried.insert(genesis->id);
        accepted.insert(genesis->id);
        conflicts.insert({genesis->data, ConflictSet{genesis, genesis, 0, 1}});
        parentSets.insert({genesis->id, {}});
    }

    TxPtr onGenerateTx(int);
    void onReceiveTx(Node &, TxPtr &);
    TxPtr onSendTx(UUID &);
    int onQuery(Node &, TxPtr &);
    void avalancheLoop();
    std::vector<TxPtr> parentSelection();
    bool isAccepted(const TxPtr &);
    double fractionAccepted();
    void dumpDag(const std::string &);
    int node_id;

private:
    TxSet parentSet(const TxPtr &);
    bool isPrefered(const TxPtr &);
    bool isStronglyPrefered(const TxPtr &);

    Parameters params;
    Network *network;
    TxPtr genesis;
    tsl::ordered_map<UUID, TxPtr, boost::hash<UUID>> transactions;
    std::set<UUID> queried, accepted;
    std::map<int, ConflictSet> conflicts; // TODO UTXO
    std::map<UUID, TxSet> parentSets;
};

class Network
{
public:
    Network(Parameters const &params)
        : params(params), rng(params.seed), genesis(-1, {}, 1)
    {
        for (auto i = 0; i <= params.num_nodes; i++)
            nodes.push_back(std::make_shared<Node>(Node(i, params, this, genesis)));
    }

    void run()
    {
        for (auto &n : nodes)
            n->avalancheLoop();
    }

    // private:
    Parameters params;
    std::mt19937_64 rng;
    Tx genesis; // genesis tx
    std::vector<std::shared_ptr<Node>> nodes;
    Network(Network const &) = delete;
    Network &operator=(Network const &) = delete;
};
