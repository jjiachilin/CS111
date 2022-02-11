#! /bin/bash

# lab2b list ops, mutex
for i in 1 2 4 8 12 16 24
do
    for j in s m
    do
        ./lab2_list --threads=${i} --iterations=1000 --sync=${j} --lists=1 >> lab2b_list.csv
    done
done

# sublist implementation tests without sync
for i in 1 4 8 12 16 
do
    for j in 1 2 4 8 16
    do 
        ./lab2_list --threads=${i} --iterations=${j} --lists=4 --yield=id >> lab2b_list.csv
    done
done

# with sync
for i in 1 4 8 12 16 
do
    for j in 10 20 40 80
    do
        for k in s m
        do
            ./lab2_list --threads=${i} --iterations=${j} --lists=4 --yield=id --sync=${k} >> lab2b_list.csv
        done
    done
done

for i in 1 2 4 8 12
do
    for j in 4 8 16
    do
        for k in s m
        do
            ./lab2_list --threads=${i} --iterations=1000 --lists=${j} --sync=${k} >> lab2b_list.csv
        done
    done
done