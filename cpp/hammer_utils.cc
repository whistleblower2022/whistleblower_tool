#include "include/hammer.h"

bool is_mapped_row(hammer_ctx_t& ctx, bank_t bk, row_t row, int aggr_num, int aggr_dist) {
    dramaddr_t a_daddr = {.bank = bk}, v_daddr_m = {.bank = bk}, v_daddr_p = {.bank = bk};
    for (int i = 0; i < aggr_num; i++) {
        a_daddr.row = row + i * aggr_dist;
        v_daddr_m.row = a_daddr.row - 1; 
        v_daddr_p.row = a_daddr.row + 1;
        for (coln_t col = 0; col < ROW_SIZE; col += SLICE_SZ) {
            a_daddr.coln = col;
            v_daddr_m.coln = col; 
            v_daddr_p.coln = col;
            if (!phys_2_virt(dram_2_phys(a_daddr, ctx), ctx)) return false;
            if (!phys_2_virt(dram_2_phys(v_daddr_m, ctx), ctx)) return false;
            if (!phys_2_virt(dram_2_phys(v_daddr_p, ctx), ctx)) return false;
        }
    } 
    return true;
}

bank_t get_bank_cnt(hammer_ctx_t& ctx) {
    return (1ul << ctx.addr_funcs.bk_func_cnt);
}

bank_t get_bank(hammer_ctx_t& ctx, physaddr_t phys) {
    bank_t bk = 0;
    for (uint8_t i = 0; i < ctx.addr_funcs.bk_func_cnt; ++i) {
        bk += __builtin_parity(phys & ctx.addr_funcs.bk_funcs[i]) << i;
    }
    return bk;
}

row_t get_row(hammer_ctx_t& ctx, physaddr_t phys) {
    return phys >> __builtin_ctzl(ctx.addr_funcs.row_mask);
}

coln_t get_coln(hammer_ctx_t& ctx, physaddr_t phys) {
    return phys & ctx.addr_funcs.coln_mask;
}

physaddr_t dram_2_phys(dramaddr_t& dram_addr, hammer_ctx_t& ctx) {
    physaddr_t phys_addr = 0;
    dram_mapping_func_t& addr_funcs = ctx.addr_funcs;
    phys_addr = (dram_addr.row << __builtin_ctzl(addr_funcs.row_mask));
    phys_addr |= (dram_addr.coln << __builtin_ctzl(addr_funcs.coln_mask));

    for (int i = 0; i < addr_funcs.bk_func_cnt; i++) {
        uint64_t masked_addr = phys_addr & addr_funcs.bk_funcs[i];
        if (__builtin_parity(masked_addr) == ((dram_addr.bank >> i) & 1L)) {
            continue;
        }
        uint64_t h_lsb = __builtin_ctzl((addr_funcs.bk_funcs[i]) &
                                        ~(addr_funcs.coln_mask) &
                                        ~(addr_funcs.row_mask));
        phys_addr ^= 1 << h_lsb;
    }

    return phys_addr;
}

dramaddr_t phys_2_dram(physaddr_t phys, hammer_ctx_t& ctx) {
    dramaddr_t d = {0};
    dram_mapping_func_t& f = ctx.addr_funcs;
    for (int i = 0; i< f.bk_func_cnt; ++i) {
        d.bank |= (__builtin_parityl(phys & f.bk_funcs[i]) << i);
    }

    d.row = get_row(ctx, phys);
    d.coln = get_coln(ctx, phys);

    return d;
}

virtaddr_t phys_2_virt(physaddr_t phys, hammer_ctx_t& ctx) {
    static size_t g_miss_cnt = 0;
    physaddr_t phys_page = phys & ~PAGE_MASK;
    if (ctx.phys_virt_map.find(phys_page) == ctx.phys_virt_map.end()) {
        return 0;
    }
    virtaddr_t virt = ctx.phys_virt_map[phys_page] + (phys & PAGE_MASK);
    return virt;
}

#define SHIFT_SWAPPED (62)
#define MASK_SWAPPED (1ul << SHIFT_SWAPPED)
physaddr_t virt_2_pfn(virtaddr_t virtual_addr) {
    int fd = open("/proc/self/pagemap", O_RDONLY);
    assert(fd >= 0);
    off_t pos = lseek(fd, (virtual_addr / PAGE_SIZE) * 8, SEEK_SET);
    assert(pos >= 0);
    uint64_t value;
    int got = read(fd, &value, 8);
    assert(got == 8);
    int rc = close(fd);
    assert(rc == 0);

    uint64_t page_frame_number = value & ((1ul << 54) - 1);
    if (((value & MASK_SWAPPED) >> SHIFT_SWAPPED) & 1) {
        dbg_printf(
            "[-] %s. (v) 0x%lx => (p) 0x%lx has been swapped out. value: 0x%lx "
            "phys obtained is incorrect.\n",
            __func__, virtual_addr, page_frame_number, value);
    }
    return page_frame_number;
}

