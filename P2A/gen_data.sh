#! /bin/bash

# lab2_add
# without yield or locks
for i in 1 2 4 8 12
do
    for j in 10 100 1000 10000 100000
    do
        ./lab2_add --threads=${i} --iterations=${j} >> lab2_add.csv
    done
done

# with yield without locks
for i in 1 2 4 8 12
do
    for j in 10 20 40 80 100 1000 10000 100000
    do 
        ./lab2_add --threads=${i} --iterations=${j} --yield >> lab2_add.csv
    done
done

# with yield with locks
for i in 2 4 8 12
do
    ./lab2_add --threads=${i} --iterations=10000 --yield --sync=m >> lab2_add.csv
    ./lab2_add --threads=${i} --iterations=10000 --yield --sync=c >> lab2_add.csv
    ./lab2_add --threads=${i} --iterations=1000 --yield --sync=s >> lab2_add.csv
    ./lab2_add --threads=${i} --iterations=1000 --yield >> lab2_add.csv
done

# without yield with locks
for i in 1 2 4 8 12
do 
    ./lab2_add --threads=${i} --iterations=10000 --sync=m >> lab2_add.csv
    ./lab2_add --threads=${i} --iterations=10000 --sync=c >> lab2_add.csv
    ./lab2_add --threads=${i} --iterations=10000 --sync=s >> lab2_add.csv
    ./lab2_add --threads=${i} --iterations=10000 >> lab2_add.csv
done

# lab2_list
# single thread, increasing iterations, no yield, no sync
for i in 10 100 1000 10000 20000
do 
    ./lab2_list --iterations=${i} >> lab2_list.csv
done

# varying threads with iterations, no yield, no sync
for i in 2 4 8 12
do 
    for j in 1 10 100 1000
    do
        ./lab2_list --threads=${i} --iterations=${j} >> lab2_list.csv
    done
done

# varying threads, iterations, yields, without sync
for i in 2 4 8 12
do 
    for j in 1 2 4 8 16 32
    do
        for k in i d l il dl id idl
        do
            ./lab2_list --threads=${i} --iterations=${j} --yield=${k} >> lab2_list.csv
        done
    done
done

# varying threads, iterations, yields, with sync
for i in 2 4 8 12
do 
    for j in 1 2 4 8 16 32
    do
        for k in i d l il dl id idl
        do
            for l in s m
            do
                ./lab2_list --threads=${i} --iterations=${j} --yield=${k} --sync=${l} >> lab2_list.csv
            done
        done
    done
done

# varying threads, constant iterations, no yield, with sync
for i in range 1 2 4 8 12 16 24
do 
    for j in s m 
    do
        ./lab2_list --threads=${i} --iterations=1000 --sync=${j} >> lab2_list.csv
    done
done