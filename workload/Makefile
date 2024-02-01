CC = g++
CFLAGS = -std=c++11 -Wall
LDFLAGS = -o

include config.mk

all: main

main: main.cpp operations.cpp dataset.cpp common.h
	$(CC) $(CFLAGS) -DOP_FILE=$(OP_FILE) -DDATASET_FILE=$(DATASET_FILE) -DNUM_KEY_VALUE_PAIRS=$(NUM_KEY_VALUE_PAIRS) -DNUM_NODES=$(NUM_NODES) -DKEY_SIZE=$(KEY_SIZE) -DVALUE_SIZE=$(VALUE_SIZE) -DTOTAL_OPERATIONS=$(TOTAL_OPERATIONS) $(LDFLAGS) main main.cpp operations.cpp dataset.cpp

clean:
	rm -f main
