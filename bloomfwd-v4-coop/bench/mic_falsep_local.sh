#!/bin/bash

# Arguments
a=$1   # Algorithm
t=$2   # Number of threads
fp=$3  # False positive ratio
h1=$4  # Bloom filters' hash function
h2=$5  # Hash tables' hash function

# Settings
BLOOMFWD_DIR='/home/alexandrelucchesi/bloomfwd/' # Mounted host directory.
PREFIXES_DISTRIBUTION_FILE='/home/alexandrelucchesi/ip-datasets/ipv4/opt_20-24-32/as65000/bgptable/distrib.txt'
PREFIXES_FILE='/home/alexandrelucchesi/ip-datasets/ipv4/basic/as65000/bgptable/prefixes.txt'
DLA_FILE='/home/alexandrelucchesi/ip-datasets/ipv4/opt_20-24-32/as65000/bgptable/dla.txt'
G1_FILE='/home/alexandrelucchesi/ip-datasets/ipv4/opt_20-24-32/as65000/bgptable/g1.txt'
G2_FILE='/home/alexandrelucchesi/ip-datasets/ipv4/opt_20-24-32/as65000/bgptable/g2.txt'
IPV4_ADDRESSES_FILE='/home/alexandrelucchesi/ip-datasets/ipv4/addrs/matching-80.txt'

SCHED_CHUNKSIZE="dynamic,1"
OUTPUT_FILE=bench/res-opt-falsep/mic/lookup_falsep.csv # Benchmark output file.

cd $BLOOMFWD_DIR

export LD_LIBRARY_PATH=bin
export OMP_NUM_THREADS=$t
export OMP_SCHEDULE="$SCHED_CHUNKSIZE"

printf "$a, $t, $fp, $h1, $h2: "
printf "$a, $t, $fp, $h1, $h2" >> $OUTPUT_FILE
for e in $(seq 1 3)  # Number of times to execute.
do
	# Assure the OpenMP environment variables are set and non-empty.
	: ${OMP_SCHEDULE:?"Need to set OMP_SCHEDULE non-empty."}
	: ${OMP_NUM_THREADS:?"Need to set OMP_NUM_THREADS non-empty."}

	# Execute for input size 2^26 (67,108,864).
	#exec_time=$(./bin/$a -d $PREFIXES_DISTRIBUTION_FILE \
	#-p $PREFIXES_FILE -r $IPV4_ADDRESSES_FILE -n 67108864)
    exec_time=$(./bin/$a -d $PREFIXES_DISTRIBUTION_FILE \
    -dla $DLA_FILE -g1 $G1_FILE -g2 $G2_FILE -r $IPV4_ADDRESSES_FILE)

	printf "."
	printf ", $exec_time" >> $OUTPUT_FILE
done
printf "\n"
printf "\n" >> $OUTPUT_FILE

