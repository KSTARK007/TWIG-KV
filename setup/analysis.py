from pathlib import Path
import json
import matplotlib.pyplot as plt
import argparse
from typing import List
import seaborn as sns

# Parse arguments
parser = argparse.ArgumentParser()
parser.add_argument('-p', '--path', nargs='+', type=Path, default='results/data')
args = parser.parse_args()

data_paths = args.path
plot_path = Path('results/plots')

num_clients = 0

all_metrics = {}
all_cache_metrics = {}
all_cache_dumps = {}
for data_path in data_paths:
    if not data_path.exists():
        raise FileNotFoundError(f'Path {data_path} does not exist')

    metrics = []
    cache_metrics = []
    cache_dumps = []
    for p in sorted(data_path.iterdir()):
        if p.exists() and p.suffix == '.json':
            with p.open() as f:
                data = json.load(f)
                if 'cache_metrics' in p.stem:
                    cache_metrics.append(data)
                elif 'metrics' in p.stem:
                    metrics.append(data)
        if p.suffix == '.txt':
            if 'cache_dump' in p.stem:
                cache_dumps.append(p)
        
    print('Metrics:', len(metrics))
    print('Cache metrics:', len(cache_metrics))
    print('Cache dumps:', len(cache_dumps))

    average_latency_us = 0
    average_throughput_s = 0
    num_metrics = 0
    for m in metrics:
        if m['average_latency_us'] > 0:
            num_clients += 1
            num_metrics += 1
            average_latency_us += m['average_latency_us']
            average_throughput_s += m['average_throughput_s']

    average_latency_us /= num_metrics
    average_throughput_s /= num_metrics

    cache_hit_rate = 0
    cache_miss_rate = 0
    for cm in cache_metrics:
        cache_hit = cm['cache_hit']
        cache_miss = cm['cache_miss']
        reads = cm['reads']

        cache_hit_rate += cache_hit / reads
        cache_miss_rate += cache_miss / reads

    cache_hit_rate /= len(cache_metrics)
    cache_miss_rate /= len(cache_metrics)

    all_metrics[data_path.stem] = {
        'average_latency_us': average_latency_us,
        'average_throughput_s': average_throughput_s,
        'cache_hit_rate': cache_hit_rate,
        'cache_miss_rate': cache_miss_rate
    }

extra_info = f'({int(num_clients / len(data_paths))} clients, 10 threads)'

# plot bar plot latency for each data path
sns.barplot(x=list(all_metrics.keys()), y=[m['average_latency_us'] for m in all_metrics.values()])
plt.title(f'Average Latency (us) {extra_info}')
plt.xlabel('Run type')
plt.ylabel('Average Latency (us)')
plt.xticks(rotation=45)
plt.tight_layout()
plt.savefig(plot_path / 'average_latency_us.png')
plt.clf()

# plot bar plot throughput for each data path
sns.barplot(x=list(all_metrics.keys()), y=[m['average_throughput_s'] for m in all_metrics.values()])
plt.title(f'Average Throughput (s) {extra_info}')
plt.xlabel('Run type')
plt.ylabel('Average Throughput (s)')
plt.xticks(rotation=45)
plt.tight_layout()
plt.savefig(plot_path / 'average_throughput_s.png')
plt.clf()

# plot bar plot cache hit rate for each data path
sns.barplot(x=list(all_metrics.keys()), y=[m['cache_hit_rate'] for m in all_metrics.values()])
plt.title(f'Cache Hit Rate {extra_info}')
plt.xlabel('Run type')
plt.ylabel('Cache Hit Rate')
plt.xticks(rotation=45)
plt.tight_layout()
plt.savefig(plot_path / 'cache_hit_rate.png')
plt.clf()

# fig, ax = plt.subplots()
# ax.set_title('Average Latency (us)')
# ax.set_xlabel('Data Path')
# ax.set_ylabel('Average Latency (us)')
# ax.bar(all_metrics.keys(), [m['average_latency_us'] for m in all_metrics.values()])



