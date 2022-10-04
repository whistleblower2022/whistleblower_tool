#include "include/hammer.h"
#include "include/config.h"
#include "include/params.h"

uint64_t g_seconds_to_hammer;
struct timeval g_hammer_start = {0};

#ifdef USE_MULT_THREAD
std::condition_variable cv;
std::mutex mtx;
bool thread_status[NUM_THREADS] = {0};
bool check_thread_status(bool t_lst[], uint64_t lst_sz, bool value) {
    for (int i = 0; i < lst_sz; i++) {
        if(t_lst[i] != value) {
            return false;
        }
    }
    return true;
}
#endif

uint64_t do_hammer_common(virtaddr_t a_lst[], int lst_sz, uint64_t hammer_count, int hammer_method, int cpu = -1) {
    #ifdef USE_MULT_THREAD
        if (cpu != -1) {
            cpu_set_t cs;
            CPU_ZERO(&cs);
            CPU_SET(cpu, &cs);
            if (pthread_setaffinity_np(pthread_self(), sizeof(cs), &cs) < 0) {
                perror("pthread_setaffinity_np");
            }

            {
                std::unique_lock <std::mutex> ulk(mtx);
                thread_status[cpu] = true;
                cv.wait(ulk);
            }
            thread_status[cpu] = false;
        }
    #endif

    void (*f[NUM_HAMMER_METHOD])(virtaddr_t[], int, uint64_t) = {         
        do_hammer_clflush_read
        , do_hammer_clflush_write
        , do_hammer_clflushopt_read
        , do_hammer_clflushopt_write
        , do_hammer_movnti_read
        , do_hammer_movnti_write
        , do_hammer_movntdq_read
        , do_hammer_movntdq_write         
    };
    #ifndef USE_MULT_THREAD
        uint64_t start = rdtsc();
    #endif

    (*f[hammer_method])(a_lst, lst_sz, hammer_count);

    #ifndef USE_MULT_THREAD
        uint64_t end = rdtsc2();
        return (end - start);
    #else
        return 0;
    #endif
}
#ifdef USE_MULT_THREAD
uint64_t do_hammer_threads(virtaddr_t a_lst[], int lst_sz, uint64_t hammer_count, int hammer_method) {
    uint64_t start = rdtsc();
    std::thread threads[NUM_THREADS];
    virtaddr_t hh_lst[NUM_THREADS][MAX_AGGRESSOR_ROW] = {0};
    int llst_sz[NUM_THREADS] = {0};
    int cache_line_sz = 256;
    for (int i = 0; i < lst_sz; i++) {
        // a thread -> a set of all addresses
        for (int j = 0; j < NUM_THREADS; j++) {
            hh_lst[j][i] = a_lst[i] + cache_line_sz * j;
        }

        // a thread -> a part of addresses
        // hh_lst[i % NUM_THREADS][i / NUM_THREADS] = a_lst[i];
        // llst_sz[i % NUM_THREADS]++;
    }

    cpu_set_t cs;
    CPU_ZERO(&cs);
    CPU_SET(NUM_THREADS, &cs);
    if (pthread_setaffinity_np(pthread_self(), sizeof(cs), &cs) < 0) {
        perror("pthread_setaffinity_np");
    }

    for (int i = 0; i < NUM_THREADS; ++i) {
        // a thread -> a set of all addresses
        threads[i] = std::thread(do_hammer_common, hh_lst[i], lst_sz, hammer_count, hammer_method, i);
        
        // a thread -> a part of addresses
        // threads[i] = std::thread(do_hammer_common, hh_lst[i], llst_sz[i], hammer_count, hammer_method, i);
    }
    {
        while (!check_thread_status(thread_status, NUM_THREADS, true));
        while (!check_thread_status(thread_status, NUM_THREADS, false)) {
            cv.notify_all();
        }
    }
    for (auto& t: threads) {
        t.join();
    }
    uint64_t end = rdtsc2();
    return end - start;  
}
#endif
bool hammer_rows(hammer_ctx_t& ctx, hammer_patt_t* hpt) {
    assert(hpt->aggr_num <= MAX_AGGRESSOR_ROW);
    virtaddr_t v_lst[MAX_AGGRESSOR_ROW] = {0};
    for (int i = 0; i < hpt->aggr_num; ++i) {
        v_lst[i] = phys_2_virt(dram_2_phys(hpt->a_lst[i], ctx), ctx);
        if (v_lst[i] == 0) {
            return false;
        }
    }

    uint64_t span = 0;
#ifdef USE_MULT_THREAD
    span = do_hammer_threads((virtaddr_t*)v_lst, hpt->aggr_num, hpt->hammer_count, hpt->hammer_method);
#else
    span = do_hammer_common((virtaddr_t*)v_lst, hpt->aggr_num, hpt->hammer_count, hpt->hammer_method);
#endif
    dbg_printf("%ld ", span/1000000);
    
    return true;
}

