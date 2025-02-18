import json
import re
from pathlib import Path
import matplotlib.pyplot as plt
import csv
import argparse
import os

parser = argparse.ArgumentParser(description='Run code on cloudlab machines from xml config of cluster')
parser.add_argument("-c", "--char", choices=["A", "B", "C"], default="C", help="Key distribution type (default: 'uniform')")
parser.add_argument("-s", "--system", choices=["thread_safe_lru", "access_rate", "nchance", "access_rate_dynamic"], default="thread_safe_lru", help="Key distribution type (default: 'uniform')")
parser.add_argument("-d", "--distribution", choices=['uniform', 'zipfian', 'hotspot'], default="none", help="Key distribution type (default: 'uniform')")
args = parser.parse_args()

def plot_data(data, foldername):
    # Create the folder if it doesn't exist
    os.makedirs(foldername, exist_ok=True)
    print(foldername)

    for key, values in data.items():
        plt.figure(figsize=(10, 6))
        plt.plot(values, marker='o', linestyle='-', label=key)
        plt.title(f'{key} Over Time')
        plt.xlabel('Time Interval')
        plt.ylabel(key)
        plt.grid(True)
        plt.xticks(range(len(values)), range(1, len(values) + 1))
        plt.legend()
        filepath = os.path.join(foldername, f'{key}.png')
        plt.savefig(filepath)
        plt.close()

def calculate_sorensen_similarity(integer_sets):
    nsets = len(integer_sets)
    keys_union = set().union(*integer_sets)
    keys_intersect = set().intersection(*integer_sets)
    total_size = sum(len(s) for s in integer_sets)
    nunique = len(keys_union)
    if(nsets == 1):
        scale = 1
    else:
        scale = nsets / (nsets - 1)
    if (nunique / total_size) == 1:
        sorensen_similarity = 1
    else:
        sorensen_similarity = scale * (1 - (nunique / total_size))
    return sorensen_similarity

def extract_access_rate_from_filename(filename):
    access_rate_details = {}
    access_rate_details['access_rate'] = []
    access_rate_details['local'] = []
    access_rate_details['remote'] = []
    access_rate_details['performance'] = []
    access_rate_details['dup_allowed'] = []
    access_rate_details['curr_dup'] = []
    access_rate_details['system_dup'] = []
    file_data = open(filename, 'r').read()
    for line in file_data.splitlines():
        details = line.split(";")
        if(len(details) > 2):
            access_rate_details['access_rate'].append(int(details[0]))
            access_rate_details['local'].append(int(details[1]))
            access_rate_details['remote'].append(int(details[2]))
            access_rate_details['performance'].append(int(details[3]))
            access_rate_details['dup_allowed'].append(int(details[4]))
            access_rate_details['curr_dup'].append(int(details[5]))
            access_rate_details['system_dup'].append(int(details[6]))
    return(access_rate_details)

