# test for ZkS

This code implements a C++ simulation of [avalanche algorithm](https://ipfs.io/ipfs/QmUy4jh5mGNZvLkjies1RWM4YuvJh5o2FYopNPVYwrRVGV).
It is based on/inspired by [this code](https://github.com/thschroeter/avalanche).
Likewise in the original version, the `--dump-dags` option will dump (in the current directory) the internal DAG of the first node everytime a transaction is created. You might want to install the `dot` program, e.g.,
```
brew install dot
```
In these graph, each vertice is labelled with the triplet `<data, chit, confidence>`, and accepted transactions are displayed as lightblue/filled labels. `prefered` transactions have an `*` appended to their `data`. The `chit` is displayed as `0` or `1`, or `?` when the transaction hasn't been queried by the node.

## Network simulation
For the sake of simplicity, the protocol is simulated using a sychronous network: at every tick `i` of the network, the following steps are performed:
```
1. N <- random node; generate transaction `i` on node `N`
2. if rand(0,1) < params.double-spend-ratio then
3.    chose an random integer j in [0,`i`[ and a random node `M`;
      generate transaction `j` on `M`
4. run `avalancheLoop` (see below)
```

## Avalanche Loop
The main loop of the algorithm is given below (see original paper for other procedures it uses):
![alt text)(https://raw.githubusercontent.com/jsulmont/zks/master/internal/fig4.png)

## UTXO
UTXO are simulated as integers, ranging from `0` to `parameters.num_transactions`.
The program is able to simulate the double spending problem by randomly emitting "an already" spent transaction (i.e., re-emmiting a transaction for an value already used), thus creating a conflicting transaction. At the end of the simulation, for each conflicting transactions `tx1` and `tx2` the program checks that either only one or none (i.e., not both) of the conflicting transactions have been accepted by all the nodes (cf. paper). Accepted transactions are printed within brackets.


## to build (assuming you've cloned this repo)
You need `cmake` to build this:
```
brew install cmake
```
or 
```
apt-get install cmake
```
then
```
git submodule update --init
make re build docker
```

## how to run

```
zeus% docker run --rm jansulmont/zks --help
 - simulation for the avalanche protocol
Usage:
  /app/zks [OPTION...]

  -h, --help                    display help and exit
  -a, --alpha arg               The alpha parameter (default: 0.8)
      --beta1 arg               The beta1 parameter (default: 0.8)
      --beta2 arg               The beta2 parameter (default: 0.8)
  -d, --double-spend-ratio arg  The double spend ratio (default: 0.02)
  -k, --sample-size arg         The sample size (default `1 + nrNodes / 10`)
  -n, --num-transactions arg    nunber of tx to generate (default: 20)
      --num-nodes arg           number of nodes to simulate (default: 50)
      --seed arg                seed random generation (default: 12345)
      --dump-dags               dump dags in dot format

```
To run:
```
docker run -v$(pwd):/data  --rm -it jansulmont/zks --dump-dags
for f in znode-*-*.dot; do dot -Tpng -O $f; done
```


