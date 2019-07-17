#pragma once
//
//
struct Parameters
{
    int num_transactions = 20;
    double double_spend_ratio = 0.02;
    double alpha = 0.8;
    int num_nodes = 50;
    int k = 1 + num_nodes / 10;
    int beta1 = 5;
    int beta2 = 5;
    unsigned long seed = 23L;
    bool dump_dags = false;
    bool verbose = false;
};
