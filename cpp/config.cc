#include "include/config.h"

bool str_2_vec(const char *line, ssize_t llen, std::vector<int> &row_data) {
    if(NULL == line) { return false; }
    char line_copy[llen], *tmp;
    strcpy(line_copy, line);
    uint64_t data = 0;

    while(strspn(line_copy, " \t\r\n") != strlen(line_copy)){
        data = strtol(line_copy, &tmp, 10);
        row_data.push_back(data);
        strcpy(line_copy, tmp);
    }
    return true;
}

int read_config(const char* fname, std::vector<int> &vec_row_bits, 
    std::vector<int> &vec_coln_bits, std::vector<std::vector<int>> &addr_funcs) {
    if (!fname) return -1;

    FILE* f = fopen(fname, "r");
    if (!f) return -1;
    char* line = NULL;
    size_t lblen = 0;
    int ret = 0;
    ssize_t llen = 0;
    while ((llen = getline(&line, &lblen, f)) != -1) {
        if (strstr(line, "[row bits]")) {
            ret = 1;
        } else if (strstr(line, "[column bits]")) {
            ret = 2;
        } else if (strstr(line, "[bank functions]")) {
            ret = 3;
        } else {
            if(1 == ret) {
                str_2_vec(line, llen, vec_row_bits); 
            } else if(2 == ret) {
                str_2_vec(line, llen, vec_coln_bits);
            } else if(3 == ret) {
                std::vector<int> addr;
                str_2_vec(line, llen, addr);
                addr_funcs.push_back(addr);
            }
        }
    }

    if (line) {
        free(line);
    }

    fclose(f);

    return ret;
}
