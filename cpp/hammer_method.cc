#include "include/hammer.h"

static inline __attribute__((always_inline)) void clflush(virtaddr_t p) {
    asm volatile("clflush (%0)\n" ::"r"(p)
                 : "memory");
}

static inline __attribute__((always_inline)) void clflushopt(virtaddr_t p) {
    asm volatile("clflushopt (%0)\n" ::"r"(p)
                 : "memory");
}

static inline __attribute__((always_inline)) void movnti(virtaddr_t p) {
    asm volatile("movnti %%rax, %0\n" : "=m"(*(uint64_t *)p) :
                 : "memory");
}

static inline __attribute__((always_inline)) void movntdq(virtaddr_t p) {
    asm volatile("movntdq %%xmm0, %0\n" :"=m"(*(uint64_t *)p) :
                 : "memory");
}

static inline __attribute__((always_inline)) void mov_read(virtaddr_t p) {
    asm volatile("mov %0, %%rax\n" : : "m"(*(uint64_t *)p)
                 : "rax");
}

static inline __attribute__((always_inline)) void mov_write(virtaddr_t p) {
    asm volatile("mov %%rax, %0\n" : "=m"(*(uint64_t *)p) :
                 : "memory");
}

static inline __attribute__((always_inline)) void cpuid() {
    asm volatile("cpuid" ::
                     : "rax", "rbx", "rcx", "rdx");
}

static inline __attribute__((always_inline)) void mfence() {
    asm volatile("mfence" ::
                     : "memory");
}

static inline __attribute__((always_inline)) void lfence() {
    asm volatile("lfence" ::
                     : "memory");
}

static inline __attribute__((always_inline)) void sfence() {
    asm volatile("sfence" ::
                     : "memory");
}


void do_hammer_clflush_read(virtaddr_t v_lst[], int lst_sz, uint64_t hammer_count) {
    while (hammer_count--) {
        for (int i = 0; i < lst_sz; i++) {
            mov_read(v_lst[i]);
            clflush(v_lst[i]);
        }
        mfence();
    }
}

void do_hammer_clflush_write(virtaddr_t v_lst[], int lst_sz, uint64_t hammer_count) {
    while (hammer_count--) {
        for (int i = 0; i < lst_sz; ++i) {
            mov_write(v_lst[i]);
            clflush(v_lst[i]);
        }
        mfence();
    }
}

void do_hammer_clflushopt_read(virtaddr_t v_lst[], int lst_sz, uint64_t hammer_count) {

    while (hammer_count--) {
        for (int i = 0; i < lst_sz; ++i) {
            mov_read(v_lst[i]);
            clflushopt(v_lst[i]);
        }
        mfence();
    }
}

void do_hammer_clflushopt_write(virtaddr_t v_lst[], int lst_sz, uint64_t hammer_count) {
    while (hammer_count--) {
        for (int i = 0; i < lst_sz; ++i) {
            mov_write(v_lst[i]);
            clflushopt(v_lst[i]);
        }
        mfence();
    }
}

void do_hammer_movnti_read(virtaddr_t v_lst[], int lst_sz, uint64_t hammer_count) {
    while (hammer_count--) {
        for (int i = 0; i < lst_sz; i++) {
            movnti(v_lst[i]);
            mov_read(v_lst[i]);
        }
        mfence();
    }
}

void do_hammer_movnti_write(virtaddr_t v_lst[], int lst_sz, uint64_t hammer_count) {
    while (hammer_count--) {
        for (int i = 0; i < lst_sz; ++i) {
            movnti(v_lst[i]);
            mov_write(v_lst[i]);
        }
        mfence();
    }
}

void do_hammer_movntdq_read(virtaddr_t v_lst[], int lst_sz, uint64_t hammer_count) {
    while (hammer_count--) {
        for (int i = 0; i < lst_sz; i++) {
            movntdq(v_lst[i]);
            mov_read(v_lst[i]);
        }
    }
}

void do_hammer_movntdq_write(virtaddr_t v_lst[], int lst_sz, uint64_t hammer_count) {
    while (hammer_count--) {
        for (int i = 0; i < lst_sz; ++i) {
            movntdq(v_lst[i]);
            mov_write(v_lst[i]);
        }
        mfence();
    }
}

bool check_flips_row(hammer_ctx_t& ctx, hammer_patt_t* hpt, int patt_index, uint8_t hammer_method, flip_results_map_t& flp_dict) {
    bool has_flip = false;     
    flip_result_t flp = {0};      
    dramaddr_t d_tmp = {0};   
    uint8_t res_tmp[SLICE_SZ];

    for(int i = 0; i < hpt->aggr_num * 2; i++) {
        if(2 == hpt->aggr_dist && i % 2 == 1 && i != hpt->aggr_num * 2 - 1) {
            continue;
        }
        d_tmp.bank = hpt->v_lst[i].bank;
        d_tmp.row = hpt->v_lst[i].row;

        if(hpt->data_patt.bit_cycle[i/2] == 3) {                                                
            memset_3bitcycle(res_tmp, hpt->data_patt.vict_patt[i], SLICE_SZ);                                   
        } else {                                               
            memset(res_tmp, hpt->data_patt.vict_patt[i], SLICE_SZ);                                   
        }

        for (coln_t coln = 0; coln < ROW_SIZE; coln += SLICE_SZ) {
            d_tmp.coln = coln;
            virtaddr_t virt = phys_2_virt(dram_2_phys(d_tmp, ctx), ctx);  
            if (!virt) { continue; }

            clflush((virt));
            cpuid();
            for (int s = 0; s < SLICE_SZ; ++s) {
                if (*(uint8_t*)(virt + s) != res_tmp[s]) {
                    flp.data_pattern = patt_index;
                    flp.v_flip = *(uint8_t*)(virt + s);
                    flp.v_org = res_tmp[s];
                    flp.hammer_method = hammer_method;
                    dramaddr_t d_flip = d_tmp;
                    d_flip.coln += s;
                    flp_dict[dram_2_phys(d_flip, ctx)].push_back(flp);
                    *(uint8_t*)(virt + s) = res_tmp[s];  // restore
                    has_flip = true;
                }
            }
        }
    }
    return has_flip; 
}
