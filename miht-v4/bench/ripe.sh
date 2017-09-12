#!/bin/bash

# Settings
CC=icc
# Paths should be set for native execution on Phi
MIHT_DIR=/home/alexandrelucchesi/miht/
DATA_DIR=/home/alexandrelucchesi/ip-datasets/ripe
ADDRS_FILE=/home/alexandrelucchesi/ip-datasets/ipv4/addrs/matching-80.txt
ALGS_PARALLEL=("miht_mic_par")
THREADS=(244)
SCHED_CHUNKSIZE="dynamic,1"
OUTPUT_FILE=bench/res/mic/ripe.csv # Benchmark output file.

cd $MIHT_DIR
mkdir -p bench/res/mic/

# Clean old data files...
data_files=$(ls bench/res/mic)
if [ ${#data_files} -gt 0 ]; then
	rm -f bench/res/mic/*
fi

export LD_LIBRARY_PATH=bin
export OMP_SCHEDULE="$SCHED_CHUNKSIZE"

# Write headers to output file.
printf "Algorithm, # Threads, Execs...\n" >> $OUTPUT_FILE

# Parallel
databases=$(ls $DATA_DIR)
for d in $databases
do
    prefixes=$DATA_DIR/$d/basic/prefixes.txt

    for a in "${ALGS_PARALLEL[@]}"
    do
        for t in "${THREADS[@]}"
        do
            export OMP_NUM_THREADS=$t
            printf "$d, $a, $t: "
            printf "$d, $a, $t" >> $OUTPUT_FILE
                for e in $(seq 1 3)  # Number of times to execute.
                do
                    # Assure the OpenMP environment variables are set and non-empty.
                    : ${OMP_SCHEDULE:?"Need to set OMP_SCHEDULE non-empty."}
                    : ${OMP_NUM_THREADS:?"Need to set OMP_NUM_THREADS non-empty."}

                    # Execute for input size 2^26 (67,108,864).
                    exec_time=$(./bin/$a -p $prefixes -r $ADDRS_FILE -n 67108864)

                    printf "."
                    printf ", $exec_time" >> $OUTPUT_FILE
                done
            printf "\n"
            printf "\n" >> $OUTPUT_FILE
        done
    done
done

