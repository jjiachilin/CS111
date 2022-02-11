#! /usr/bin/gnuplot

# general plot parameters
set terminal png
set datafile separator ","

# plot total number of operations per second for each sync method
set title "List-1: Aggregate Throughput for Different Synchronization Methods"
set xlabel "Threads"
set logscale x 2
set xrange [0.75:]
set ylabel "Throughput (ops/s)"
set output 'lab2b_1.png'
set key left top
plot \
     "< egrep 'list-none-m,(1|2|4|8|12|16|24),1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'mutex lock' with linespoints lc rgb 'red', \
     "< egrep 'list-none-s,(1|2|4|8|12|16|24),1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'spin lock' with linespoints lc rgb 'green'

# plot mean time per mutex wait and mean time per operation for mutex synced list operations
set title "List-2: Mean Mutex Wait-for-lock Time and Mean Time per Operation"
set xlabel "Threads"
set logscale x 2
set xrange [0.75:]
set ylabel "Mean time/Operation (ns)"
set output 'lab2b_2.png'
set key left top
plot \
     "< egrep 'list-none-m,(1|2|4|8|12|16|24),1000,1,' lab2b_list.csv" using ($2):($7) \
	title 'time per operation' with linespoints lc rgb 'red', \
     "< egrep 'list-none-m,(1|2|4|8|12|16|24),1000,1,' lab2b_list.csv" using ($2):($8) \
	title 'wait-for-lock time' with linespoints lc rgb 'green'

# plot successful iterations vs threads for each synchronization method
set title "List-3: Successful Iterations vs Threads for different sync methods"
set xlabel "Threads"
set logscale x 2
set xrange [0.75:]
set ylabel "Successful Iterations"
set output 'lab2b_3.png'
set key left top
plot \
     "< grep 'list-id-none,[0-9]*,[0-9]*,4' lab2b_list.csv" using ($2):($3) \
	title 'unprotected' with points lc rgb 'red', \
     "< grep 'list-id-m,[0-9]*,[0-9]*,4' lab2b_list.csv" using ($2):($3) \
	title 'mutex lock' with points lc rgb 'green', \
     "< grep 'list-id-s,[0-9]*,[0-9]*,4' lab2b_list.csv" using ($2):($3) \
	title 'spin lock' with points lc rgb 'blue'

# throughput vs number of threads for mutex synced partitioned lists
set title "List-4: Throughput vs Threads for mutex synced partitioned lists"
set xlabel "Threads"
set logscale x 2
set xrange [0.75:]
set ylabel "Throughput (ops/s)"
set logscale y 10
set output 'lab2b_4.png'
set key left top
plot \
     "< egrep 'list-none-m,(1|2|4|8|12),1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'lists=1' with linespoints lc rgb 'red', \
     "< egrep 'list-none-m,(1|2|4|8|12),1000,4,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'lists=4' with linespoints lc rgb 'green', \
     "< egrep 'list-none-m,(1|2|4|8|12),1000,8,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'lists=8' with linespoints lc rgb 'blue', \
     "< egrep 'list-none-m,(1|2|4|8|12),1000,16,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'lists=16' with linespoints lc rgb 'yellow'

# throughput vs number of threads for spin lock synced partitioned lists
set title "List-5: Throughput vs threads for spin lock synced partitioned lists"
set xlabel "Threads"
set logscale x 2
set xrange [0.75:]
set ylabel "Throughput (ops/s)"
set logscale y 10
set output 'lab2b_5.png'
set key left top
plot \
     "< egrep 'list-none-s,(1|2|4|8|12),1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'lists=1' with linespoints lc rgb 'red', \
     "< egrep 'list-none-s,(1|2|4|8|12),1000,4,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'lists=4' with linespoints lc rgb 'green', \
     "< egrep 'list-none-s,(1|2|4|8|12),1000,8,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'lists=8' with linespoints lc rgb 'blue', \
     "< egrep 'list-none-s,(1|2|4|8|12),1000,16,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'lists=16' with linespoints lc rgb 'yellow'