void report_flips(hammer_ctx_t& ctx, flip_results_map_t& flip_map) {
    uint8_t* vict_patt = ctx.data_patt.vict_patt;
    uint8_t* aggr_patt = ctx.data_patt.aggr_patt;
    int data_pattern = 0;
    bool one_2_zero = false;
    for (auto it = flip_map.begin(); it != flip_map.end(); ++it) {
        dramaddr_t d = phys_2_dram(it->first, ctx);
        std::vector<flip_result_t>& flip = it->second;
        
        dbg_printf("\n[+] Found Flip. pa: %lx. bank: %d, row: r%ld, col: c%ld. Flip Patterns Count: %ld\n", 
            it->first, d.bank, d.row, d.coln, flip.size());
        for (size_t i = 0; i < flip.size(); ++i) {
            data_pattern = flip[i].data_pattern;
            one_2_zero = (__builtin_popcount(flip[i].v_org) > __builtin_popcount(flip[i].v_flip));
            dbg_printf("\t %16s: %2d (%02x/%02x/%02x): [%02x => %02x] (%s)\n", hammer_methods_str[flip[i].hammer_method],
                       data_pattern, aggr_patt[data_pattern], vict_patt[data_pattern], aggr_patt[data_pattern],
                       flip[i].v_org, flip[i].v_flip, str_flip_direct(one_2_zero));
        }
    }  
}

std::vector<flip_info_t> flip_info_array;

int flip_info_array_size(std::vector<flip_info_t> flip_info_array) {
    int count = 0;
    for (auto it : flip_info_array) {
        count += it.flip_location.size();
    }
    return count;
}

int insert_flip_info_array(hammer_ctx_t& ctx, hammer_patt_t& hpt, flip_results_map_t& flip_res) {
    flip_info_t flip_info = {.aggr_num = hpt.aggr_num, .aggr_dist = hpt.aggr_dist, .aggr_phys = dram_2_phys(hpt.a_lst[0], ctx)};
    for (auto it : flip_res) {
        flip_location_t flip_location = {.vict_phys = it.first,
             .offset = it.second[0].v_flip ^ it.second[0].v_org};
        flip_info.flip_location.push_back(flip_location);
    }
    flip_info_array.push_back(flip_info);
    return flip_info_array_size(flip_info_array);
}

void helper_write_flip_info(std::vector<flip_info_t>& flip_info_array) {
    FILE* fx = fopen("../log/flip_info.log", "w");
    assert(fx);
    int len = flip_info_array.size();
    for (int i = 0; i < len; i++) {
        fprintf(fx, "[aggr_num]:\n%d\n", flip_info_array[i].aggr_num);
        fprintf(fx, "[aggr_dist]:\n%d\n", flip_info_array[i].aggr_dist);
        fprintf(fx, "[aggr_phys]:\n%lu\n", flip_info_array[i].aggr_phys);
        for (int j = 0; j < flip_info_array[i].flip_location.size(); j++) {
            fprintf(fx, "[vict_phys]:\n%lu %d\n", flip_info_array[i].flip_location[j].vict_phys, flip_info_array[i].flip_location[j].offset);
        }
        fprintf(fx, "[end]\n");
    }
    fclose(fx);
}

