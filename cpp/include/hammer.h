#pragma once
#include "params.h"
#include "type.h"

static const char hammer_methods_str[NUM_HAMMER_METHOD][20] = {
    "clflush+read",
    "clflush+write",
    "clflushopt+read",
    "clflushopt+write",
    "movnti+read",
    "movnti+write",
    "movntdq+read",
    "movntdq+write"
};

void do_hammer_clflush_read(virtaddr_t v_lst[], int lst_sz, uint64_t hammer_count);
void do_hammer_clflush_write(virtaddr_t v_lst[], int lst_sz, uint64_t hammer_count);
void do_hammer_clflushopt_read(virtaddr_t v_lst[], int lst_sz, uint64_t hammer_count);
void do_hammer_clflushopt_write(virtaddr_t v_lst[], int lst_sz, uint64_t hammer_count);
void do_hammer_movnti_read(virtaddr_t v_lst[], int lst_sz, uint64_t hammer_count);
void do_hammer_movnti_write(virtaddr_t v_lst[], int lst_sz, uint64_t hammer_count);
void do_hammer_movntdq_read(virtaddr_t v_lst[], int lst_sz, uint64_t hammer_count);
void do_hammer_movntdq_write(virtaddr_t v_lst[], int lst_sz, uint64_t hammer_count);

static inline __attribute__((always_inline)) uint64_t rdtsc() {
    uint64_t a, d;
    asm volatile(
        "xor %%rax, %%rax\n"
        "cpuid" ::
            : "rax", "rbx", "rcx", "rdx");
    asm volatile("rdtscp" : "=a"(a), "=d"(d) : : "rcx");
    a = (d << 32) | a;
    return a;
}

static inline __attribute__((always_inline)) uint64_t rdtsc2() {
    uint64_t a, d;
    asm volatile("rdtscp" : "=a"(a), "=d"(d) : : "rcx");
    asm volatile("cpuid" ::: "rax", "rbx", "rcx", "rdx");
    a = (d << 32) | a;
    return a;
}

bank_t get_bank_cnt(hammer_ctx_t& ctx);
bank_t get_bank(hammer_ctx_t& ctx, physaddr_t phys);
row_t get_row(hammer_ctx_t& ctx, physaddr_t phys);
coln_t get_coln(hammer_ctx_t& ctx, physaddr_t phys);

physaddr_t dram_2_phys(dramaddr_t& dram_addr, hammer_ctx_t& ctx);
dramaddr_t phys_2_dram(physaddr_t phys, hammer_ctx_t& ctx);
virtaddr_t phys_2_virt(physaddr_t phys, hammer_ctx_t& ctx);
physaddr_t virt_2_pfn(virtaddr_t virt);
physaddr_t virt_2_phys(virtaddr_t virt);

bool is_mapped_row(hammer_ctx_t& ctx, bank_t bk, row_t row, int aggr_num, int aggr_dist);
void select_hammer_rows(hammer_patt_t* hpt, int aggr_num, row_t r_start, int aggr_dist, bool fixed_gap = true, bool fixed_num = true);

void memset_3bitcycle(uint8_t *dst, uint8_t patt, uint64_t len);
void fill_aggr_rows(hammer_ctx_t& ctx, hammer_patt_t* hpt);
void fill_vict_rows(hammer_ctx_t& ctx, hammer_patt_t* hpt);
void fill_row(hammer_ctx_t& ctx, dramaddr_t d_addr, uint8_t patt, uint8_t bit_cycle);
const char* str_flip_direct(bool one_2_zero);
bool check_flips_row(hammer_ctx_t& ctx, hammer_patt_t* hpt, int patt_index, uint8_t hammer_method, flip_results_map_t& flp_dict);
void clear_flips(flip_results_map_t& flip_map);