import json
import re
from pathlib import Path
import matplotlib.pyplot as plt
import csv

def write_to_csv(sorted_results, filename='metrics_summary.csv'):
    """Write the aggregated metrics to a CSV file."""
    headers = ['Server', 'Client', 'ClientsPerThread', 'Thread', 'Throughput', 'Latency50', 'Latency99']
    
    with open(filename, 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(headers)  # Write the header row
        
        for result in sorted_results:
            row = [
                result['details']['num_servers'],
                result['details']['num_clients'],
                result['details']['num_clients_per_thread'],
                result['details']['num_threads'],
                result['throughput'],
                result['average_latency_us'],
                result['average_tail_latency_us']
            ]
            writer.writerow(row)

# Define the base directory containing the result folders
base_directory = Path('results')

# Regular expression to match folder names and extract components
folder_name_regex = re.compile(r'zzzz_RANDOM_DISTRIBUTION_RDMA_data_ns(?P<num_servers>\d+)_nc(?P<num_clients>\d+)_ncpt(?P<num_clients_per_thread>\d+)_nt(?P<num_threads>\d+)')

# Function to average values in a list
def average(lst):
    return sum(lst) / len(lst) if lst else 0

# List to store the details and metrics for sorting
results = []

# Iterate over each subdirectory in the base directory that matches the pattern
for subdirectory in base_directory.iterdir():
    if subdirectory.is_dir():  # Ensure it's a directory
        match = folder_name_regex.match(subdirectory.name)
        if match:  # Folder name matches the pattern
            details = match.groupdict()  # Extract details as a dictionary
            num_clients = int(details['num_clients'])
            
            # Variables to accumulate metrics
            throughput = 0
            p50_latencies = []
            p99_latencies = []

            # Read and aggregate metrics based on num_clients
            for i in range(num_clients):
                metrics_file = subdirectory / f'latency_results_{i}.json'
                if metrics_file.exists():
                    with open(metrics_file, 'r') as file:
                        data = json.load(file)
                        throughput += data["total"].get("total_throughput", 0)  # Sum throughput
                        p50_latencies.append(data["total"].get("average_latency_us", 0))
                        p99_latencies.append(data["total"].get("average_tail_latency_us", 0))

            # Calculate averages
            average_latency_us = average(p50_latencies)
            average_tail_latency_us = average(p99_latencies)

            # Add the folder details and metrics to the results list
            results.append({
                "folder": subdirectory.name,
                "details": details,
                "throughput": throughput,
                "average_latency_us": average_latency_us,
                "average_tail_latency_us": average_tail_latency_us
            })

# Sort the results based on num_clients, num_threads, num_clients_per_thread
sorted_results = sorted(results, key=lambda x: (int(x["details"]["num_clients"]), int(x["details"]["num_threads"]), int(x["details"]["num_clients_per_thread"])))
sorted_by_throughput = sorted(results, key=lambda x: x["throughput"], reverse=True)
write_to_csv(sorted_results)
write_to_csv(sorted_by_throughput, filename='metrics_summary_throughput_sorted.csv')


# Print the sorted results
for result in sorted_by_throughput:
    print(f"Folder: {result['folder']}")
    print(f"Details: Servers={result['details']['num_servers']}, Clients={result['details']['num_clients']}, Clients/Thread={result['details']['num_clients_per_thread']}, Threads={result['details']['num_threads']}")
    print(f"Total tx_mps: {result['throughput']}")
    print(f"Avg average_latency_us: {result['average_latency_us']}")
    print(f"Avg p99_latency_us: {result['average_tail_latency_us']}\n")
