CC=gcc
CFLAGS=-Wall -Wextra -g -lpthread -lprofiler

default: 
	$(CC) $(CFLAGS) -o lab2_list lab2_list.c SortedList.c

tests: default
	rm -f lab2b_list.csv
	chmod +x gen_data.sh
	-./gen_data.sh 2> /dev/null

profile: default
	rm -f raw.gperf profile.out
	LD_PRELOAD=/usr/lib64/libprofiler.so
	CPUPROFILE=./raw.gperf ./lab2_list --threads=12 --iterations=1000 --sync=s
	/usr/bin/pprof --text ./lab2_list ./raw.gperf > profile.out
	/usr/bin/pprof --list=thread_worker ./lab2_list ./raw.gperf >> profile.out

graphs: tests
	-./lab2b_list.gp 2> /dev/null

dist: graphs profile
	tar -czvf lab2b-505111868.tar.gz SortedList.h SortedList.c lab2_list.c Makefile README lab2b_list.csv lab2b_1.png lab2b_2.png lab2b_3.png lab2b_4.png lab2b_5.png gen_data.sh lab2b_list.gp profile.out raw.gperf

clean:
	rm -f *.o lab2_list lab2b-505111868.tar.gz