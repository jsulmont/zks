#include <fstream>
#include <algorithm>
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

Tx Node::onGenerateTx(int data)
{

    auto edge = parentSelection();

    list<UUID> parent_uuids;
    transform(edge.begin(), edge.end(),
              back_inserter(parent_uuids),
              [](auto t) -> UUID { return t->id; });
    auto t = Tx(data, parent_uuids);
    BOOST_LOG_TRIVIAL(trace) << "N" << node_id << " onGenerateTx: " << t;
    onReceiveTx(*this, t);
    return t;
}

void Node::onReceiveTx(Node &sender, Tx &tx)
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
            auto t = sender.onSendTx(it);
            onReceiveTx(sender, t); // recursive
        }
    auto c = conflicts.find(tx.data);
    if (c != conflicts.end())
        c->second.size++;
    else
        conflicts.insert(make_pair(tx.data, ConflictSet{&tx, &tx, 0, 1}));
    transactions.insert(make_pair(tx.id, tx));
}

Tx Node::onSendTx(UUID &id)
{
    auto it = transactions.find(id);
    assert(it != transactions.end());
    BOOST_LOG_TRIVIAL(trace)
        << "N" << node_id << " onSendTx: " << it->second;
    return it->second;
}

int Node::onQuery(Node &sender, Tx &tx)
{
    onReceiveTx(sender, tx);
    return isStronglyPrefered(&tx) ? 1 : 0;
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
    //TODO: remove
    ostringstream ss;
    ss << "transactions={";
    for_each(txs.begin(), txs.end(),
             [&](auto &it) { ss << it.second.strid << ","; });
    ss << "}";
    return ss.str();
}

void Node::avalancheLoop()
{
    for (auto it = transactions.begin(); it != transactions.end(); ++it)
    {
        auto &T = it.value();
        // line 4.3:  find T that satisfies T ∈ T ∧ T ∈/ Q
        if (queried.find(T.id) != queried.end())
            continue;

        // line 4.4:  K := sample(N \u, k)
        vector<shared_ptr<Node>> K;
        K.resize(params.k);
        {
            auto tmp{network->nodes};
            auto it(remove_if(tmp.begin(), tmp.end(), [this](auto &it) { return it.get() == this; }));
            tmp.erase(it);
            shuffle(tmp.begin(), tmp.end(), network->rng);
            copy_n(tmp.begin(), params.k, K.begin());
        }

        // line 4.5: P := Σ_(v∈K) query(v,T)
        int P = 0;
        for (auto &n : K)
            P += n->onQuery(*this, T);

        BOOST_LOG_TRIVIAL(trace) << "avalancheLoop: sum=" << P;

        // block for line 4.6 to 4.15
        // line 4.6:  if P ≥ α·k then
        if (P >= params.alpha * params.k)
        {
            // line 4.7: cT :=1
            T.chit = 1;
            auto ancestors = parentSet(&T);

            // update the preference for ancestors
            // line 4.9:  for T′∈ T: T′←∗ T  do
            for (auto Tp : ancestors)
            {
                [](const Tx *tp) { const_cast<Tx *>(tp)->confidence++; }(Tp);
                //Tp->confidence++;
                auto cs = conflicts.find(Tp->data);
                assert(cs != conflicts.end());

                // line 4.10: if d(T′) > d(PT′.pref) then
                if (Tp->confidence > cs->second.pref->confidence)
                    // line 4.11: PT′.pref := T′
                    cs->second.pref = Tp;

                // line 4.12: if T′ ≠ PT′.last then
                if (Tp != cs->second.last)
                    // line 4.13: PT′.last :=  T′, PT′.cnt := 0
                    cs->second.last = Tp, cs->second.count = 0;
                else
                    // line 4.15: ++PT′.cnt
                    cs->second.count++;
            }
        }
        queried.insert(T.id);
    }
}

