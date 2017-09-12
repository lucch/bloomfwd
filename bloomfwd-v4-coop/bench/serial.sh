#!/bin/bash

# This script executes 'bloomfwd' varying the input size and the hash functions
# used within the Bloom filters. It outputs the corresponding execution times to
# files in the CSV format.

# Settings
PROJECT_DIR=~/Development/c/bloomfwd/
CC=icc
PREFIXES_DISTRIBUTION_FILE=res/sp-prefixes-distribution.txt
PREFIXES_FILE=res/sp-prefixes.txt
IPV4_ADDRESSES_FILE=res/addresses.txt

cd $PROJECT_DIR
mkdir -p bench/res/serial/

# Clean data files...
data_files=$(ls bench/res/serial)
if [ ${#data_files} -gt 0 ]; then
    rm bench/res/serial/*
fi

export CC=$CC

# Serial
printf "Serial\n"
printf "%s\n" "------"

hash_functions=("jen" "fnv" "mur")

for f in "${hash_functions[@]}"
do
    make clean > /dev/null # 2>&1
    make V=1 HIDE_OUTPUT=1 HASH_FUNCTION=$(echo $f | awk '{print toupper($0)}') \
        # > /dev/null 2>&1

    file1=bench/res/serial/input_$f.csv
    file2=bench/res/serial/lookup_$f.csv

    printf "\t, Serial\n" >> $file1
    printf "\t, Serial\n" >> $file2

    printf "$f\n"

    for i in $(seq 0 5)  # Input size.
    do
        k=$((20 + 2 * $i))
        printf "2^$k" >> $file1
        printf "2^$k" >> $file2

        input_size=$(echo "2^$k" | bc)
        time_input=0.0
        time_lookup=0.0

        for j in $(seq 0 2)
        do
            output=$(./bin/bloomfwd -d $PREFIXES_DISTRIBUTION_FILE \
                -p $PREFIXES_FILE -r $IPV4_ADDRESSES_FILE -n $input_size)
            exec_times=($output)  # Convert to array (splitting at ' ').

            time_input=$(echo "$time_input + ${exec_times[0]}" | bc)
            time_lookup=$(echo "$time_lookup + ${exec_times[1]}" | bc)

            printf "."
        done

        avg_time_input=$(echo "$time_input / 3.0" | bc -l)
        avg_time_lookup=$(echo "$time_lookup / 3.0" | bc -l)

        printf ", $avg_time_input\n" >> $file1
        printf ", $avg_time_lookup\n" >> $file2

        printf "\n"
    done

    printf "\n"
done

