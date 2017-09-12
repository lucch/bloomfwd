#!/bin/bash

# Settings
BLOOMFWD_DIR="/home/alexandrelucchesi/Development/bloomfwd-coop"
#ALGS=("bloomfwd_opt_coop" "bloomfwd_opt_coop_async")
#ALGS=("bloomfwd_opt_coop")
ALGS=("bloomfwd_opt_coop_async")

CPU_DATA="/home/alexandrelucchesi/Development/ip-datasets/ipv4/opt_20-24-32/as65000/bgptable"
MIC_DATA="/home/micuser/fwtbl"	# Remember to mount this directory first!

CPU_DISTRIB="$CPU_DATA/distrib.txt"
MIC_DISTRIB="$MIC_DATA/distrib.txt"
CPU_DLA="$CPU_DATA/dla.txt"
MIC_DLA="$MIC_DATA/dla.txt"
CPU_G1="$CPU_DATA/g1.txt"
MIC_G1="$MIC_DATA/g1.txt"
CPU_G2="$CPU_DATA/g2.txt"
MIC_G2="$MIC_DATA/g2.txt"
#BUF_LEN=(32768 65536 131072 262144 524288 1048576 2097152 4194304 8388608 16777216 33554432 67108864)
BUF_LEN=(262144 524288 1048576)
#BUF_LEN=(2097152 4194304 8388608)
#BUF_LEN=(2097152)

ADDRS="/home/alexandrelucchesi/Development/ip-datasets/ipv4/addrs/matching-80.txt"

#ADDRS_COUNT=(33554432 67108864 134217728)
#ADDRS_COUNT=(134217728)
ADDRS_COUNT=(1073741824)

#MIC_RATIOS=(0.00 0.75 0.80 0.83 0.86 0.88 0.90 0.91 0.92 0.94 0.95 1.00)
MIC_RATIOS=(1.00)

OUTPUT_FILE=bench/res-opt-coop-exectime/lookup.csv # Benchmark output file.

cd $BLOOMFWD_DIR
mkdir -p bench/res-opt-coop-exectime/

# Clean old data files...
data_files=$(ls bench/res-opt-coop-exectime)
if [ ${#data_files} -gt 0 ]; then
	rm -f bench/res-opt-coop-exectime/*
fi

# Write headers to output file.
printf "Num. addresses, algorithm, Buffer size, Execs...\n" >> $OUTPUT_FILE

# Execute and write data to output file.
for n in "${ADDRS_COUNT[@]}"
do
    printf "Running for $n addresses...\n"
    for z in "${MIC_RATIOS[@]}"
    do
        for a in "${ALGS[@]}"
        do
            for b in "${BUF_LEN[@]}"
            do
                printf "$n, $a, $z, $b: "
                printf "$n, $a, $z, $b" >> $OUTPUT_FILE

                for e in $(seq 1 3)  # Number of times to execute.
                do
                    exec_time=$(./bin/$a -d $CPU_DISTRIB -D $MIC_DISTRIB \
                        -dla $CPU_DLA -DLA $MIC_DLA -g1 $CPU_G1 -G1 $MIC_G1 \
                        -g2 $CPU_G2 -G2 $MIC_G2 -b $b -z $z -r $ADDRS -n $n)

                    printf "."
                    printf ", $exec_time" >> $OUTPUT_FILE
                done
                printf "\n"
                printf "\n" >> $OUTPUT_FILE
            done
        done
    done

    printf "\n"
    printf "\n" >> $OUTPUT_FILE
done

mail -s 'coop script done!' alexandrelucchesi@gmail.com <<< 'Executed!'
