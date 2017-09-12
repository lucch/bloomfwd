#!/bin/bash

# Settings
CC=icc
PHI_BLOOMFWD_DIR=/home/alexandrelucchesi/bloomfwd-rand/ # Mounted host directory.
PREFIXES_DISTRIBUTION_FILE=/home/alexandrelucchesi/ip-datasets/ipv4/opt_20-24-32/as65000/bgptable/distrib.txt
DLA_FILE=/home/alexandrelucchesi/ip-datasets/ipv4/opt_20-24-32/as65000/bgptable/dla.txt
G1_FILE=/home/alexandrelucchesi/ip-datasets/ipv4/opt_20-24-32/as65000/bgptable/g1.txt
G2_FILE=/home/alexandrelucchesi/ip-datasets/ipv4/opt_20-24-32/as65000/bgptable/g2.txt
IPV4_ADDRESSES_FILE=/home/alexandrelucchesi/ip-datasets/ipv4/addrs/matching-80.txt

#ALGS_SERIAL=("bloomfwd_opt_mic_rand")
ALGS_PARALLEL=("bloomfwd_opt_mic_par_rand")
#THREADS=(30 60 90 120 150 180 210 244)
THREADS=(30)
SCHED_CHUNKSIZE="dynamic,1"
OUTPUT_FILE=bench/res/mic/exectime/lookup.csv # Benchmark output file.

cd $PHI_BLOOMFWD_DIR
mkdir -p bench/res/mic/exectime/

# Clean old data files...
data_files=$(ls bench/res/mic/exectime/)
if [ ${#data_files} -gt 0 ]; then
	rm -f bench/res/mic/exectime/*
fi

export LD_LIBRARY_PATH=bin
export OMP_SCHEDULE="$SCHED_CHUNKSIZE"

# Write headers to output file.
printf "Algorithm, # Threads, Execs...\n" >> $OUTPUT_FILE

# Write data to output file.
# Serial
for a in "${ALGS_SERIAL[@]}"
do
		printf "$a, 1: "
		printf "$a, 1" >> $OUTPUT_FILE

		for e in $(seq 1 3)  # Number of times to execute.
		do
			# Execute for input size 2^26 (67,108,864).
			exec_time=$(./bin/$a -d $PREFIXES_DISTRIBUTION_FILE \
			-dla $DLA_FILE -g1 $G1_FILE -g2 $G2_FILE -r $IPV4_ADDRESSES_FILE)

			printf "."
			printf ", $exec_time" >> $OUTPUT_FILE
		done
		printf "\n"
		printf "\n" >> $OUTPUT_FILE
done

# Parallel
for a in "${ALGS_PARALLEL[@]}"
do
	for t in "${THREADS[@]}"
	do
		export OMP_NUM_THREADS=$t
		printf "$a, $t: "
		printf "$a, $t" >> $OUTPUT_FILE

        for e in $(seq 1 3)  # Number of times to execute.
        do
            # Assure the OpenMP environment variables are set and non-empty.
            : ${OMP_SCHEDULE:?"Need to set OMP_SCHEDULE non-empty."}
            : ${OMP_NUM_THREADS:?"Need to set OMP_NUM_THREADS non-empty."}

            # Execute for input size 2^26 (67,108,864).
            exec_time=$(./bin/$a -d $PREFIXES_DISTRIBUTION_FILE \
            -dla $DLA_FILE -g1 $G1_FILE -g2 $G2_FILE -r $IPV4_ADDRESSES_FILE)

            printf "."
            printf ", $exec_time" >> $OUTPUT_FILE
        done

		printf "\n"
		printf "\n" >> $OUTPUT_FILE
	done
done

