#!/bin/bash

# Settings
CC=icc
PHI_BLOOMFWD_DIR=/home/alexandrelucchesi/bloomfwd/ # Mounted host directory.
PREFIXES_DISTRIBUTION_FILE=data/as6447_distrib.txt
PREFIXES_FILE=data/as6447_prefixes.txt
IPV4_ADDRESSES_FILE=data/randomAddrs.txt
#ALGS_SERIAL=("bloomfwd_mic" "bloomfwd_mic_autovec" "bloomfwd_mic_intrin")
#ALGS_SERIAL=("bloomfwd_mic")
#ALGS_PARALLEL=("bloomfwd_mic_par" "bloomfwd_mic_par_autovec" "bloomfwd_mic_par_autovec_twosteps" "bloomfwd_mic_par_intrin_twosteps")
ALGS_PARALLEL=("bloomfwd_mic_par_intrin")
#THREADS=(30 60 90 120 150 180 210 244)
THREADS=(244)
SCHED_CHUNKSIZE="dynamic,1"
OUTPUT_FILE=bench/res/mic/lookup.csv # Benchmark output file.

cd $PHI_BLOOMFWD_DIR
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
			-p $PREFIXES_FILE -r $IPV4_ADDRESSES_FILE -n 67108864)

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

#			for e in $(seq 1 5)  # Number of times to execute.
			for e in $(seq 1 3)  # Number of times to execute.
			do
				# Assure the OpenMP environment variables are set and non-empty.
				: ${OMP_SCHEDULE:?"Need to set OMP_SCHEDULE non-empty."}
				: ${OMP_NUM_THREADS:?"Need to set OMP_NUM_THREADS non-empty."}

				# Execute for input size 2^26 (67,108,864).
				exec_time=$(./bin/$a -d $PREFIXES_DISTRIBUTION_FILE \
				-p $PREFIXES_FILE -r $IPV4_ADDRESSES_FILE -n 67108864)

				printf "."
				printf ", $exec_time" >> $OUTPUT_FILE
			done
		printf "\n"
		printf "\n" >> $OUTPUT_FILE
	done
done

