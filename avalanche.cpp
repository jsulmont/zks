#include "avalanche.hpp"
#include <algorithm>
#include <boost/format.hpp>
#include <fstream>

using namespace std;

TxPtr Node::onGenerateTx(int data) {

  auto edge = parentSelection();

  list<UUID> parent_uuids;
  transform(edge.begin(), edge.end(), back_inserter(parent_uuids),
            [](auto t) -> UUID { return t->id; });
  auto t = make_shared<Tx>(data, parent_uuids);
  onReceiveTx(*this, t);
  return t;
}

// onSendTx is an artefact of our simulation
// environment: it is called by a node when it first
// learns from a Tx (e.g., as p)
TxPtr Node::onSendTx(UUID &id) {
  auto it = transactions.find(id);
  assert(it != transactions.end());
  return make_shared<Tx>(*it->second);
}

// lines 5.8 to 5.14
//
void Node::onReceiveTx(Node &sender, TxPtr &tx) {
  // line 5.9: if T ∉ T then
  if (transactions.find(tx->id) == transactions.end()) {
    // new transaction:
    tx->chit = 0;
    tx->confidence = 0;

    // [SIMUL]
    // make sure we know every transactions in tx's ancestors.
    // TODO: check optimization section in paper.
    for (auto &it : tx->parents)
      if (transactions.find(it) == transactions.end()) {
        // simulate the network; this will
        // allocate a new transaction this Node's
        // transaction space
        auto t = sender.onSendTx(it);
        onReceiveTx(sender, t); // recursive
      }

    auto c = conflicts.find(tx->data);
    if (c != conflicts.end())
      c->second.size++;
    else
      conflicts.insert(make_pair(tx->data, ConflictSet{tx, tx, 0, 1}));

    transactions.insert(make_pair(tx->id, tx));
  }
}

int Node::onQuery(Node &sender, TxPtr &tx) {
  onReceiveTx(sender, tx);
  return isStronglyPrefered(tx) ? 1 : 0;
}

