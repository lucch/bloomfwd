#!/bin/bash

# Settings
CC=icc
PROJECT_DIR=/home/alexandrelucchesi/miht/ # Mounted host directory.
PREFIXES_FILE=/home/alexandrelucchesi/ip-datasets/ipv4/basic/as65000/bgptable/prefixes.txt
IPV4_ADDRESSES_FILE=/home/alexandrelucchesi/ip-datasets/ipv4/addrs/matching-80.txt
#ALGS_SERIAL=("miht_mic")
ALGS_PARALLEL=("miht_mic_par")
#THREADS=(30 60 90 120 150 180 210 240 244)
#THREADS=(30 60 90 120 150 180 210 244)
THREADS=(30)
SCHED_CHUNKSIZE="dynamic,1"
OUTPUT_FILE=bench/res/mic/lookup.csv # Benchmark output file.

cd $PROJECT_DIR
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
			# Execute for input size 2^26 (67108864).
			exec_time=$(./bin/$a -p $PREFIXES_FILE \
				-r $IPV4_ADDRESSES_FILE -n 67108864)

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

				# Execute for input size 2^26 (67108864).
				exec_time=$(./bin/$a -p $PREFIXES_FILE \
					-r $IPV4_ADDRESSES_FILE -n 67108864)

				printf "."
				printf ", $exec_time" >> $OUTPUT_FILE
			done
		printf "\n"
		printf "\n" >> $OUTPUT_FILE
	done
done

