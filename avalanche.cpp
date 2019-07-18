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

void Node::receive_tx(Node &node, Tx tx)
{
}

// template <typename InIter, typename OutIter, typename FN>
// void flat_map(InIter begin, InIter end, OutIter out, FN fn)
// {
//     for (; begin != end; ++begin)
//     {
//         auto y = fn(*begin);
//         std::copy(std::begin(y), std::end(y), out);
//     }
// }

std::set<Tx>
Node::parent_set(Tx tx)
{
    auto it = parent_sets.find(tx.id);
    if (it != parent_sets.end())
        return it->second;
    set<Tx> parents;
    set<UUID> ps(tx.parents.begin(), tx.parents.end());
    while (!ps.empty())
    {
        for (auto it : ps)
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
    for (auto it : parent_set(tx))
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
    for (auto tx : transactions)
        if (is_strongly_prefered(tx.second))
        {
            eps0.push_back(tx.second);
            auto c = conflicts.find(tx.second.data);
            if (c != conflicts.end() && (c->second.size == 1 || tx.second.confidence > 0))
                eps1.insert(tx.second);
        }
    list<Tx> parents;
    {
        auto out = parents.begin();
        for (auto it : eps1)
        {
            auto p = parent_set(it);
            for (auto jt : p)
                if (eps1.find(jt) == eps1.end())
                    parents.push_back(jt);
        }
    }
    list<Tx> fallback{
        [this]() -> list<Tx> {
            if (transactions.size() == 1)
                return {genesis_tx};
            else
            {
                list<Tx> rc; // take 10
                auto it = transactions.rbegin();
                auto end = it;
                advance(end, 10);
                for (; it != end; ++it)
                    rc.push_back(it->second);
                vector<Tx> rc2; // filter
                for (auto it : rc)
                {
                    auto c = conflicts.find(it.data);
                    if (!is_accepted(it) && c != conflicts.end() && c->second.size == 1)
                        rc2.push_back(it);
                }
                shuffle(rc2.begin(), rc2.end(), network->rng);
                return list<Tx>(rc2.begin(), rc2.begin() + 3);
            }
        }()};
    return {};
}