void Node::avalancheLoop() {
  for (auto it = transactions.begin(); it != transactions.end(); ++it) {
    auto &T = it.value();

    // line 4.3:  find t that satisfies t ∈ T ∧ t ∉ Q
    if (queried.find(T->id) != queried.end())
      continue;

    // line 4.4:  K := sample(N\u, k)
    vector<shared_ptr<Node>> K;
    {
      auto n = network->nodes;
      auto r(remove_if(n.begin(), n.end(),
                       [this](auto &it) { return it.get() == this; }));
      sample(n.begin(), r, back_inserter(K), params.k, network->rng);
    }

    // line 4.5: P := Σ_(v∈K) query(v,T)
    int P = 0;
    for (auto &n : K) {
      auto newtx = make_shared<Tx>(*T);
      P += n->onQuery(*this, newtx);
    }

    // block for line 4.6 to 4.15
    // line 4.6:  if P ≥ α·k then
    if (P >= params.alpha * params.k) {
      // line 4.7: cT :=1
      T->chit = 1;

      // update the preference for ancestors
      // line 4.9:  for T′∈ T: T′←∗ T  do
      for (auto Tp : parentSet(T)) {
        Tp->confidence++; // missing from figure 4.
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
    queried.insert(T->id);
  }
}

TxSet Node::parentSet(const TxPtr &tx) {
  if (auto it = parentSets.find(tx->id); it != parentSets.end())
    return it->second;

  vector<TxPtr> parents;

  set<UUID> pset(tx->parents.begin(), tx->parents.end());
  while (!pset.empty()) {
    set<UUID> npset;
    for (auto &it : pset)
      if (auto x = transactions.find(it); x != transactions.end()) {
        parents.push_back(x->second);
        for (auto &oid : x->second->parents)
          npset.insert(oid);
      }
    pset = npset;
  }

  TxSet rc(parents.begin(), parents.end());
  parentSets[tx->id] = rc;
  return rc;
}

bool Node::isPrefered(const TxPtr &tx) {
  auto c = conflicts.find(tx->data);
  assert(c != conflicts.end());
  return c->second.pref == tx;
}

bool Node::isStronglyPrefered(const TxPtr &tx) {
  // line 6.4: return ∀T′ ∈ T ,T′ ←∗ T : isPreferred(T′)
  if (auto pset = parentSet(tx); !pset.empty())
    for (auto p = pset.begin(); p != pset.end(); ++p)
      if (!isPrefered(*p))
        return false;
  return true;
}

bool Node::isAccepted(const TxPtr &tx) {
  if (accepted.find(tx->id) != accepted.end())
    return true;
  if (queried.find(tx->id) == queried.end())
    return false;
  auto c{conflicts.find(tx->data)};
  assert(c != conflicts.end());
  auto cs{c->second};
  auto parents_accepted{[tx, this]() {
    for (auto it : tx->parents)
      if (accepted.find(it) == accepted.end())
        return false;
    return true;
  }()};
  auto rc{(parents_accepted && cs.size == 1 && tx->confidence > params.beta1) ||
          (cs.pref == tx && cs.count > params.beta2)};
  if (rc)
    accepted.insert(tx->id);
  return rc;
}

// TODO: check it is the right strategy
// figure 19
//

vector<TxPtr> Node::parentSelection() {
  // Avalanche paper section IV.2: Parent Selection
  //   E = {T : ∀ T ∈ T, isStronglyPreferred(T)}
  //   E′ := {T : |PT|=1 ∨ d(T)>0, ∀T ∈ E}.
  vector<TxPtr> E0, E1;
  for (auto [id, T] : transactions)
    if (isStronglyPrefered(T)) {
      E0.push_back(T);
      auto c = conflicts.find(T->data);
      assert(c != conflicts.end());
      if (c->second.size == 1 || T->confidence > 0)
        E1.push_back(T);
    }

  vector<TxPtr> parents;
  for (auto &it : E1)
    for (auto &jt : parentSet(it))
      if (find(E1.begin(), E1.end(), jt) == E1.end())
        parents.push_back(jt);

  // cout << "parentSelection : eps0.size = " << E0.size() << " eps1.size = "
  //      << E1.size() << " parents.size = " << parents.size() << endl;

  vector<TxPtr> fallback;
  if (transactions.size() == 1)
    fallback.push_back(genesis);
  else {
    vector<TxPtr> tx10;
    int n{10};
    for (auto it = transactions.rbegin(); it != transactions.rend() && n > 0;
         ++it, --n)
      tx10.push_back(it->second);

    vector<TxPtr> tx3;
    for (auto &e : tx10) {
      auto c = conflicts.find(e->data);
      assert(c != conflicts.end());
      if (!isAccepted(e) && c->second.size == 1)
        tx3.push_back(e);
    }
    sample(tx3.begin(), tx3.end(), back_inserter(fallback), 3, network->rng);
  }

  assert(!(parents.empty() && fallback.empty()));
  if (!parents.empty())
    return parents;
  return fallback;
}

double Node::fractionAccepted() {
  int rc{0};
  for (auto it = transactions.begin(); it != transactions.end(); ++it)
    if (isAccepted(it->second))
      rc++;

  return double(rc) / transactions.size();
}

void Node::dumpDag(const std::string &fname) {
  ofstream fs;
  fs.open(fname);
  fs << "digraph G{\n";
  for (auto &[id, tx] : transactions) {
    auto color = isAccepted(tx) ? "color=lightblue; style=filled;" : "";
    auto c = conflicts.find(tx->data);
    auto pref = (c->second.size > 1 && isPrefered(tx)) ? "*" : "";
    auto chit = queried.find(id) != queried.end() ? to_string(tx->chit) : "?";
    fs << boost::format("\"%s\" [%s  label=\"%d%s, %s, %d\"];\n") %
              boost::uuids::to_string(tx->id) % color % tx->data % pref % chit %
              tx->confidence;
  }
  for (auto &[id, tx] : transactions) {
    for (auto &p : tx->parents) {
      fs << boost::format("\"%s\" -> \"%s\"\n") %
                boost::uuids::to_string(tx->id) % boost::uuids::to_string(p);
    }
  }
  fs << "}\n";
  fs.close();
}