def write_to_csv(sorted_results, filename='metrics_summary.csv'):
    """Write the aggregated metrics to a CSV file."""
    headers = ['CacheSize', 'Throughput', 'Avg Latency', 'Latency50', 'Latency99', 'MissRate', 'RemoteHitRate', 'LocalHitRate', 'Disk_access', 'Remote_disk', 'DataCoverage', 'Similarity', 'SorensenSimilarity', 'AccessRate']
    
    with open(filename, 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(headers)  # Write the header row
        
        for result in sorted_results:
            row = [
                result['details']['cache_size'],
                result['throughput'],
                result['average_latency_us'],
                result['p50_latency'],
                result['p99_latency'],
                result['miss_rate'],
                result['remote_hit_rate'],
                result['local_hit_rate'],
                result['disk_accesses'],
                result['remote_disk_accesses'],
                result['data_coverage'],
                result['similarity'],
                result['sorensen_similarity'],
                result['details']['access_rate']
            ]
            writer.writerow(row)

def read_integers(file_path):
    with open(file_path, 'r') as file:
        integers = set(map(int, file.read().splitlines()))
    return integers

# base_directory = Path('backup/full_runs_backup')
# base_directory = Path('results/SINGLE_NODE_HOT_KEYS')
# base_directory = Path('results/hotspot_80_20')
# base_directory = Path('results/hotspot_95_5')
# base_directory = Path('results/zipfian_0.90')
# base_directory = Path('results/mixed_uniform_to_zipfian')
# base_directory = Path('results/mixed_uniform_to_hotspot')
# base_directory = Path('results/mixed_zipfian_to_hotspot')
base_directory = Path('results/uniform')
# base_directory = Path('results/Non_sticky_session')
# base_directory = Path('results_4min_5_servers/zipfian_0.99')
# base_directory = Path('results_4min_5_servers/uniform')
# base_directory = Path('results_4min_5_servers/hotspot_80_20')
# base_directory = Path('backup')

def average(lst):
    return sum(lst) / len(lst) if lst else 0

results = []

system = args.system
distribution = args.distribution
char = args.char
for subdirectory in base_directory.iterdir():
    if(distribution == 'none'):
        checkfile = subdirectory.is_dir() and subdirectory.name.startswith(f'{system}::{char}_')
    else:
        checkfile = subdirectory.is_dir() and subdirectory.name.startswith(f'{system}::{char}_{distribution}_Y')
    if (checkfile):
        system_type = subdirectory.name.split("::")[0]
        parts = subdirectory.name.split("::")[1].split('_')
        count = 0
        for part in parts:
            if(part.endswith('0')):
                break
            count += 1
                

        details = {
            'distribution': parts[1],
            'cache_size': int(int(parts[count])/1000),
            'num_servers': int(parts[count + 1][2:]),
            'num_clients': int(parts[count + 2][2:]),
            'num_clients_per_thread': int(parts[count + 3][4:]),
            'num_threads': int(parts[count + 4][2:]),
            'access_rate': int(parts[-1][4:])
        }

        num_total_nodes = int(details['num_clients']) + int(details['num_servers'])
        
        # Variables to accumulate metrics
        throughput = 0
        average_latency = []
        p50_latencies = []
        p99_latencies = []

        total_reads = 0
        misses = 0
        remote_hits = 0
        local_hits = 0
        freq_addition = 0
        disk_accesses = 0
        remote_disk_accesses = 0

        integer_sets = []

        access_rate_details = {}
        

        # Read and aggregate metrics based on num_clients
        for i in range(num_total_nodes):
            metrics_file = subdirectory / f'latency_results_{i}.json'
            if metrics_file.exists():
                with open(metrics_file, 'r') as file:
                    data = json.load(file)
                    throughput += data["total"].get("total_throughput", 0)  # Sum throughput
                    average_latency.append(data["total"].get("average_latency_us", 0))
                    p99_latencies.append(data["total"].get("p99", 0))
                    p50_latencies.append(data["total"].get("p50_latency_us", 0))
            cache_metrics_file = subdirectory / f'cache_metrics_{i + details["num_clients"]}.json'
            if cache_metrics_file.exists():
                with open(cache_metrics_file, 'r') as file:
                    cache_data = json.load(file)
                    remote_rdma_cache_hits_sum = sum(server["remote_rdma_cache_hits"] for server in cache_data["server_stats"])
                    total_reads += cache_data["total_reads"]
                    misses += cache_data["cache_miss"]
                    remote_hits += total_reads - misses - remote_disk_accesses
                    local_hits += cache_data["cache_hit"]
                    freq_addition += cache_data["cache_freq_addition"]
                    disk_accesses += cache_data["local_disk_access"]
                    remote_disk_accesses += cache_data["remote_disk_access"]
            cache_dump_file = subdirectory / f'cache_dump_{i + details["num_clients"]}.txt'
            if cache_dump_file.exists():
                integer_sets.append(read_integers(cache_dump_file))
            access_rate_file = subdirectory / f'access_rate_{i + details["num_clients"]}.txt'
            if access_rate_file.exists():
                access_rate_details[f'node_{i + details["num_clients"]}'] = extract_access_rate_from_filename(access_rate_file)
        # print(access_rate_details['node_3'])
        access_rate_file = subdirectory / f'access_rate_{i + details["num_clients"]}.txt'
        # if access_rate_file.exists():
        #     plot_data(access_rate_details['node_3'], f'{subdirectory}/data_series_plot')

        # Calculate averages
        average_latency_us = average(average_latency)
        p99_latency_us = average(p99_latencies)
        p50_latency_us = average(p50_latencies)
        
        if(total_reads == 0):
            total_reads = -1
        miss_rate = misses / total_reads
        if(misses == 0):
            misses = -1
        remote_hit_rate = (misses - disk_accesses - remote_disk_accesses) / misses
        local_hit_rate = local_hits / total_reads
        disk_accesses = disk_accesses / total_reads
        remote_disk_accesses = remote_disk_accesses / total_reads

        union_keys = set().union(*integer_sets)
        if(len(integer_sets) == 0 or len(integer_sets[0]) == 0):
            data_coverage = -1
            similarity = -1
            sorensen_similarity = -1
        else:
            intersection_keys = set(integer_sets[0]).intersection(*integer_sets[1:])
            data_coverage = len(union_keys) / 100000
            similarity = len(intersection_keys) / len(integer_sets[0])
            # sorensen_similarity = (2 * len(intersection_keys)) / (sum(len(s) for s in integer_sets))
            sorensen_similarity = calculate_sorensen_similarity(integer_sets)

        # Add the folder details and metrics to the results list
        results.append({
            "folder": subdirectory.name,
            "details": details,
            "throughput": throughput,
            "average_latency_us": average_latency_us,
            "p99_latency": p99_latency_us,
            "p50_latency": p50_latency_us,
            "miss_rate": miss_rate,
            "remote_hit_rate": remote_hit_rate,
            "local_hit_rate": local_hit_rate,
            "disk_accesses": disk_accesses,
            "remote_disk_accesses": remote_disk_accesses,
            "data_coverage": data_coverage,
            "similarity": similarity,
            "sorensen_similarity": sorensen_similarity,
            "freq_addition": freq_addition
        })

# Sort the results based on num_clients, num_threads, num_clients_per_thread
sorted_results = sorted(results, key=lambda x: (int(x["details"]["cache_size"]), int(x["details"]["access_rate"])))
sorted_by_throughput = sorted(results, key=lambda x: x["throughput"], reverse=True)
write_to_csv(sorted_results)
write_to_csv(sorted_by_throughput, filename='metrics_summary_throughput_sorted.csv')


# Print the sorted results
for result in sorted_by_throughput:
    print(f"Folder: {result['folder']}")
    print(f"Details: Servers={result['details']['num_servers']}, Clients={result['details']['num_clients']}, Clients/Thread={result['details']['num_clients_per_thread']}, Threads={result['details']['num_threads']}")
    print(f"Total tx_mps: {result['throughput']}")
    print(f"Avg average_latency_us: {result['average_latency_us']}")
    print(f"Avg p99_latency_us: {result['p99_latency']}")
    print(f"Avg p50_latency_us: {result['p50_latency']}")
    print(f"Miss Rate: {result['miss_rate']}")
    print(f"Remote Hit Rate: {result['remote_hit_rate']}")
    print(f"Local Hit Rate: {result['local_hit_rate']}")
    print(f"Data Coverage: {result['data_coverage']}")
    print(f"Similarity: {result['similarity']}")
    print(f"Sorensen Similarity: {result['sorensen_similarity']}")
    print(f"Freq Addition: {result['freq_addition']} \n")

