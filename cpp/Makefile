EXTFLAG=-DDEBUG
LDFLAG=-lpthread
IFLAG=-mclflushopt 

all: hammer
hammer: *.cc
	g++ -std=c++11 -g -O0 $(EXTFLAG) $^ -o $@ $(LDFLAG) $(IFLAG)
hammer-multithread: *.cc
	g++ -std=c++11 -g -O0 $(EXTFLAG) $^ -o $@ $(LDFLAG) $(IFLAG) -DUSE_MULT_THREAD
hammer-record: *cc
	g++ -std=c++11 -g -O0 $(EXTFLAG) $^ -o $@ $(LDFLAG) $(IFLAG) -DRECORD
clean:
	rm -f hammer hammer-multithread hammer-record