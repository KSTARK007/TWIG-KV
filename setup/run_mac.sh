#!/bin/bash

# Define the function 'r'
r() {
    cd /Users/khombal2/Desktop/ldc/LDC/setup
    source ~/python_env/bin/activate
    python3 setup.py --identity_file /users/Khombal2/.ssh/id_rsa --username Khombal2 --git_ssh_key_path /users/Khombal2/.ssh/id_rsa --step 12 --git_branch "$1" --cache_size "$2"
}

System_type=

# Calls to the function 'r' with various cache sizes
r nchance-addition 0.10
r nchance-addition 0.20
r nchance-addition 0.25
r nchance-addition 0.30
r nchance-addition 0.334
r nchance-addition 0.35