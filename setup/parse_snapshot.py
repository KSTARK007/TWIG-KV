import argparse
import json

class Entry:
    def __init__(self, access_rate, cache_hits, cache_miss, disk_access, evicted, key_index, local_disk_access, remote_disk_access, total_accesses):
        self.access_rate = access_rate
        self.cache_hits = cache_hits
        self.cache_miss = cache_miss
        self.disk_access = disk_access
        self.evicted = evicted
        self.key_index = key_index
        self.local_disk_access = local_disk_access
        self.remote_disk_access = remote_disk_access
        self.total_accesses = total_accesses

    def __str__(self):
        return f"Entry(access_rate={self.access_rate}, cache_hits={self.cache_hits}, cache_miss={self.cache_miss}, disk_access={self.disk_access}, evicted={self.evicted}, key_index={self.key_index}, local_disk_access={self.local_disk_access}, remote_disk_access={self.remote_disk_access}, total_accesses={self.total_accesses})"

def parse_json(file_path):
    with open(file_path, 'r') as file:
        data = json.load(file)
    entries = []
    for item in data:
        entry = Entry(**item)
        entries.append(entry)
    return entries

def calculate_average_access_rate(entries):
    total_access_rate = sum(entry.access_rate for entry in entries)
    average_access_rate = total_access_rate / len(entries)
    return average_access_rate

def find_max_cache_hits(entries):
    max_cache_hits_entry = max(entries, key=lambda entry: entry.cache_hits)
    return max_cache_hits_entry

def calculate_total_disk_accesses(entries):
    total_local_disk_access = sum(entry.local_disk_access for entry in entries)
    total_remote_disk_access = sum(entry.remote_disk_access for entry in entries)
    total_disk_accesses = total_local_disk_access + total_remote_disk_access
    return total_disk_accesses

def find_max_evictions(entries):
    max_evictions_entry = max(entries, key=lambda entry: entry.evicted)
    return max_evictions_entry

def calculate_cache_miss_rate(entries):
    total_cache_miss = sum(entry.cache_miss for entry in entries)
    total_accesses = sum(entry.total_accesses for entry in entries)
    if(total_accesses == 0):
        return 0
    cache_miss_rate = total_cache_miss / total_accesses
    return cache_miss_rate

def calculate_cache_hit_rate(entries):
    total_cache_hits = sum(entry.cache_hits for entry in entries)
    total_accesses = sum(entry.total_accesses for entry in entries)
    if(total_accesses == 0):
        return 0
    cache_hit_rate = total_cache_hits / total_accesses
    return cache_hit_rate

def find_min_access_rate(entries):
    min_access_rate_entry = min(entries, key=lambda entry: entry.access_rate)
    return min_access_rate_entry

def calculate_average_cache_hits(entries):
    total_cache_hits = sum(entry.cache_hits for entry in entries)
    average_cache_hits = total_cache_hits / len(entries)
    return average_cache_hits

def calculate_total_misses(entries):
    total_misses = sum(entry.cache_miss for entry in entries)
    return total_misses

def calculate_total_hits(entries):
    total_hits = sum(entry.cache_hits for entry in entries)
    return total_hits

def calculate_total_evictions(entries):
    total_evictions = sum(entry.evicted for entry in entries)
    return total_evictions

def calculate_total_accesses(entries):
    total_accesses = sum(entry.total_accesses for entry in entries)
    return total_accesses

def calculate_total_access_rate(entries):
    total_accesses = sum(entry.access_rate for entry in entries)
    return total_accesses

def top_10_cache_hits(entries):
    top_10_cache_hits = sorted(entries, key=lambda entry: entry.cache_hits, reverse=True)[:10]
    return top_10_cache_hits

def top_10_cache_misses(entries):
    top_10_cache_misses = sorted(entries, key=lambda entry: entry.cache_miss, reverse=True)[:10]
    return top_10_cache_misses

def top_10_accesses(entries):
    top_10_accesses = sorted(entries, key=lambda entry: entry.total_accesses, reverse=True)[:10]
    return top_10_accesses

def top_10_evicted(entries):
    top_10_accesses = sorted(entries, key=lambda entry: entry.evicted, reverse=True)[:10]
    return top_10_accesses

def print_average_eviction(entries):
    print("Average eviction rate: ", calculate_total_evictions(entries) / len(entries))

def print_total_access_rate(entries):
    print("Total access rate: ", calculate_total_access_rate(entries))

def print_top_10_cache_hits(entries):
    top_10_cache_hit = top_10_cache_hits(entries)
    print("Top 10 entries by cache hits:")
    for entry in top_10_cache_hit:
        print(f"Key index: {entry.key_index}, Cache hits: {entry.cache_hits}")

def print_top_10_cache_misses(entries):
    top_10_cache_miss = top_10_cache_misses(entries)
    print("Top 10 entries by cache misses:")
    for entry in top_10_cache_miss:
        print(f"Key index: {entry.key_index}, Cache misses: {entry.cache_miss}")

def print_top_10_accesses(entries):
    top_10_access = top_10_accesses(entries)
    print("Top 10 entries by total accesses:")
    for entry in top_10_access:
        print(f"Key index: {entry.key_index}, Total accesses: {entry.total_accesses}")

def print_top_10_evicts(entries):
    top_10_evicts = top_10_evicted(entries)
    print("Top 10 entries by eviction:")
    for entry in top_10_evicts:
        print(f"Key index: {entry.key_index}, Cache misses: {entry.cache_miss}")

def perform_analysis(entries):
    average_access_rate = calculate_average_access_rate(entries)
    max_cache_hits_entry = find_max_cache_hits(entries)
    total_disk_accesses = calculate_total_disk_accesses(entries)
    max_evictions_entry = find_max_evictions(entries)
    cache_miss_rate = calculate_cache_miss_rate(entries)
    cache_hit_rate = calculate_cache_hit_rate(entries)
    min_access_rate_entry = find_min_access_rate(entries)
    average_cache_hits = calculate_average_cache_hits(entries)

    print(f"Average access rate: {average_access_rate}")
    print(f"Total accesses: {calculate_total_accesses(entries)}")
    print(f"Entry with the highest cache hits: {max_cache_hits_entry}")
    print(f"Total disk accesses: {total_disk_accesses}")
    print(f"Entry with the highest number of evictions: {max_evictions_entry}")
    print(f"Cache miss rate: {cache_miss_rate}")
    print(f"Cache hit rate: {cache_hit_rate}")
    print("Total evictions: ", calculate_total_evictions(entries))
    print(f"Entry with the minimum access rate: {min_access_rate_entry}")
    print(f"Average cache hits: {average_cache_hits}")
    print_top_10_cache_hits(entries)
    print_top_10_cache_misses(entries)
    print_top_10_accesses(entries)
    print_top_10_evicts(entries)
    print_average_eviction(entries)
    print_total_access_rate(entries)

def main():
    parser = argparse.ArgumentParser(description='Parse a JSON file and perform analysis on the entries')
    parser.add_argument('file', help='Path to the JSON file')
    args = parser.parse_args()

    entries = parse_json(f"../build/{args.file}")
    perform_analysis(entries)

if __name__ == "__main__":
    main()