void helper_read_flip_info(std::vector<flip_info_t>& flip_info_array) {
    FILE* fx = fopen("../log/flip_info.log", "r");
    assert(fx);
    char* line = NULL;
    size_t lblen = 0;
    int ret = 0;
    ssize_t llen = 0;
    flip_info_t flip_info;
    while ((llen = getline(&line, &lblen, fx)) != -1) {
        if (strstr(line, "[end]")) { 
            flip_info_array.push_back(flip_info);
            flip_info.flip_location.clear(); 
            ret = 0;
        }
        else if (strstr(line, "[aggr_num]:")) { ret = 1; }
        else if (strstr(line, "[aggr_dist]:")) { ret = 2; }
        else if (strstr(line, "[aggr_phys]:")) { ret = 3; }
        else if (strstr(line, "[vict_phys]:")) { ret = 4; }
        else {
            if (1 == ret) { flip_info.aggr_num = atoi(line); }
            else if (2 == ret) { flip_info.aggr_dist = atoi(line); }
            else if (3 == ret) { flip_info.aggr_phys = strtoul(line, NULL, 10); }
            else if (4 == ret) {
                flip_location_t flip_location;
                char *tmp;
                flip_location.vict_phys = strtoul(line, &tmp, 10);
                flip_location.offset = strtoul(tmp, NULL, 10);
                flip_info.flip_location.push_back(flip_location);
            }
        }
    }

    fclose(fx);
}

