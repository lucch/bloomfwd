#!/bin/bash

# This script executes 'bloomfwd' in its parallel version (using OpenMP) in
# order to compute the optimum chunk size for each scheduler. Thus, for a fixed
# hash function (see the script 'serial.sh') and a fixed number of threads, it
# varies the chunk size for each scheduler. It outputs the corresponding
# execution times to files in the CSV format.

# Settings
PROJECT_DIR=~/Development/c/bloomfwd/
CC=icc
PREFIXES_DISTRIBUTION_FILE=res/sp-prefixes-distribution.txt
PREFIXES_FILE=res/sp-prefixes.txt
IPV4_ADDRESSES_FILE=res/addresses.txt
HASH_FUNCTION=FNV
NUM_THREADS=16

cd $PROJECT_DIR
mkdir -p bench/res/chunksize/

# Clean data files...
data_files=$(ls bench/res/chunksize)
if [ ${#data_files} -gt 0 ]; then
    rm bench/res/chunksize/*
fi

export CC=$CC

# Parallel - Chunk Size
printf "Parallel - Chunk Size\n"
printf "%s\n" "---------------------"

make clean
    #> /dev/null 2>&1
make V=1 PARALLEL=1 HIDE_OUTPUT=1 HASH_FUNCTION=$HASH_FUNCTION
    #> /dev/null 2>&1

export OMP_NUM_THREADS=$NUM_THREADS

schedulers=("static" "dynamic" "guided")

for s in "${schedulers[@]}"  # Scheduler.
do
    file1=bench/res/chunksize/$s\_input.csv
    file2=bench/res/chunksize/$s\_lookup.csv

    # Write headers to files.
    printf "Chunk Size/Input Size" >> $file1
    printf "Chunk Size/Input Size" >> $file2

    for j in $(seq 0 8)
    do
        printf ", 2^$((2 * $j))" >> $file1
        printf ", 2^$((2 * $j))" >> $file2
    done

    printf "\n" >> $file1
    printf "\n" >> $file2
    # End write headers.

    for i in $(seq 0 5)  # Input size.
    do
        k=$((20 + 2 * $i))
        input_size=$((2 ** $k))

        printf "2^$k" >> $file1
        printf "2^$k" >> $file2

        for j in $(seq 0 8)  # Chunk size.
        do
            chunk_size=$((2 ** (2 * $j)))
            export OMP_SCHEDULE="$s,$chunk_size"

            printf "{$s, 2^$k, $chunk_size}: "

            time_input=0.0
            time_lookup=0.0

            for r in $(seq 0 2)
            do
                # Assure the OpenMP environment variables are set and non-empty.
                : ${OMP_SCHEDULE:?"Need to set OMP_SCHEDULE non-empty."}
                : ${OMP_NUM_THREADS:?"Need to set OMP_NUM_THREADS non-empty."}

                output=$(./bin/bloomfwd -d $PREFIXES_DISTRIBUTION_FILE \
                    -p $PREFIXES_FILE -r $IPV4_ADDRESSES_FILE -n $input_size)
                exec_times=($output)  # Convert to array (splitting at ' ').

                time_input=$(echo "$time_input + ${exec_times[0]}" | bc)
                time_lookup=$(echo "$time_lookup + ${exec_times[1]}" | bc)

                printf "."
            done

            avg_time_input=$(echo "$time_input / 3.0" | bc -l)
            avg_time_lookup=$(echo "$time_lookup / 3.0" | bc -l)

            printf ", $avg_time_input" >> $file1
            printf ", $avg_time_lookup" >> $file2

            printf "\n"
        done

        printf "\n" >> $file1
        printf "\n" >> $file2

        printf "\n"
    done

    printf "\n"
done

