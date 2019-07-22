#pragma once
#include "cxxopts.hpp"
//
//
struct Parameters
{
    int num_transactions = 20;
    double double_spend_ratio = 0.03;
    double alpha = 0.8;
    int num_nodes = 50;
    int k = 1 + num_nodes / 10;
    int beta1 = 5;
    int beta2 = 5;
    unsigned long seed = 23L;
    bool dump_dags = false;
    bool verbose = false;
};

inline Parameters
parse_options(int argc, char *argv[])
{
    Parameters p;
    try
    {
        cxxopts::Options options(argv[0], " - simulation for the avalanche protocol");
        options.add_options()("h,help", "display help and exit");
        options.add_options()("a,alpha", "The alpha parameter", cxxopts::value<double>()->default_value("0.8"));
        options.add_options()("beta1", "The beta1 parameter", cxxopts::value<double>()->default_value("0.8"));
        options.add_options()("beta2", "The beta2 parameter", cxxopts::value<double>()->default_value("0.8"));
        options.add_options()("d,double-spend-ratio", "The double spend ratio", cxxopts::value<double>()->default_value("0.02"));
        options.add_options()("k,sample-size", "The sample size (default `1 + nrNodes / 10`)", cxxopts::value<int>());
        options.add_options()("n,num-transactions", "nunber of tx to generate", cxxopts::value<int>()->default_value("20"));
        options.add_options()("num-nodes", "number of nodes to simulate", cxxopts::value<int>()->default_value("50"));
        options.add_options()("seed", "seed random generation", cxxopts::value<int>()->default_value("23"));
        options.add_options()("dump-dags", "dump dags in dot format", cxxopts::value<int>()->default_value("false"));

        auto result = options.parse(argc, argv);

        if (result.count("help"))
        {
            std::cout << options.help({"", "Group"}) << std::endl;
            exit(0);
        }
        if (result.count("alpha"))
            p.alpha = result["alpha"].as<double>();
        if (result.count("beta1"))
            p.beta1 = result["beta1"].as<double>();
        if (result.count("beta2"))
            p.beta2 = result["beta2"].as<double>();
        if (result.count("double-spend-ratio"))
            p.double_spend_ratio = result["double-spend-ratio"].as<double>();
        if (result.count("num-nodes"))
            p.num_nodes = result["num-nodes"].as<int>();
        if (result.count("sample-size"))
            p.k = result["sample-size"].as<int>();
        else
            p.k = 1 + p.num_nodes / 10;
        if (result.count("num-transactions"))
            p.num_transactions = result["num-transactions"].as<int>();
        if (result.count("seed"))
            p.seed = result["seed"].as<int>();
    }
    catch (const cxxopts::OptionException &e)
    {
        std::cout << "error parsing options: " << e.what() << std::endl;
        exit(1);
    }
    return p;
}