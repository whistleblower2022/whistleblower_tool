#include "include/params.h" 

void showUsage() {
    fprintf(stderr, "\nNOTE: please make sure your dram mapping is correct!\n"); 
    fprintf(stderr, "\nOptions:\n"); 
    fprintf(stderr, "\t-h  \t help message\n");
    fprintf(stderr, "\t-i x\t predefined dram mapping configuration file path (default ../conf/Skylake_16bank_8GB.conf)\n");
    fprintf(stderr, "\t-s x\t seconds of hammer (default 3600 * 24 * 10)\n");
    fprintf(stderr, "\t-t x\t hammer count (default 1000000)\n");
    fprintf(stderr, "\t-r x\t fixed row x to be hammer (x is the index of the first aggressor row)\n");
    fprintf(stderr, "\t-g x\t fixed aggressor row aggr_dist x (default 2)\n");
    fprintf(stderr, "\t-n x\t fixed aggressor row num x (default 3)\n");
    fprintf(stderr, "\t--random_dist   \trandom aggressor aggr_dist\n");
    fprintf(stderr, "\t--random_aggr_num    \trandom aggressor number\n");
    fprintf(stderr, "\t--hm x\t hammer method number x (default all)\n"); 
    fprintf(stderr, "\t\t 0: clflush + read\n");
    fprintf(stderr, "\t\t 1: clflush + write\n");
    fprintf(stderr, "\t\t 2: clflushopt + read (only available on skylake+)\n");
    fprintf(stderr, "\t\t 3: clflushopt + write (only available on skylake+)\n");
    fprintf(stderr, "\t\t 4: movnti + read\n");
    fprintf(stderr, "\t\t 5: movnti + write\n");
    fprintf(stderr, "\t\t 6: movntdq + read\n");
    fprintf(stderr, "\t\t 7: movntdq + write\n");
    fprintf(stderr, "\t--dp x\t data pattern number x (default all)\n");   
    fprintf(stderr, "\t\t 0: f/t/s (0x00/0x00/0x00) - solid stripe\n");
    fprintf(stderr, "\t\t 1: f/t/s (0xff/0xff/0xff) - solid stripe\n");
    fprintf(stderr, "\t\t 2: f/t/s (0xff/0x00/0xff) - row stripe\n");
    fprintf(stderr, "\t\t 3: f/t/s (0x00/0xff/0x00) - row stripe\n"); 
    fprintf(stderr, "\t\t 4: f/t/s (0x55/0x55/0x55) - column stripe\n"); 
    fprintf(stderr, "\t\t 5: f/t/s (0xaa/0xaa/0xaa) - column stripe\n"); 
    fprintf(stderr, "\t\t 6: f/t/s (0x33/0x33/0x33) - double column stripe\n");
    fprintf(stderr, "\t\t 7: f/t/s (0xcc/0xcc/0xcc) - double column stripe\n");  
    fprintf(stderr, "\t\t 8: f/t/s (0xaa/0x55/0xaa) - checkered board\n");
    fprintf(stderr, "\t\t 9: f/t/s (0x55/0xaa/0x55) - checkered board\n");   
    fprintf(stderr, "\t\t10: f/t/s (0x6d/0xb6/0xdb) - killer\n");   
    fprintf(stderr, "\t\t11: f/t/s (0x92/0x49/0x24) - killer\n");     
}

void showError() {
    fprintf(stderr, "[-] Error parameters! \n\n"); 
    showUsage();
}

int parse_params(int argc, char **argv, ProfileParams *params) {
    int ch, longindex;

    // default
    strcpy(params->ini_filename, "../conf/Skylake_16bank_8GB.conf");
    params->is_data_pattern_fixed = false;
	params->data_pattern = 0;
    params->is_hammer_method_fixed = false;
    params->hammer_method = 0;
    params->is_aggr_row_fixed = false;
    params->first_aggr_row = 0;
    params->is_aggr_bank_fixed = false;
    params->aggr_bank = 0;
    params->is_aggr_dist_fixed = true;
    params->aggr_dist = 2;
    params->is_aggr_num_fixed = true;
    params->aggr_num = 3;
    params->seconds_to_hammer = 3600*24*10;
    params->hammer_count = 1000000;

    const struct option options[] = {
        {.name = "dp", required_argument, .flag = NULL, .val = 0},
        {.name = "hm", required_argument, .flag = NULL, .val = 0},
        {.name = "random_dist", no_argument, .flag = NULL, .val = 0},        
        {.name = "random_aggr_num", no_argument, .flag = NULL, .val = 0}, 
    };

    while ((ch = getopt_long(argc, argv, "hi:s:t:r:b:g:n:", options, &longindex)) != -1) {
        switch (ch) {
            case 'h':
                showUsage();
                return -1;
            case 'i':
                if (NULL == optarg) {
                    showError();
                    return -1;
                }
                strcpy(params->ini_filename, optarg);
                break;
            case 's':
                if (NULL == optarg) {
                    showError();
                    return -1;
                }
                params->seconds_to_hammer = atoi(optarg);
                break;
            case 't':
                if (NULL == optarg) {
                    showError();
                    return -1;
                }
                params->hammer_count = atoi(optarg);
                break;
            case 'r':
                if(NULL == optarg) {
                    showError();
                    return -1;
                }
                params->is_aggr_row_fixed = true;
                params->first_aggr_row = atoi(optarg); 
                break;
            case 'b':
                if (NULL == optarg) {
                    showError();
                    return -1;
                }
                params->is_aggr_bank_fixed = true;
                params->aggr_bank = atoi(optarg);
                break;
            case 'g':
                if (NULL == optarg) {
                    showError();
                    return -1;
                }
                params->is_aggr_dist_fixed = true;
                params->aggr_dist = atoi(optarg);
                if(params->aggr_dist > MAX_AGGRESSOR_DIST) {
                    showError();
                    return -1;
                }
                break;
            case 'n':
                if (NULL == optarg) {
                    showError();
                    return -1;
                }
                params->is_aggr_num_fixed = true;
                params->aggr_num = atoi(optarg);
                if(params->aggr_num > MAX_AGGRESSOR_ROW) {
                    showError();
                    return -1;
                }
                break;
            case 0:
                switch (longindex) {
                    case 0:
                        if(NULL == optarg) {
                            showError();
                            return -1;
                        }
                        params->data_pattern = atoi(optarg);
                        params->is_data_pattern_fixed = true;
                        break;
                    case 1:
                        if(NULL == optarg) {
                            showError();
                            return -1;
                        }
                        params->hammer_method = atoi(optarg);
                        params->is_hammer_method_fixed = true;
                        break;
                    case 2:
                        params->is_aggr_dist_fixed = false;
                        break;
                    case 3:
                        params->is_aggr_num_fixed = false;
                        break;
                    default:
                        showError();
                        return -1;
                }
                break;
            default:
                showError();
                return -1;
        }
    }
    return 0;
}