TxSet Node::parentSet(const Tx *tx)
{
    if (auto it = parentSets.find(tx->id); it != parentSets.end())
        return it->second;

    vector<const Tx *> parents;
    set<UUID> ps(tx->parents.begin(), tx->parents.end());
    while (!ps.empty())
    {
        for (auto &it : ps)
        {
            auto x = transactions.find(it);
            if (x != transactions.end())
                parents.push_back(&x->second);
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
    TxSet rc(parents.begin(), parents.end(), tx_compare());
    parentSets[tx->id] = rc;
    return rc;
}

bool Node::isPrefered(const Tx *tx)
{
    if (auto it = conflicts.find(tx->data); it != conflicts.end())
        return it->second.pref == tx;
    else
        return false;
}

bool Node::isStronglyPrefered(const Tx *tx)
{
    if (auto pset = parentSet(tx); !pset.empty())
        for (auto &&p = pset.begin(); p != pset.end(); ++p)
            if (!isPrefered(*p))
                return false;
    return true;
}

bool Node::isAccepted(const Tx *tx)
{
    if (accepted.find(tx->id) != accepted.end())
        return true;
    if (queried.find(tx->id) == queried.end())
        return false;
    auto it{conflicts.find(tx->data)};
    assert(it != conflicts.end());
    auto cs{it->second};
    auto parents_accepted{
        [tx, this]() {
            for (auto it : tx->parents)
                if (accepted.find(it) == accepted.end())
                    return false;
            return true;
        }()};
    auto rc{
        (parents_accepted && cs.size == 1 && tx->confidence > params.beta1) ||
        (cs.pref == tx && cs.count > params.beta2)};
    if (rc)
        accepted.insert(tx->id);
    return rc;
}

ostream &operator<<(ostream &out, const TxSet &st)
{
    out << "[";
    for_each(st.begin(), st.end(), [&](auto &e) { out << e->strid << ","; });
    out << "]";
    return out;
}

TxSet Node::parentSelection()
{
    // Avalanche paper section IV.2: Parent Selection
    //   E = {T : ∀ T ∈ T, isStronglyPreferred(T)}
    //   E′ := {T : |PT|=1 ∨ d(T)>0, ∀T ∈ E}.
    TxSet E0, E1;

    for (auto [id, T] : transactions)
        if (isStronglyPrefered(&T))
        {
            E0.insert(&T);
            auto c = conflicts.find(T.data);
            assert(c != conflicts.end());
            if (c->second.size == 1 || T.confidence > 0)
                E1.insert(&T);
        }

    TxSet parents;
    for (auto &it : E1)
        for (auto &jt : parentSet(it))
            if (auto f = find(E1.begin(), E1.end(), jt); f != E1.end())
                parents.insert(jt);

    TxSet fallback;
    if (transactions.size() == 1)
        fallback.insert(&tx_genesis);
    else
    {
        /* transactions.values.reversed()
           .take(10)
           .filter{!isAccepted(it) && conflicts[it.data]!!.size == 1}
           .shuffled(network.rng)
           .take(3)*/
        //   TODO: use std::range
        vector<const Tx *> tx10;
        int n{10};
        for (auto it = transactions.rbegin(); it != transactions.rend() && n > 0; ++it, --n)
            tx10.push_back(&it->second);
        vector<const Tx *> tx3;
        for (auto &e : tx10)
        {
            auto c = conflicts.find(e->data);
            assert(c != conflicts.end());
            if (c->second.size == 1)
                tx3.push_back(e);
        }
        shuffle(tx3.begin(), tx3.end(), network->rng);
        // list<const Tx *> rc(tx3.begin(), tx3.begin() + (tx3.size() > 3 ? 3 : tx3.size()));
        // //        fallback.insert(rc.begin(), rc.end());
        fallback.insert(tx3.begin(), tx3.begin() + (tx3.size() > 3 ? 3 : tx3.size()));
    }

    assert(!(parents.empty() && fallback.empty()));
    BOOST_LOG_TRIVIAL(trace)
        << "N" << node_id
        << " E0=" << E0
        << " E1=" << E1
        << " P=" << parents
        << " F=" << fallback;

    if (!parents.empty())
        return parents;
    return fallback;
}

double Node::fractionAccepted()
{
    int rc{0};
    for (auto it = transactions.begin(); it != transactions.end(); ++it)
        if (isAccepted(&it->second))
            rc++;

    return double(rc) / transactions.size();
}

void Node::dumpDag(const std::string &fname)
{
    ofstream fs;
    fs.open(fname);
    fs << "digraph G{\n";
    for (auto &[id, tx] : transactions)
    {
        auto color = isAccepted(&tx) ? "color=lightblue; style=filled;" : "";
        auto c = conflicts.find(tx.data);
        auto pref = (c->second.size > 1 && isPrefered(&tx)) ? "*" : "";
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