void hammer_repeat(hammer_ctx_t& ctx, ProfileParams params, std::vector<flip_info_t>& flip_info_array, int cpu = -1) {
    uint8_t* vict_patt_array = ctx.data_patt.vict_patt;
    uint8_t* aggr_patt_array = ctx.data_patt.aggr_patt;
    uint8_t* bit_cycle_array = ctx.data_patt.bit_cycle;

    int patt_num = params.data_pattern + 2;
    if (params.data_pattern % 2 != 0) {
        return;
    }
    int aggr_num = params.is_aggr_num_fixed? params.aggr_num : MAX_AGGRESSOR_ROW;
    int hmmd_num = params.is_hammer_method_fixed ? params.hammer_method + 1 : NUM_HAMMER_METHOD;
    int aggr_dist = params.aggr_dist;
    int bank_num = params.is_aggr_bank_fixed ? params.aggr_bank + 1 : get_bank_cnt(ctx);
    
    dbg_printf("\n\nhammer repeat start:\n\n")
    hammer_patt_t hpt = { .hammer_count = params.hammer_count };
    flip_results_map_t flip_res;
    dbg_printf("[+] hammer count: %ld\n", hpt.hammer_count);

    int hammered_addrs = 0, flip_addrs = 0, mapped_addrs = 0;
    for (flip_info_t it : flip_info_array) {
        hpt.aggr_num = it.aggr_num;
        hpt.aggr_dist = it.aggr_dist;
        row_t r = get_row(ctx, it.aggr_phys);
        bank_t bk = get_bank(ctx, it.aggr_phys);
        select_hammer_rows(&hpt, it.aggr_num, get_row(ctx, it.aggr_phys), it.aggr_dist);
        
        if (!is_mapped_row(ctx, bk, r, aggr_num, aggr_dist)) {
            continue;
        }

        mapped_addrs += it.flip_location.size();
    }
    if (mapped_addrs < 0.8 * flip_info_array_size(flip_info_array)) {
        dbg_printf("[!] Mapping %d addresses is too little compared to collected %d addresses\n", mapped_addrs, flip_info_array_size(flip_info_array));
        return;
    }

    int res_hammer[NUM_REPEAT] = {0}, res_flip[NUM_REPEAT] = {0};
    for (int i = 0; i < NUM_REPEAT; i++) {
        for (flip_info_t it : flip_info_array) {
            hpt.aggr_num = it.aggr_num;
            hpt.aggr_dist = it.aggr_dist;
            row_t r = get_row(ctx, it.aggr_phys);
            bank_t bk = get_bank(ctx, it.aggr_phys);
            select_hammer_rows(&hpt, it.aggr_num, get_row(ctx, it.aggr_phys), it.aggr_dist);
            
            if (!is_mapped_row(ctx, bk, r, aggr_num, aggr_dist)) {
                continue;
            }
            dbg_printf("\nhammering rows: ");
            for (int i = 0; i < it.aggr_num - 1; ++i) {
                dbg_printf("r%ld/", r + it.aggr_dist * i);
            }
            dbg_printf("r%ld: ", r + it.aggr_dist * (it.aggr_num - 1));

            for (int i = 0; i < hpt.aggr_num; i++) {
                hpt.a_lst[i].bank = bk;
                hpt.v_lst[i * 2].bank = bk;
                hpt.v_lst[i * 2 + 1].bank = bk;       
            }
            for (int patt_index = params.data_pattern; patt_index < patt_num; ++patt_index) {
                uint8_t vict_patt = vict_patt_array[patt_index];
                uint8_t aggr_patt = aggr_patt_array[patt_index]; 
                uint8_t bit_cycle = bit_cycle_array[patt_index];

                if (patt_index < 10) {
                    for (int i = 0; i < aggr_num; ++i) {
                        hpt.data_patt.aggr_patt[i] = aggr_patt;
                        hpt.data_patt.vict_patt[i * 2] = vict_patt;
                        hpt.data_patt.vict_patt[i * 2 + 1] = vict_patt;
                        hpt.data_patt.bit_cycle[i] = bit_cycle;
                    }
                } else { // killer
                    for (int i = 0; i < aggr_num; ++i) {
                        hpt.data_patt.vict_patt[i * 2] = vict_patt;                                    // 0xb6, 0b10110110     0b49, 01001001
                        hpt.data_patt.aggr_patt[i] = aggr_patt;                                        // 0xdb, 0b11011011     0b24, 00100100
                        vict_patt = (vict_patt << 1 & 0xff) + (vict_patt >> 2 & 0b1);
                        hpt.data_patt.vict_patt[i * 2 + 1] = vict_patt;                                // 0x6d, 0b01101101     0b92, 10010010
                        aggr_patt = (aggr_patt << 1 & 0xff) + (aggr_patt >> 2 & 0b1);
                        hpt.data_patt.bit_cycle[i] = bit_cycle;
                    }
                }
                for (int hmmd_index = params.hammer_method; hmmd_index < hmmd_num; hmmd_index++) {                
                    hpt.hammer_method = hmmd_index;
                    fill_vict_rows(ctx, &hpt); 
                    fill_aggr_rows(ctx, &hpt);

                    if (!hammer_rows(ctx, &hpt)) {
                        continue;
                    }
                
                    if (patt_index == params.data_pattern) {
                        hammered_addrs += it.flip_location.size();
                    }

                    clear_flips(flip_res);
                    if (check_flips_row(ctx, &hpt, patt_index, hpt.hammer_method, flip_res)) {                     
                        dbg_printf("[x] \n");  // has flip
                        report_flips(ctx, flip_res);
                        for (auto res : flip_res) {
                            for (auto rec : it.flip_location) {
                                if (res.first == rec.vict_phys) {
                                    int offset = res.second[0].v_flip ^ res.second[0].v_org;
                                    if (offset == rec.offset) {
                                        ++flip_addrs;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }

        }
        res_hammer[i] = hammered_addrs;
        res_flip[i] = flip_addrs;
        hammered_addrs = 0;
        flip_addrs = 0;
    }

    dbg_printf("\n");
    for (int i = 0; i < NUM_REPEAT; i++) {
        dbg_printf("[+] Repeat index %d, hammer %d, flip %d\n", i, res_hammer[i], res_flip[i]);
    }
}

void hammer_record(hammer_ctx_t& ctx, ProfileParams params) { 
    uint8_t* vict_patt_array = ctx.data_patt.vict_patt;
    uint8_t* aggr_patt_array = ctx.data_patt.aggr_patt;
    uint8_t* bit_cycle_array = ctx.data_patt.bit_cycle;

    int patt_num = params.is_data_pattern_fixed ? params.data_pattern + 1 : NUM_DATA_PATTERN;  
    int aggr_num = params.is_aggr_num_fixed? params.aggr_num : MAX_AGGRESSOR_ROW;
    int hmmd_num = params.is_hammer_method_fixed ? params.hammer_method + 1 : NUM_HAMMER_METHOD;
    int aggr_dist = params.aggr_dist;
    bank_t bank_num = params.is_aggr_bank_fixed ? params.aggr_bank + 1 : get_bank_cnt(ctx);

    dbg_printf("[+] bank count: %d\n", get_bank_cnt(ctx));

    hammer_patt_t hpt = {.hammer_count = params.hammer_count};
    flip_results_map_t flip_res;
    dbg_printf("[+] hammer count: %ld\n", hpt.hammer_count);  

    row_t rmin = -1, rmax = 0;
    for (bank_t bk = 0; bk < get_bank_cnt(ctx); ++bk) {
        if (rmin > ctx.occupy_bk_rows[bk].first) {
            rmin = ctx.occupy_bk_rows[bk].first;
        }
        if (rmax < ctx.occupy_bk_rows[bk].second) {
            rmax = ctx.occupy_bk_rows[bk].second;
        }
    }
    dbg_printf("[+] rmin: %ld, rmax: %ld\n", rmin, rmax);
    if (params.is_aggr_row_fixed) {
        rmin = params.first_aggr_row;
        rmax = rmin + 1;
    }
    rmin = rmin + rand() % (rmax - rmin + 1);

    for (row_t r = rmin; r < rmax; r += aggr_num * 2 + 1) {
        select_hammer_rows(&hpt, aggr_num, r, aggr_dist, params.is_aggr_dist_fixed, params.is_aggr_num_fixed);
        dbg_printf("\nhammering rows: ");         
        for (int i = 0; i < hpt.aggr_num - 1; ++i) {             
            dbg_printf("r%ld/", r + hpt.aggr_dist * i);         
        }
        dbg_printf("r%ld: ", r + hpt.aggr_dist * (hpt.aggr_num - 1));         

        clear_flips(flip_res);
        for (bank_t bk = params.aggr_bank; bk < bank_num - 1 ; ++bk){ 
            if (!is_mapped_row(ctx, bk, r, aggr_num, aggr_dist)) {
                continue;
            }
            for (int i = 0; i < aggr_num; ++i) {
                hpt.a_lst[i].bank = bk;
                hpt.v_lst[i * 2].bank = bk;
                hpt.v_lst[i * 2 + 1].bank = bk;       
            }
            dbg_printf("\n\tbank %d: ", bk);
            for (int patt_index = params.data_pattern; patt_index < patt_num; ++patt_index) {
                uint8_t vict_patt = vict_patt_array[patt_index];
                uint8_t aggr_patt = aggr_patt_array[patt_index]; 
                uint8_t bit_cycle = bit_cycle_array[patt_index];
                if (patt_index < 10) {
                    for (int i = 0; i < aggr_num; ++i) {
                        hpt.data_patt.aggr_patt[i] = aggr_patt;
                        hpt.data_patt.vict_patt[i * 2] = vict_patt;
                        hpt.data_patt.vict_patt[i * 2 + 1] = vict_patt;
                        hpt.data_patt.bit_cycle[i] = bit_cycle;
                    }
                } else {  // killer
                    for (int i = 0; i < aggr_num; ++i) {
                        hpt.data_patt.vict_patt[i * 2] = vict_patt;  // 0xb6, 0b10110110     0b49, 01001001
                        hpt.data_patt.aggr_patt[i] = aggr_patt;  // 0xdb, 0b11011011     0b24, 00100100
                        vict_patt = (vict_patt << 1 & 0xff) + (vict_patt >> 2 & 0b1);
                        hpt.data_patt.vict_patt[i * 2 + 1] = vict_patt;  // 0x6d, 0b01101101     0b92, 10010010
                        aggr_patt = (aggr_patt << 1 & 0xff) + (aggr_patt >> 2 & 0b1);
                        hpt.data_patt.bit_cycle[i] = bit_cycle;
                    }
                }

                for (int hmmd_index = params.hammer_method; hmmd_index < hmmd_num; ++hmmd_index) {                
                    hpt.hammer_method = hmmd_index;
                    fill_vict_rows(ctx, &hpt); 
                    fill_aggr_rows(ctx, &hpt);

                    if (!hammer_rows(ctx, &hpt)) {
                        continue;
                    }
                    clear_flips(flip_res);
                    if (check_flips_row(ctx, &hpt, patt_index, hpt.hammer_method, flip_res)) {                     
                        dbg_printf("[x] \n");  // has flip
                        report_flips(ctx, flip_res);

                        if(insert_flip_info_array(ctx, hpt, flip_res) > 1000) {
                            helper_write_flip_info(flip_info_array);
                            return;
                        }
                    }
                }
            }
        }
    }
}

void init_memory_alloc_info(hammer_ctx_t& ctx, virtaddr_t mapping, uint64_t mapping_sz) {
    virtaddr_t virt_page = 0;
    physaddr_t phys_page = 0;
    bank_t bank = 0;
    row_t row = 0;
    
    for (bank_t i = 0; i < get_bank_cnt(ctx); ++i) {
        ctx.occupy_bk_rows[i].first = -1;
        ctx.occupy_bk_rows[i].second = 0;
    }

    std::map<bank_t, row_t> tmp;
    for (uint64_t offset = 0; offset < mapping_sz; offset += PAGE_SIZE) { 
        virt_page = (mapping + offset) & ~PAGE_MASK;
        phys_page = virt_2_phys(virt_page);
        ctx.phys_virt_map[phys_page] = virt_page;

        bank = get_bank(ctx, phys_page);
        row = get_row(ctx, phys_page);
        if (row < ctx.occupy_bk_rows[bank].first) {
            ctx.occupy_bk_rows[bank].first = row;
        } 
        else if (row > ctx.occupy_bk_rows[bank].second) {
            ctx.occupy_bk_rows[bank].second = row;
        }
    }
}

void init_hammer_ctx(hammer_ctx_t& ctx, virtaddr_t mapping, uint64_t mapping_size, 
                    std::vector<std::vector<int>>& vec_bank_funcs,
                     std::vector<int>& vec_row_bits,
                     std::vector<int>& vec_coln_bits) {
    dram_mapping_func_t addr_funcs = {0};
    for (int i = 0; i < vec_row_bits.size(); ++i) {
        addr_funcs.row_mask += (1ul << vec_row_bits[i]);
    }
    addr_funcs.coln_mask = ROW_SIZE - 1;
    addr_funcs.bk_func_cnt = vec_bank_funcs.size();
    for (int i = 0; i < addr_funcs.bk_func_cnt; ++i) {
        addr_funcs.bk_funcs[i] = 0;
        std::vector<int>& bk_bts = vec_bank_funcs[i];
        for (int j = 0; j < bk_bts.size(); ++j) {
            addr_funcs.bk_funcs[i] += (1ul << bk_bts[j]);
        }
    }
    ctx.addr_funcs = addr_funcs;

    init_memory_alloc_info(ctx, mapping, mapping_size);

    physaddr_t phys_mapping = virt_2_phys(mapping);
    ctx.mapping_dram_addr.bank = get_bank(ctx, phys_mapping);
    ctx.mapping_dram_addr.row = get_row(ctx, phys_mapping);
    ctx.mapping_dram_addr.coln = get_coln(ctx, phys_mapping);

    uint8_t aggr_patt_array[NUM_DATA_PATTERN] = {0x00, 0xff, 0xff, 0x00, 0x55, 0xaa, 0x33, 0xcc, 0xaa, 0x55, 0xdb, 0x24};
    uint8_t vict_patt_array[NUM_DATA_PATTERN] = {0x00, 0xff, 0x00, 0xff, 0x55, 0xaa, 0x33, 0xcc, 0x55, 0xaa, 0xb6, 0x49};
    uint8_t bit_cycle[NUM_DATA_PATTERN] = {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3};
    memcpy(ctx.data_patt.vict_patt, vict_patt_array, sizeof(vict_patt_array));
    memcpy(ctx.data_patt.aggr_patt, aggr_patt_array, sizeof(aggr_patt_array));
    memcpy(ctx.data_patt.bit_cycle, bit_cycle, sizeof(bit_cycle));

    dbg_printf("[+] addr_funcs (%d): ", ctx.addr_funcs.bk_func_cnt);
    for (int i = 0; i < ctx.addr_funcs.bk_func_cnt; ++i) {
        dbg_printf("0x%lx, ", ctx.addr_funcs.bk_funcs[i]);
    }
    dbg_printf("\n");
    dbg_printf("[+] row mask: 0x%lx. coln mask: 0x%lx\n", addr_funcs.row_mask, addr_funcs.coln_mask);
    dbg_printf("[+] phys_virt map size: %ld\n", ctx.phys_virt_map.size());
    dbg_printf("[+] alloc memory at 0x%lx => 0x%lx. dram row: %ld, bank: %d\n", 
        (virtaddr_t)mapping, phys_mapping, ctx.mapping_dram_addr.row, ctx.mapping_dram_addr.bank);
}

void go_hammer(hammer_ctx_t& ctx, ProfileParams params) {
#ifdef RECORD
    hammer_record(ctx, params);
#endif
    std::vector<flip_info_t> flip_info_array_tmp;
    helper_read_flip_info(flip_info_array_tmp);
    dbg_printf("\nsize: %lu\n", flip_info_array_tmp.size());
#ifndef RECORD
    hammer_repeat(ctx, params, flip_info_array_tmp);  
#endif
}

uint64_t get_phys_mem_size() {
    struct sysinfo info;
    sysinfo(&info);
    return (size_t)info.totalram * (size_t)info.mem_unit;
}

void setup_mapping(uint64_t* mapping_size, void** mapping, double fraction) {
    *mapping_size = static_cast<uint64_t>(
        (static_cast<double>(get_phys_mem_size()) * fraction));
    *mapping = mmap(NULL, *mapping_size, PROT_READ | PROT_WRITE, MAP_POPULATE | MAP_ANONYMOUS | MAP_PRIVATE | MAP_LOCKED, -1, 0);
    assert(*mapping != (void*)-1);
}

int setup_mapping_hugetlb(uint64_t* mapping_size, void** mapping) {
    uint64_t alloc_size = 1 * G;
    uint64_t alloc_flags = MAP_PRIVATE | MAP_POPULATE | MAP_ANONYMOUS |
                           MAP_HUGETLB |
                           MAP_HUGEPAGE_1G;  //(30 << MAP_HUGE_SHIFT);
    int fd = -1;
    if ((fd = open("/mnt/huge/buff", O_CREAT | O_RDWR, 0755)) == -1) {
        perror("[ERROR] - Unable to open hugetlbfs");
        return -1;
    }
    *mapping = (char*)mmap(NULL, alloc_size, PROT_READ | PROT_WRITE,
                           alloc_flags, fd, 0);
    if (*mapping == MAP_FAILED) {
        perror("[ERROR] - mmap() failed");
        close(fd);
        exit(1);
    }
    *mapping_size = alloc_size;
    close(fd);

    return 0;
}

void HammerAllReachableRows(std::vector<std::vector<int>>& vec_bank_funcs,
                            std::vector<int>& vec_row_bits,
                            std::vector<int>& vec_coln_bits,
                            ProfileParams params) {
    uint64_t mapping_size;
    void* mapping;
#ifndef USE_HUGEPAGE
    dbg_printf("[+] mmap usual mapping...\n");
    setup_mapping(&mapping_size, &mapping, 0.8);
#else
    dbg_printf("[+] mmap huge page mapping...\n");
    setup_mapping_hugetlb(&mapping_size, &mapping);
#endif

    hammer_ctx_t ctx;
    init_hammer_ctx(ctx, (virtaddr_t)mapping, mapping_size, vec_bank_funcs, vec_row_bits, vec_coln_bits);
    
    go_hammer(ctx, params);
}

void HammeredEnough(int sig) {
    dbg_printf("[!] Spent %ld seconds hammering, exiting now.\n",
           g_seconds_to_hammer);
    exit(0);
}

void init_hammer(std::vector<std::vector<int>>& vec_bank_funcs, std::vector<int>& vec_row_bits, std::vector<int>& vec_coln_bits, ProfileParams params) {
    setvbuf(stdout, NULL, _IONBF, 0);
    signal(SIGALRM, HammeredEnough);
    dbg_printf("[!] Starting the hammering process...\n");
    alarm(params.seconds_to_hammer);
    g_seconds_to_hammer = params.seconds_to_hammer;

    MEASURE_TIME_COST_START(g_hammer_start);
    HammerAllReachableRows(vec_bank_funcs, vec_row_bits, vec_coln_bits, params);
    MEASURE_TIME_COST_END("Hammer all reachable rows", g_hammer_start);
}

int main(int argc, char** argv) {
    srand(time(NULL));

    ProfileParams params;
    if (-1 == parse_params(argc, argv, &params)) {
        exit(1);
    }

    std::vector<std::vector<int>> addr_funcs;
    std::vector<int> vec_row_bits;
    std::vector<int> vec_coln_bits;
    read_config(params.ini_filename, vec_row_bits, vec_coln_bits, addr_funcs);
    init_hammer(addr_funcs, vec_row_bits, vec_coln_bits, params);

    return 0;
}