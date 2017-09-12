#!/bin/bash

# Settings
CC=icc
PHI_BLOOMFWD_DIR=/home/alexandrelucchesi/Development/bloomfwd/ # Mounted host directory.
PREFIXES_DISTRIBUTION_FILE='/home/alexandrelucchesi/Development/ip-datasets/ipv4/opt_20-24-32/as65000/bgptable/distrib.txt'
PREFIXES_FILE='/home/alexandrelucchesi/Development/ip-datasets/ipv4/basic/as65000/bgptable/prefixes.txt'
DLA_FILE='/home/alexandrelucchesi/Development/ip-datasets/ipv4/opt_20-24-32/as65000/bgptable/dla.txt'
G1_FILE='/home/alexandrelucchesi/Development/ip-datasets/ipv4/opt_20-24-32/as65000/bgptable/g1.txt'
G2_FILE='/home/alexandrelucchesi/Development/ip-datasets/ipv4/opt_20-24-32/as65000/bgptable/g2.txt'
ADDRS_FILES=( "matching-0.txt" "matching-10.txt" "matching-20.txt" "matching-30.txt" "matching-40.txt" "matching-50.txt" "matching-60.txt" "matching-70.txt" "matching-80.txt" "matching-90.txt" "matching-100.txt" )
ALGS_PARALLEL=("bloomfwd_opt_par")
THREADS=(32)
SCHED_CHUNKSIZE="dynamic,1"
OUTPUT_FILE=bench/res_opt/cpu/def_route_opt.csv # Benchmark output file.

cd $PHI_BLOOMFWD_DIR
mkdir -p bench/res_opt/cpu/

# Clean old data files...
data_files=$(ls bench/res_opt/cpu)
if [ ${#data_files} -gt 0 ]; then
	rm -f bench/res_opt/cpu/*
fi

#export LD_LIBRARY_PATH=bin
export OMP_SCHEDULE="$SCHED_CHUNKSIZE"

# Write headers to output file.
printf "Algorithm, # Threads, Input, Execs...\n" >> $OUTPUT_FILE

# Write data to output file.
# Serial
#for a in "${ALGS_SERIAL[@]}"
#do
#		printf "$a, 1: "
#		printf "$a, 1" >> $OUTPUT_FILE
#
#		for e in $(seq 1 3)  # Number of times to execute.
#		do
#			# Execute for input size 2^26 (67,108,864).
#			#exec_time=$(./bin/$a -d $PREFIXES_DISTRIBUTION_FILE \
#			#-p $PREFIXES_FILE -r $IPV4_ADDRESSES_FILE -n 67108864)
#			exec_time=$(./bin/$a -d $PREFIXES_DISTRIBUTION_FILE \
#			-p $PREFIXES_FILE -r $IPV4_ADDRESSES_FILE)
#
#			printf "."
#			printf ", $exec_time" >> $OUTPUT_FILE
#		done
#		printf "\n"
#		printf "\n" >> $OUTPUT_FILE
#done

# Parallel
for a in "${ALGS_PARALLEL[@]}"
do
	for t in "${THREADS[@]}"
	do
		export OMP_NUM_THREADS=$t
            for f in "${ADDRS_FILES[@]}"
            do
                printf "$a, $t, $f: "
                printf "$a, $t, $f" >> $OUTPUT_FILE

                filepath="/home/alexandrelucchesi/Development/ip-datasets/ipv4/addrs/"$f

                for e in $(seq 1 3)  # Number of times to execute.
                do
                    # Assure the OpenMP environment variables are set and non-empty.
                    : ${OMP_SCHEDULE:?"Need to set OMP_SCHEDULE non-empty."}
                    : ${OMP_NUM_THREADS:?"Need to set OMP_NUM_THREADS non-empty."}

                    # Execute for input size 2^26 (67,108,864).
                    #exec_time=$(./bin/$a -d $PREFIXES_DISTRIBUTION_FILE \
                    #-p $PREFIXES_FILE -r $filepath -n 67108864)
                    exec_time=$(./bin/$a -d $PREFIXES_DISTRIBUTION_FILE \
                    -dla $DLA_FILE -g1 $G1_FILE -g2 $G2_FILE -r $filepath)

                    printf "."
                    printf ", $exec_time" >> $OUTPUT_FILE
                done
                printf "\n"
                printf "\n" >> $OUTPUT_FILE
            done
	done
done

