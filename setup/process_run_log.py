import os
import matplotlib.pyplot as plt
import re
import sys

filename = sys.argv[1]
outfile = sys.argv[2]

start_processing = False
ops_values = []

with open(filename, 'r') as log_file:
    log_lines = log_file.readlines()

for line in log_lines:
    ops_match = re.search(r"Ops \[\d+\] \+\[(\d+)\]", line)
    if ops_match:
        ops_value = int(ops_match.group(1))
        if start_processing:
            ops_values.append(ops_value)
        if ops_value >= 10000000000000000000:
            start_processing = True

while ops_values and ops_values[-1] == 0:
    ops_values.pop()

for i in ops_values:
    print(f"{i}")

plt.plot(ops_values)
plt.xlabel('time (s)')
plt.ylabel('operations')
plt.title('operations over time')
plt.grid(True, which='both', linestyle='--')
print(f"saving output in {outfile}")
plt.savefig(f"{outfile}", dpi=600)
