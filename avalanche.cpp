#include "avalanche.hpp"

using namespace std;

Tx Node::create_tx(int data)
{
    auto edge = parent_selection();
    list<UUID> parent_uuids;
    transform(edge.begin(), edge.end(),
              back_inserter(parent_uuids),
              [](auto t) -> UUID { return t.id; });
    auto t = Tx(data, {});
    receive_tx(*this, t);
    return t;
}

void Node::receive_tx(Node &sender, Tx &tx)
{
    auto it = transactions.find(tx.id);
    if (it != transactions.end())
        return;
    tx.chit = 0;
    tx.confidence = 0;
    for (auto &it : tx.parents)
        if (transactions.find(it) == transactions.end())
        {
            auto t = sender.send_tx(it);
            receive_tx(sender, t); // recursive
        }
    auto c = conflicts.find(tx.data);
    if (c != conflicts.end())
        c->second.size++;
    else
        conflicts.insert(make_pair(tx.data, ConflictSet{tx, tx, 0, 1}));
    transactions.insert(make_pair(tx.id, tx));
}

Tx Node::send_tx(UUID &id)
{
    auto it = transactions.find(id);
    assert(it != transactions.end());
    return it->second;
}

int Node::query(Node &sender, Tx &tx)
{
    receive_tx(sender, tx);
    return is_strongly_prefered(tx) ? 1 : 0;
}

void Node::avalanche_loop()
{
    for (auto &[id, tx] : transactions)
    {
        if (queried.find(id) == queried.end())
            continue;
        vector<shared_ptr<Node>> sample;
        sample.resize(params.k);
        {
            auto tmp{network->nodes};
            remove_if(tmp.begin(), tmp.end(), [this](auto &it) { return it.get() == this; });
            shuffle(tmp.begin(), tmp.end(), network->rng);
            copy_n(tmp.begin(), params.k, sample.begin());
        }
        int res = 0;
        for (auto &n : sample)
        {
            auto x = tx; // TODO:
            res += n->query(*this, x);
        }
        if (res >= params.alpha * params.k)
        {
            tx.chit = 1;
            auto ps = parent_set(tx);
        }
        queried.insert(it.second.id);
    }
}

std::set<Tx> Node::parent_set(Tx tx)
{
    auto it = parent_sets.find(tx.id);
    if (it != parent_sets.end())
        return it->second;
    set<Tx> parents;
    set<UUID> ps(tx.parents.begin(), tx.parents.end());
    while (!ps.empty())
    {
        for (auto &it : ps)
        {
            auto x = transactions.find(it);
            if (x != transactions.end())
                parents.insert(x->second);
        }
        list<UUID> new_ps;
        auto out = new_ps.begin();
        for (auto it = ps.begin(); it != ps.end(); it++)
        {
            auto jt = transactions.find(*it);
            if (jt != transactions.end())
            {
                auto p = jt->second.parents;
                copy(p.begin(), p.end(), out);
            }
        }
        ps = set<UUID>(new_ps.begin(), new_ps.end());
    }
    parent_sets[tx.id] = parents;
    return parents;
}

bool Node::is_prefered(Tx tx)
{
    auto it = conflicts.find(tx.data);
    if (it != conflicts.end())
        return it->second.pref == tx;
    return false;
}

bool Node::is_strongly_prefered(Tx tx)
{
    for (auto &it : parent_set(tx))
        if (!is_prefered(tx))
            return false;
    return true;
}

bool Node::is_accepted(Tx tx)
{
    if (accepted.find(tx.id) != accepted.end())
        return true;
    if (queried.find(tx.id) == queried.end())
        return false;
    auto it{conflicts.find(tx.data)};
    assert(it != conflicts.end());
    auto cs{it->second};
    auto parents_accepted{
        [tx, this]() {
            for (auto it : tx.parents)
                if (accepted.find(it) == accepted.end())
                    return false;
            return true;
        }()};
    auto rc{
        (parents_accepted && cs.size == 1 && tx.confidence > params.beta1) ||
        (cs.pref == tx && cs.count > params.beta2)};
    if (rc)
        accepted.insert(tx.id);
    return rc;
}

list<Tx>
Node::parent_selection()
{
    list<Tx> eps0;
    set<Tx> eps1;
    for (auto &tx : transactions)
        if (is_strongly_prefered(tx.second))
        {
            eps0.push_back(tx.second);
            auto c = conflicts.find(tx.second.data);
            assert(c != conflicts.end());
            if (c->second.size == 1 || tx.second.confidence > 0)
                eps1.insert(tx.second);
        }
    list<Tx> parents;
    {
        auto out = parents.begin();
        for (auto &it : eps1)
        {
            auto p = parent_set(it);
            for (auto &jt : p)
                if (eps1.find(jt) == eps1.end())
                    parents.push_back(jt);
        }
    }
    list<Tx> fallback{
        [this]() -> list<Tx> {
            if (transactions.size() == 1)
                return {tx_genesis};
            else
            {
                // TODO: use std::range when available
                list<Tx> rc; // take 10
                auto it = transactions.rbegin(), end = it;
                advance(end, 10);
                for (; it != end; ++it)
                    rc.push_back(it->second);
                // for_each_n(transactions.rbegin(), 10, [rc](auto &e) { rc.push_back(e); });
                vector<Tx> rc2;
                for (auto &e : rc)
                {
                    if (is_accepted(e))
                        continue;
                    auto it = conflicts.find(e.data);
                    assert(it != conflicts.end());
                    if (it->second.size == 1)
                        rc2.push_back(std::move(e));
                }
                shuffle(rc2.begin(), rc2.end(), network->rng);
                return list<Tx>(rc2.begin(), rc2.begin() + 3);
            }
        }()};
    assert(!(parents.empty() && fallback.empty()));
    if (!parents.empty())
        return parents;
    return fallback;
}