# Program for testing different factors for RowHammer at the system level

"Rowhammer" is a vulnerability found in recent DRAM modules.
Repeatedly accessing a DRAM row can cause bit flips in its adjacent rows. 
This program can be used to test different factors for RowHammer and works on Linux.
It collects vulnerable bits and then repeatly hammer these addresses to count the re-produced bit flips under different arguments.
Make sure that it must run under the root privilege.

## Getting Started

### Testing setup:
- OS: 64-bit Ubuntu 16.04
- CPU: Intel(R) Core(TM) i3-10100 CPU @ 3.60GHz
- DDR4 Mem: Galaxy DDR4 4000MHz (8G, 16 banks, 1 rank).
- Mother Board: MSI MPG Z490 GAMING EDGE WIFI (MS-7C79)

### Prerequisites

Running the RowHammer test needs correct dram address mapping.
Please reverse engineer your dram mapping and create your dram address mapping configuration file at first.
The sample configuration file is located at [./conf/Skylake_16bank_8GB.conf](./conf/Skylake_16bank_8GB.conf).

#### Options

The configurable options are listed below:
```
-h       help message
-i x     predefined dram mapping configuration file path (default ../conf/Skylake_16bank_8GB.conf)
-s x     seconds to hammer (default 3600 * 24 * 10)
-t x     hammer count (default 1000000)
-r x     fixed row x to be hammer (x is the index of the first aggressor row)
-g x     fixed aggressor row distance x (default 2)
-n x     fixed aggressor row num x (default 3)
--random_dist           randomize aggressor aggr_dist
--random_aggr_num       randomize aggressor number
--hm x   hammer method index x (default all)
         0: clflush + read
         1: clflush + write
         2: clflushopt + read (only available on skylake+)
         3: clflushopt + write (only available on skylake+)
         4: movnti + read
         5: movnti + write
         6: movntdq + read
         7: movntdq + write
--dp x   data pattern index x (default all)
         0: f/t/s (0x00/0x00/0x00) - solid stripe
         1: f/t/s (0xff/0xff/0xff) - solid stripe
         2: f/t/s (0xff/0x00/0xff) - row stripe
         3: f/t/s (0x00/0xff/0x00) - row stripe
         4: f/t/s (0x55/0x55/0x55) - column stripe
         5: f/t/s (0xaa/0xaa/0xaa) - column stripe
         6: f/t/s (0x33/0x33/0x33) - double column stripe
         7: f/t/s (0xcc/0xcc/0xcc) - double column stripe
         8: f/t/s (0xaa/0x55/0xaa) - checkered board
         9: f/t/s (0x55/0xaa/0x55) - checkered board
        10: f/t/s (0x6d/0xb6/0xdb) - killer
        11: f/t/s (0x92/0x49/0x24) - killer
```

#### Usage

The program will allocate a large amount of memory (e.g., 80\%) to generate many-sided memory layout. So please make sure that there is enough free memory space.
In order to translate virtual address to physical address, the program needs root privilege to access pagemap for address mapping.

A sample usage is presented below:

```
cd cpp/

# Record 
make record
# collect vulnerable bits and print into ./log/flip_info.log
# Note that the tested Galaxy module is vulnerable to 13-sided RowHammer
sudo ./hammer-record --hm 0 --dp 2 -i ../conf/Skylake_16bank_8GB.conf -n 13 -t 1000000 

# Repeat hammer the collected bits
make
sudo ./hammer --hm 0 --dp 2 -i ../conf/Skylake_16bank_8GB.conf -n 13 -t 1000000 

```

#### Record Results
The sample collected flip information is stored in [./log/flip_info.log](./log/flip_info.log). And the output of program is captured below:

```
[!] Starting the hammering process...
[+] mmap usual mapping...
[+] addr_funcs (4): 0x90000, 0x48000, 0x24000, 0x2040, 
[+] row mask: 0x1fffe0000. coln mask: 0x1fff
[+] phys_virt map size: 985407
[+] alloc memory at 0x7f8662cb1000 => 0x1047e0000. dram row: 33343, bank: 7
[+] bank count: 16
[+] hammer count: 1000000
[+] rmin: 128, rmax: 77879

...

hammering rows: r55875/r55877/r55879/r55881/r55883/r55885/r55887/r55889/r55891/r55893/r55895/r55897/r55899: 
	bank 0: 1712 
	bank 1: 1712 
	bank 2: 1720 [x] 

[+] Found Flip. pa: 1b4b0ab57. bank: 2, row: r55896, col: c2903. Flip Patterns Count: 1
	     clflush+read:  2 (ff/00/ff): [00 => 80] (0 -> 1)
...

size: 287
[+] Hammer all reachable rows cost: 172.307782 sec

```

#### Repeat Results
To repeatly hammer collected vulnerable bits, you must firstly collect these addresses into [./log/flip_info.log](./log/flip_info.log). And the output of program is captured below:

```
[!] Starting the hammering process...
[+] mmap usual mapping...
[+] addr_funcs (4): 0x90000, 0x48000, 0x24000, 0x2040, 
[+] row mask: 0x1fffe0000. coln mask: 0x1fff
[+] phys_virt map size: 985407
[+] alloc memory at 0x7f73f03ae000 => 0x137bd6000. dram row: 39902, bank: 14

size: 287


hammer repeat start:

[+] hammer count: 1000000

hammering rows: r45604/r45606/r45608/r45610/r45612/r45614/r45616/r45618/r45620/r45622/r45624/r45626/r45628: 1719 [x] 

[+] Found Flip. pa: 1644b6067. bank: 0, row: r45605, col: c103. Flip Patterns Count: 1
	     clflush+read:  2 (ff/00/ff): [00 => 20] (0 -> 1)

...

[+] Repeat index 0, hammer 921, flip 792
[+] Repeat index 1, hammer 921, flip 795
[+] Repeat index 2, hammer 921, flip 791
[+] Repeat index 3, hammer 921, flip 752
[+] Repeat index 4, hammer 921, flip 778
[+] Hammer all reachable rows cost: 1279.069409 sec

```
