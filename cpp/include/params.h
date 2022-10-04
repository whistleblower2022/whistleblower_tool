#pragma once

#include "type.h"
#include <getopt.h>

typedef struct ProfileParams {
    char ini_filename[100];
    bool is_data_pattern_fixed;
	uint8_t data_pattern; 
    bool is_hammer_method_fixed;
    uint8_t hammer_method;
    bool is_aggr_row_fixed;
    row_t first_aggr_row;
    bool is_aggr_bank_fixed;
    bank_t aggr_bank;
    bool is_aggr_dist_fixed;
    uint8_t aggr_dist;
    bool is_aggr_num_fixed;
    uint8_t aggr_num;
    uint64_t seconds_to_hammer;     
    uint64_t hammer_count;
} ProfileParams;

int parse_params(int argc, char **argv, ProfileParams *params);
