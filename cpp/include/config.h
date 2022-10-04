#pragma once
#include "common.h"

int read_config(const char* fname, std::vector<int> &vec_row_bits,
    std::vector<int> &vec_coln_bits, std::vector<std::vector<int>> &addr_funcs);