#include <fstream>
#include <boost/format.hpp>
#include <boost/log/trivial.hpp>
#include "avalanche.hpp"

using namespace std;

string prtx(const Tx &tx)
{
    ostringstream ss;
    ss << "T(" << tx.strid
       << ", data=" << tx.data << ")";
    return ss.str();
}

string prtxs(const list<Tx> &txs)
{
    ostringstream ss;
    ss << "[";
    for_each(txs.begin(), txs.end(),
             [&](auto &it) { ss << it << ","; });
    ss << "]";
    return ss.str();
}

Tx Node::create_tx(int data)
{

    auto edge = parent_selection();

    list<UUID> parent_uuids;
    transform(edge.begin(), edge.end(),
              back_inserter(parent_uuids),
              [](auto t) -> UUID { return t.id; });
    auto t = Tx(data, parent_uuids);
    BOOST_LOG_TRIVIAL(trace) << "N" << node_id << " create_tx: " << t;
    receive_tx(*this, t);
    return t;
}

void Node::receive_tx(Node &sender, Tx &tx)
{
    BOOST_LOG_TRIVIAL(trace)
        << "N" << node_id << " received_tx: from=N" << sender.node_id
        << " tx=" << tx;
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
    BOOST_LOG_TRIVIAL(trace)
        << "N" << node_id << " send_tx: " << it->second;
    return it->second;
}

int Node::query(Node &sender, Tx &tx)
{
    receive_tx(sender, tx);
    return is_strongly_prefered(tx) ? 1 : 0;
}

string dump_nodes(const string &what, const vector<shared_ptr<Node>> &nodes)
{
    ostringstream ss;
    ss << what << "= [";
    for_each(nodes.begin(), nodes.end(), [&](auto &it) { ss << it->node_id << " ,"; });
    ss << "]";
    return ss.str();
}

string dump_uids(const string &name, const set<UUID> &uuids)
{
    ostringstream ss;
    ss << name << "={";
    for_each(uuids.begin(), uuids.end(),
             [&](auto &it) { ss << boost::uuids::to_string(it).substr(0, 5) << ","; });
    ss << "}";
    return ss.str();
}

string dump_txs(const tsl::ordered_map<UUID, Tx, boost::hash<UUID>> &txs)
{
    //REMOVEME
    ostringstream ss;
    ss << "transactions={";
    for_each(txs.begin(), txs.end(),
             [&](auto &it) { ss << it.second.strid << ","; });
    ss << "}";
    return ss.str();
}

void Node::avalanche_loop()
{
    // BOOST_LOG_TRIVIAL(trace)
    //     << "N" << node_id
    //     << ": avalanche_loop: " << dump_txs(transactions)
    //     << " " << dump_uids("queried", queried)
    //     << " " << dump_uids("accepted", accepted);

    for (auto it = transactions.begin(); it != transactions.end(); ++it)
    {
        auto &id = it->first;
        auto &tx = it.value();
        // BOOST_LOG_TRIVIAL(trace)
        //     << "avalanche_loop: id=" << boost::uuids::to_string(id).substr(0, 5)
        //     << " tx=" << tx;
        if (queried.find(id) != queried.end())
        {
            // BOOST_LOG_TRIVIAL(trace) << "avalanche loop: queried found tx=" << tx;
            continue;
        }

        vector<shared_ptr<Node>> sample;
        sample.resize(params.k);
        {
            auto tmp{network->nodes};
            auto it(remove_if(tmp.begin(), tmp.end(), [this](auto &it) { return it.get() == this; }));
            tmp.erase(it);
            shuffle(tmp.begin(), tmp.end(), network->rng);
            copy_n(tmp.begin(), params.k, sample.begin());
        }

        int res = 0;
        for (auto &n : sample)
        {
            auto x = tx; // TODO:
            res += n->query(*this, x);
        }
        /*
            if (res >= alpha * k) {
                tx.chit = 1
                // Update the preference for ancestors.
                parentSet(tx).forEach { p ->
                    p.confidence += 1
                }
                parentSet(tx).forEach { p->
                    val cs = conflicts[p.data]!!
                    if (p.confidence > cs.pref.confidence) {
                       cs.pref = p
                    }
                    if (p != cs.last) {
                        cs.last = p
                        cs.count = 0
                    } else {
                        cs.count++
                    }
                }
            } */
        if (res >= params.alpha * params.k)
        {
            tx.chit = 1;
            auto ps = parent_set(tx);
            for (auto ptx : ps)
            {
                ptx.confidence++;
                auto cs = conflicts.find(ptx.data);
                assert(cs != conflicts.end());
                if (ptx.confidence > cs->second.pref.confidence)
                    cs->second.pref = ptx;
                if (ptx != cs->second.last)
                {
                    cs->second.last = ptx;
                    cs->second.count = 0;
                }
                else
                    cs->second.count++;
            }
        }
        queried.insert(id);
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
        if (!is_prefered(it))
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
                int n{10};
                for (auto it = transactions.rbegin(); it < transactions.rend() && n > 0; ++it, --n)
                    rc.push_back(it->second);
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
                return list<Tx>(rc2.begin(), rc2.begin() + (rc2.size() > 3 ? 3 : rc2.size()));
            }
        }()};
    assert(!(parents.empty() && fallback.empty()));
    if (!parents.empty())
        return parents;
    return fallback;
}

double Node::fraction_accepted()
{
    int rc{0};
    for (auto it = transactions.begin(); it != transactions.end(); ++it)
        if (is_accepted(it->second))
            rc++;

    return double(rc) / transactions.size();
}

void Node::dump_dag(const std::string &fname)
{
    ofstream fs;
    fs.open(fname);
    fs << "digraph G{\n";
    for (auto &[id, tx] : transactions)
    {
        auto color = is_accepted(tx) ? "color=lightblue; style=filled;" : "";
        auto c = conflicts.find(tx.data);
        auto pref = (c->second.size > 1 && is_prefered(tx)) ? "*" : "";
        auto chit = queried.find(id) != queried.end() ? to_string(tx.chit) : "?";
        fs << boost::format("\"%s\" [%s  label=\"%d%s, %s, %d\"];\n") % boost::uuids::to_string(tx.id) % color % tx.data % pref % chit % tx.confidence;
    }
    for (auto &[id, tx] : transactions)
    {
        for (auto &p : tx.parents)
        {
            fs << boost::format("\"%s\" -> \"%s\"\n") % boost::uuids::to_string(tx.id) % boost::uuids::to_string(p);
        }
    }
    fs << "}\n";
    fs.close();
}