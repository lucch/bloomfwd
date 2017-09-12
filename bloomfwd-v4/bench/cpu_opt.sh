#!/bin/bash

# Settings
CC=icc
PHI_BLOOMFWD_DIR=/home/alexandrelucchesi/Development/c/bloomfwd/ # Mounted host directory.
PREFIXES_DISTRIBUTION_FILE=data/opt/as6447_distrib_opt.txt
DLA_FILE=data/opt/dla.txt
G1_FILE=data/opt/g1.txt
G2_FILE=data/opt/g2.txt
IPV4_ADDRESSES_FILE=data/randomAddrs.txt
#ALGS_SERIAL=("bloomfwd")
ALGS_PARALLEL=("bloomfwd_par_opt")
#THREADS=(2 4 8 12 16 20 24 28 32)
THREADS=(16 32)

SCHED_CHUNKSIZE="dynamic,1"
OUTPUT_FILE=bench/res/cpu/lookup.csv # Benchmark output file.

cd $PHI_BLOOMFWD_DIR
mkdir -p bench/res/cpu/

# Clean old data files...
data_files=$(ls bench/res/cpu)
if [ ${#data_files} -gt 0 ]; then
	rm -f bench/res/cpu/*
fi

#export LD_LIBRARY_PATH=bin
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
			# Execute for input size 2^24 (16,777,216).
			exec_time=$(./bin/$a -d $PREFIXES_DISTRIBUTION_FILE \
			-dla $DLA_FILE -g1 $G1_FILE -g2 $G2_FILE -r $IPV4_ADDRESSES_FILE -n 67108864)

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

				# Execute for input size 2^24 (16,777,216).
				exec_time=$(./bin/$a -d $PREFIXES_DISTRIBUTION_FILE \
				-dla $DLA_FILE -g1 $G1_FILE -g2 $G2_FILE -r $IPV4_ADDRESSES_FILE -n 67108864)

				printf "."
				printf ", $exec_time" >> $OUTPUT_FILE
			done
		printf "\n"
		printf "\n" >> $OUTPUT_FILE
	done
done