physaddr_t virt_2_phys(virtaddr_t virtual_addr) {
    int fd = open("/proc/self/pagemap", O_RDONLY);
    assert(fd >= 0);
    off_t pos = lseek(fd, (virtual_addr / PAGE_SIZE) * 8, SEEK_SET);
    assert(pos >= 0);
    uint64_t value;
    int got = read(fd, &value, 8);
    assert(got == 8);
    int rc = close(fd);
    assert(rc == 0);

    uint64_t frame_num = value & ((1ul << 54) - 1);
    uint64_t phys_addr =
        (frame_num * PAGE_SIZE) | (virtual_addr & (PAGE_SIZE - 1));

    if (((value & MASK_SWAPPED) >> SHIFT_SWAPPED) & 1) {
        dbg_printf(
            "[-] %s. (v) 0x%lx => (p) 0x%lx has been swapped out. value: 0x%lx "
            "phys obtained is incorrect.\n",
            __func__, virtual_addr, phys_addr, value);
    }

    return phys_addr;
}

void memset_3bitcycle(uint8_t *dst, uint8_t patt, uint64_t len) {
    uint8_t val_cycle[3] = {patt, static_cast<uint8_t>(patt >> 1 | ((patt >> 2) & 1) << 7), static_cast<uint8_t>(patt << 1 | ((patt >> 2) & 1))};
    for (int i = 0; i < len / sizeof(uint8_t); i++) {
        dst[i] = val_cycle[i % 3];
    }
}

void fill_row(hammer_ctx_t& ctx, dramaddr_t d_addr, uint8_t patt, uint8_t bit_cycle) {
    dramaddr_t d_tmp = d_addr;
    physaddr_t phys = 0;
    virtaddr_t virt = 0;
    for (uint64_t coln = 0; coln < ROW_SIZE; coln += SLICE_SZ) {
        d_tmp.coln = coln;
        phys = dram_2_phys(d_tmp, ctx);
        virt = phys_2_virt(phys, ctx);
        if (virt) {
            if(3 == bit_cycle) {
                // killer pattern: 49 24 92 or b6 db 6d (3bit cycles)
                memset_3bitcycle((uint8_t*)virt, patt, SLICE_SZ);
            } else {
                memset((char*)virt, patt, SLICE_SZ);
            }
        }
    }
    #if verbose
    dbg_printf("[+] %s. filling row: r%ld with %02x\n", __func__, d_addr.row, pat);
    #endif
}

void select_hammer_rows(hammer_patt_t* hpt, int aggr_num, row_t r_start, int aggr_dist, bool fixed_aggr_dist, bool fixed_num) {    
    hpt->aggr_num = fixed_num ? aggr_num : rand() % (MAX_AGGRESSOR_ROW - 2) + 2;     
    hpt->aggr_dist = fixed_aggr_dist ?  aggr_dist : rand() % MAX_AGGRESSOR_DIST;      
    for (int i = 0; i < hpt->aggr_num; ++i) {         
        hpt->a_lst[i] = {.bank = 0, .row = r_start + i * hpt->aggr_dist, .coln = 0};      
        hpt->v_lst[i * 2] = {.bank = 0, .row = hpt->a_lst[i].row - 1, .coln = 0};         
        hpt->v_lst[i * 2 + 1] = {.bank = 0, .row = hpt->a_lst[i].row + 1, .coln = 0};     
    } 
}

void fill_aggr_rows(hammer_ctx_t& ctx, hammer_patt_t* hpt) {
    for (int i = 0; i < hpt->aggr_num; ++i) {
        fill_row(ctx, hpt->a_lst[i], hpt->data_patt.aggr_patt[i], hpt->data_patt.bit_cycle[i]);
    }
}

void fill_vict_rows(hammer_ctx_t& ctx, hammer_patt_t* hpt) {
    for (int i = 0; i < hpt->aggr_num; ++i) {
        fill_row(ctx, hpt->v_lst[i * 2], hpt->data_patt.vict_patt[i * 2], hpt->data_patt.bit_cycle[i]);
    }   
    if (hpt->aggr_dist > 2) {
        for (int i = 0; i < hpt->aggr_num; ++i) {
            fill_row(ctx, hpt->v_lst[i * 2 + 1], hpt->data_patt.vict_patt[i * 2 + 1], hpt->data_patt.bit_cycle[i]);
        }
    } else {
        int index = hpt->aggr_num * 2 - 1;
        fill_row(ctx, hpt->v_lst[index], hpt->data_patt.vict_patt[index], hpt->data_patt.bit_cycle[hpt->aggr_num - 1]);
    }
}

const char* str_flip_direct(bool one_2_zero) {
   return one_2_zero ? "1 -> 0" : "0 -> 1";
}

void clear_flips(flip_results_map_t& flip_map) {
    if (flip_map.size() == 0) return;
    for (auto it = flip_map.begin(); it != flip_map.end(); ++it) {
        it->second.clear();
    }
    flip_map.clear();
}