#pragma once
#include "common.h"

typedef uint8_t bank_t;
typedef uint64_t row_t;
typedef uint64_t coln_t;
typedef uint64_t physaddr_t;
typedef uint64_t virtaddr_t;

typedef struct flip_result {
    uint8_t data_pattern;  // data pattern index
    uint8_t hammer_method;
    uint8_t v_org;      // original value
    uint8_t v_flip;      // flipped value
} flip_result_t;

typedef std::map<uint8_t, std::pair<uint64_t, uint64_t>> dram_alloc_info_t;
typedef std::map<physaddr_t, virtaddr_t> phys_virt_map_t;
typedef std::map<physaddr_t, std::vector<flip_result_t>> flip_results_map_t; 

typedef struct data_pattern {
    uint8_t vict_patt[MAX_AGGRESSOR_ROW * 2]; 
    uint8_t aggr_patt[MAX_AGGRESSOR_ROW];
    uint8_t bit_cycle[MAX_AGGRESSOR_ROW]; 
} data_patt_t;

typedef struct ctx_data_patt {
    uint8_t vict_patt[NUM_DATA_PATTERN]; 
    uint8_t aggr_patt[NUM_DATA_PATTERN];
    uint8_t bit_cycle[NUM_DATA_PATTERN]; 
} ctx_data_patt_t;

typedef struct dram_mapping_func {
    int bk_func_cnt;
    uint64_t bk_funcs[32];  // enough
    uint64_t row_mask;
    uint64_t coln_mask;
} dram_mapping_func_t;

typedef struct dramaddr {
    bank_t bank;
    row_t row;
    coln_t coln;
} dramaddr_t;

typedef struct hammer_context {
    dram_mapping_func_t addr_funcs;
    phys_virt_map_t phys_virt_map;
    dramaddr_t mapping_dram_addr;
    ctx_data_patt_t data_patt;
    dram_alloc_info_t occupy_bk_rows;
} hammer_ctx_t;

typedef struct hammer_pattern {
    uint64_t hammer_count; 
    uint8_t hammer_method; 
    int aggr_num;
    int aggr_dist;
    dramaddr_t a_lst[MAX_AGGRESSOR_ROW];  
    dramaddr_t v_lst[MAX_AGGRESSOR_ROW * 2];
    data_patt_t data_patt;
} hammer_patt_t;

typedef struct flip_location {
    physaddr_t vict_phys;
    int offset;
} flip_location_t;

typedef struct flip_info {
    int aggr_num;
    int aggr_dist;
    physaddr_t aggr_phys; // the physical address of the first aggressor row
    std::vector<flip_location_t> flip_location;
} flip_info_t; 