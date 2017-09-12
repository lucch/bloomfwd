#!/bin/bash

# Settings
CC=icc
BLOOMFWD_DIR=/home/alexandrelucchesi/Development/bloomfwd/ # Host directory.
#PREFIXES_DISTRIBUTION_FILE=data/as6447_distrib.txt
#PREFIXES_FILE=data/as6447_prefixes.txt
#IPV4_ADDRESSES_FILE=data/randomAddrs.txt
#ALGS_SERIAL=("bloomfwd_opt_mic_intrin")
ALGS_PARALLEL=("bloomfwd_opt_mic_par_intrin")
#THREADS=(30 60 90 120 150 180 210 240 244)
THREADS=(244)
#FALSEP_RATIO=(0.005 0.01 0.02 0.05 0.1 0.2 0.4 0.6 0.8 0.9 0.95)
FALSEP_RATIO=(0.01 0.1 0.3 0.6 0.9)
BLOOM_HASH_FUNCTIONS=("BLOOM_MURMUR_HASH" "BLOOM_H2_HASH" "BLOOM_KNUTH_HASH")
HASHTBL_HASH_FUNCTIONS=("HASHTBL_MURMUR_HASH" "HASHTBL_H2_HASH" "HASHTBL_KNUTH_HASH")
#BLOOM_HASH_FUNCTIONS=("BLOOM_MURMUR_HASH")
#HASHTBL_HASH_FUNCTIONS=("HASHTBL_MURMUR_HASH")
SCHED_CHUNKSIZE="dynamic,1"
OUTPUT_FILE=bench/res-opt-falsep/mic/lookup_falsep.csv # Benchmark output file.

export CC=$CC

cd $BLOOMFWD_DIR
mkdir -p bench/res-opt-falsep/mic/

# Clean old data files...
data_files=$(ls bench/res-opt-falsep/mic)
if [ ${#data_files} -gt 0 ]; then
	rm -f bench/res-opt-falsep/mic/*
fi

# Write headers to output file.
printf "Algorithm, # False Positive Ratio, Execs...\n" >> $OUTPUT_FILE

## Write data to output file.
## Serial
#for a in "${ALGS_SERIAL[@]}"
#do
#		printf "$a, 1: "
#		printf "$a, 1" >> $OUTPUT_FILE
#
#		for e in $(seq 1 3)  # Number of times to execute.
#		do
#			# Execute for input size 2^26 (67,108,864).
#			exec_time=$(./bin/$a -d $PREFIXES_DISTRIBUTION_FILE \
#			-p $PREFIXES_FILE -r $IPV4_ADDRESSES_FILE -n 67108864)
#
#			printf "."
#			printf ", $exec_time" >> $OUTPUT_FILE
#		done
#		printf "\n"
#		printf "\n" >> $OUTPUT_FILE
#done

rm -rf build/*

# Parallel
for a in "${ALGS_PARALLEL[@]}"
do
	for t in "${THREADS[@]}"
	do
		for fp in "${FALSEP_RATIO[@]}"
		do
			for h1 in "${BLOOM_HASH_FUNCTIONS[@]}"
			do
				for h2 in "${HASHTBL_HASH_FUNCTIONS[@]}"
				do
					# Recompile for each ratio/hash functions.
					printf "Building ($a, $t, $fp, $h1, $h2)...\n"
					cd build/
					#rm -rf *
					cmake -DCMAKE_BUILD_TYPE=Release -DBENCHMARK=1 -DFALSE_POSITIVE_RATIO=$fp -DBLOOM_HASH_FUNCTION=$h1 -DHASHTBL_HASH_FUNCTION=$h2 .. &> /dev/null
					make &> /dev/null
					cd ..
					printf "Build complete!\n"
					# Setup MIC for execution
					ssh mic0 'bash -s' < bench/mic_falsep_local.sh $a $t $fp $h1 $h2
					printf "Executed on Phi!\n"
				done
			done
		done
	done
done

