#!/bin/bash

# Define the path to the Python program and the constant arguments
PROGRAM_PATH="sudo python3 setup.py"
IDENTITY_FILE="--identity_file /users/Khombal2/.ssh/id_rsa"
USERNAME="--username Khombal2"
GIT_SSH_KEY_PATH="--git_ssh_key_path /users/Khombal2/.ssh/id_rsa"
STEP="--step 12"
NUM_SERVERS="--num_servers 3"


# NUM_CLIENTS_START=3
# NUM_CLIENTS_END=3
# NUM_THREADS_START=1
# NUM_THREADS_END=16
# NUM_CLIENTS_PER_THREAD_START=1
# NUM_CLIENTS_PER_THREAD_END=5


# for ((num_clients=NUM_CLIENTS_START; num_clients<=NUM_CLIENTS_END; num_clients++)); do
#     for ((num_threads=NUM_THREADS_START; num_threads<=NUM_THREADS_END; num_threads++)); do
#         for ((num_clients_per_thread=NUM_CLIENTS_PER_THREAD_START; num_clients_per_thread<=NUM_CLIENTS_PER_THREAD_END; num_clients_per_thread++)); do
#             CMD="$PROGRAM_PATH $IDENTITY_FILE $USERNAME $GIT_SSH_KEY_PATH $STEP $NUM_SERVERS \
# --num_threads $num_threads --num_clients $num_clients \
# --num_clients_per_thread $num_clients_per_thread"


#             echo "Executing: $CMD"
#             eval $CMD
#         done
#     done
# done


NUM_CLIENTS_START=3
NUM_CLIENTS_END=3
NUM_THREADS_START=1
NUM_THREADS_END=4
NUM_CLIENTS_PER_THREAD_START=6
NUM_CLIENTS_PER_THREAD_END=15


for ((num_clients=NUM_CLIENTS_START; num_clients<=NUM_CLIENTS_END; num_clients++)); do
    for ((num_threads=NUM_THREADS_START; num_threads<=NUM_THREADS_END; num_threads++)); do
        for ((num_clients_per_thread=NUM_CLIENTS_PER_THREAD_START; num_clients_per_thread<=NUM_CLIENTS_PER_THREAD_END; num_clients_per_thread++)); do
            CMD="$PROGRAM_PATH $IDENTITY_FILE $USERNAME $GIT_SSH_KEY_PATH $STEP $NUM_SERVERS \
--num_threads $num_threads --num_clients $num_clients \
--num_clients_per_thread $num_clients_per_thread"


            echo "Executing: $CMD"
            eval $CMD
        done
    done
done



PROGRAM_PATH="sudo python3 setup_part.py"

NUM_THREADS_START=1
NUM_THREADS_END=16
NUM_CLIENTS_PER_THREAD_START=1
NUM_CLIENTS_PER_THREAD_END=5


for ((num_clients=NUM_CLIENTS_START; num_clients<=NUM_CLIENTS_END; num_clients++)); do
    for ((num_threads=NUM_THREADS_START; num_threads<=NUM_THREADS_END; num_threads++)); do
        for ((num_clients_per_thread=NUM_CLIENTS_PER_THREAD_START; num_clients_per_thread<=NUM_CLIENTS_PER_THREAD_END; num_clients_per_thread++)); do
            CMD="$PROGRAM_PATH $IDENTITY_FILE $USERNAME $GIT_SSH_KEY_PATH $STEP $NUM_SERVERS \
--num_threads $num_threads --num_clients $num_clients \
--num_clients_per_thread $num_clients_per_thread"


            echo "Executing: $CMD"
            eval $CMD
        done
    done
done

NUM_THREADS_START=1
NUM_THREADS_END=4
NUM_CLIENTS_PER_THREAD_START=6
NUM_CLIENTS_PER_THREAD_END=15


for ((num_clients=NUM_CLIENTS_START; num_clients<=NUM_CLIENTS_END; num_clients++)); do
    for ((num_threads=NUM_THREADS_START; num_threads<=NUM_THREADS_END; num_threads++)); do
        for ((num_clients_per_thread=NUM_CLIENTS_PER_THREAD_START; num_clients_per_thread<=NUM_CLIENTS_PER_THREAD_END; num_clients_per_thread++)); do
            CMD="$PROGRAM_PATH $IDENTITY_FILE $USERNAME $GIT_SSH_KEY_PATH $STEP $NUM_SERVERS \
--num_threads $num_threads --num_clients $num_clients \
--num_clients_per_thread $num_clients_per_thread"


            echo "Executing: $CMD"
            eval $CMD
        done
    done
done