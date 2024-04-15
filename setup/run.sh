#!/bin/bash

# Define constants
PROGRAM_PATH="sudo python3 setup.py"
IDENTITY_FILE="--identity_file /users/Khombal2/.ssh/id_rsa"
USERNAME="--username Khombal2"
GIT_SSH_KEY_PATH="--git_ssh_key_path /users/Khombal2/.ssh/id_rsa"
NUM_SERVERS="--num_servers 3"
# CACHE_SIZE=(0.10 0.20 0.25 0.30 0.334 0.35)
# SYSTEM_NAMES=("A" "B" "C" "D")

CACHE_SIZE=(0.334)
SYSTEM_NAMES=("A")
# A : Cache Unaware Routing + Admit + Disk
# B : Cache Unaware Routing + no-Admit + Disk
# C : Cache Unaware Routing + no-Admit + RDMA = old(LDC)
# D : Cache Aware Routing + no-Admit = Perfect system

SYS_NAME="A"

# Function to execute the command
execute_cmd() {
    local num_clients=$1
    local num_threads=$2
    local num_clients_per_thread=$3
    local step=$4
    local distribution=$5
    local system_name=$6
    #itr throu
    for cache_size in "${CACHE_SIZE[@]}"; do
        CMD="$PROGRAM_PATH $IDENTITY_FILE $USERNAME $GIT_SSH_KEY_PATH --step $step $NUM_SERVERS \
--num_threads $num_threads --system_type $system_name --distribution $distribution --num_clients $num_clients \
--num_clients_per_thread $num_clients_per_thread --git_branch nchance-addition --cache_size $cache_size"
    done

    echo "Executing: $CMD"
    eval $CMD
}

# Function to iterate through ranges and execute commands
iterate_and_execute() {
    local num_clients_start=$1
    local num_clients_end=$2
    local num_threads_start=$3
    local num_threads_end=$4
    local num_clients_per_thread_start=$5
    local num_clients_per_thread_end=$6
    local distribution=$7
    local system_name=$8


    for ((num_clients=num_clients_start; num_clients<=num_clients_end; num_clients++)); do
        for ((num_threads=num_threads_start; num_threads<=num_threads_end; num_threads++)); do
            for ((num_clients_per_thread=num_clients_per_thread_start; num_clients_per_thread<=num_clients_per_thread_end; num_clients_per_thread++)); do
                # execute_cmd $num_clients $num_threads $num_clients_per_thread 13 $distribution $system_name
                execute_cmd $num_clients $num_threads $num_clients_per_thread 12 $distribution $system_name
            done
        done
    done
}

# Execute with specified ranges and steps
for system_name in "${SYSTEM_NAMES[@]}"; do
    iterate_and_execute 3 3 8 8 2 2 "hotspot" $system_name
    iterate_and_execute 3 3 8 8 2 2 "uniform" $system_name
    iterate_and_execute 3 3 8 8 2 2 "zipfian" $system_name
done