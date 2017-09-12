#!/bin/bash

# Settings
CC=icc
BLOOMFWD_DIR=/home/alexandrelucchesi/Development/bloomfwd/ # Mounted host directory.
#PREFIXES_DISTRIBUTION_FILE=data/as6447_distrib.txt
#PREFIXES_FILE=data/as6447_prefixes.txt
PREFIXES_DISTRIBUTION_FILE=data/as65000_distrib.txt
PREFIXES_FILE=data/as65000_prefixes.txt
IPV4_ADDRESSES_FILE=data/addresses.txt
#ALGS_SERIAL=("bloomfwd")
ALGS_PARALLEL=("bloomfwd_par")
#THREADS=(2 4 8 16 24 32)
NUM_THREADS=32
FALSEP_RATIO=(0.01 0.02 0.05 0.1 0.2 0.4 0.8)

SCHED_CHUNKSIZE="dynamic,1"
OUTPUT_FILE=bench/res-falsep/cpu/lookup_falsep.csv # Benchmark output file.

cd $BLOOMFWD_DIR
mkdir -p bench/res-falsep/cpu/

# Clean old data files...
data_files=$(ls bench/res-falsep/cpu)
if [ ${#data_files} -gt 0 ]; then
	rm -f bench/res-falsep/cpu/*
fi

#export LD_LIBRARY_PATH=bin
export OMP_NUM_THREADS=$NUM_THREADS
export OMP_SCHEDULE="$SCHED_CHUNKSIZE"

# Write headers to output file.
printf "Algorithm, # False Positive Ratio, Execs...\n" >> $OUTPUT_FILE

# Write data to output file.
# Serial
for a in "${ALGS_SERIAL[@]}"
do
		printf "$a, 1: "
		printf "$a, 1" >> $OUTPUT_FILE

		for e in $(seq 1 5)  # Number of times to execute.
		do
			# Execute for input size 2^24 (16,777,216).
			exec_time=$(./bin/$a -d $PREFIXES_DISTRIBUTION_FILE \
			-p $PREFIXES_FILE -r $IPV4_ADDRESSES_FILE -n 16777216)

			printf "."
			printf ", $exec_time" >> $OUTPUT_FILE
		done
		printf "\n"
		printf "\n" >> $OUTPUT_FILE
done

# Parallel
for a in "${ALGS_PARALLEL[@]}"
do
	for t in "${FALSEP_RATIO[@]}"
	do
		# Recompile for each ratio.
		cd build/
		cmake -DCMAKE_BUILD_TYPE=Release -DBENCHMARK=1 -DFALSE_POSITIVE_RATIO=$t -DHASH_FUNCTION=H2_HASH .. &> /dev/null
		make &> /dev/null
		cd ..

#		./bin/$a -d $PREFIXES_DISTRIBUTION_FILE \
#		-p $PREFIXES_FILE -r $IPV4_ADDRESSES_FILE -n 16777216

		printf "$a, $t: "
		printf "$a, $t" >> $OUTPUT_FILE
			for e in $(seq 1 5)  # Number of times to execute.
			do
				# Assure the OpenMP environment variables are set and non-empty.
				: ${OMP_SCHEDULE:?"Need to set OMP_SCHEDULE non-empty."}
				: ${OMP_NUM_THREADS:?"Need to set OMP_NUM_THREADS non-empty."}

				# Execute for input size 2^24 (16,777,216).
				exec_time=$(./bin/$a -d $PREFIXES_DISTRIBUTION_FILE \
				-p $PREFIXES_FILE -r $IPV4_ADDRESSES_FILE -n 16777216)

				printf "."
				printf ", $exec_time" >> $OUTPUT_FILE
			done
		printf "\n"
		printf "\n" >> $OUTPUT_FILE
	done
done

