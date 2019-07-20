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
#include <boost/log/trivial.hpp>

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
    std::string strid;

    Tx(int data, std::list<UUID> parents, int chit = 0, int confidence = 0)
        : id(boost::uuids::random_generator()()),
          data(data), parents(std::move(parents)), chit(chit),
          confidence(confidence)
    {
        strid = boost::uuids::to_string(id).substr(0, 5);
    }

    Tx(Tx const &tx)
        : id(tx.id), data(tx.data), parents(tx.parents),
          chit(tx.chit), confidence(tx.confidence), strid(tx.strid)
    {
    }

    Tx(Tx const &&tx)
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

    Tx &operator=(Tx const &tx)
    {
        id = std::move(tx.id);
        data = std::move(tx.data);
        parents = std::move(tx.parents);
        chit = std::move(tx.chit);
        confidence = std::move(tx.confidence);
        return *this;
    }

    bool operator!=(Tx const &tx) const
    {
        return id != tx.id;
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
struct tx_compare
{
    bool operator()(const Tx *tx1, const Tx *tx2) const
    {
        return tx1->id < tx2->id;
    }
};
typedef std::set<const Tx *, tx_compare> TxSet;

class ConflictSet
{
public:
    const Tx *pref = 0, *last = 0;
    size_t count, size;
    ConflictSet(const Tx *pref, const Tx *last, size_t count = 0, size_t size = 0)
        : pref(pref), last(last), count(count), size(size) {}
    ConflictSet(const ConflictSet &cs)
        : pref(cs.pref), last(cs.last), count(cs.count), size(cs.size) {}
    ConflictSet &operator=(ConflictSet &cs)
    {
        pref = cs.pref;
        last = cs.pref;
        count = cs.count;
        size = cs.size;
        return *this;
    }

private:
    ConflictSet();
};

class Network;

class Node
{
public:
    Node(int id, Parameters const &params,
         Network *network, Tx &genesis)
        : node_id(id), params(params), network(network), tx_genesis(genesis)
    {
        transactions.insert({tx_genesis.id, tx_genesis});
        queried.insert(tx_genesis.id);
        conflicts.insert({tx_genesis.data, ConflictSet{&tx_genesis, &tx_genesis, 0, 1}});
        accepted.insert(tx_genesis.id);
    }

    Tx onGenerateTx(int);
    void onReceiveTx(Node &, Tx &);
    Tx onSendTx(UUID &);
    int onQuery(Node &, Tx &);
    void avalancheLoop();
    TxSet parentSelection();
    double fractionAccepted();
    void dumpDag(const std::string &);

    int node_id;

    //private:
    TxSet parentSet(const Tx *);
    bool isPrefered(const Tx *);
    bool isStronglyPrefered(const Tx *);
    bool isAccepted(const Tx *);

    Parameters params;
    Network *network;
    Tx tx_genesis;
    tsl::ordered_map<UUID, Tx, boost::hash<UUID>> transactions;
    std::set<UUID> queried, accepted;
    std::map<int, ConflictSet> conflicts; // TODO UTXO
    std::map<UUID, TxSet> parentSets;
};

class Network
{
public:
    Network(Parameters const &params)
        : params(params), rng(params.seed), tx_genesis(-1, {}, 1)
    {
        for (auto i = 0; i <= params.num_nodes; i++)
            nodes.push_back(std::make_shared<Node>(Node(i, params, this, tx_genesis)));
    }

    void run()
    {
        for (auto &n : nodes)
            n->avalancheLoop();
    }

    // private:
    Parameters params;
    std::mt19937_64 rng;
    Tx tx_genesis; // genesis tx
    std::vector<std::shared_ptr<Node>> nodes;
    Network(Network const &) = delete;
    Network &operator=(Network const &) = delete;
};
