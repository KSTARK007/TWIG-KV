import json
import subprocess
from pathlib import Path

dataset_sizes = [100000]
block_sizes = [4096]
ops_sizes = [s * 10 for s in dataset_sizes]
workloads = ['RANDOM_DISTRIBUTION', 'PARTITIONED_DISTRIBUTION', 'ZIPFIAN_DISTRIBUTION']
# workloads = ['PARTITIONED_DISTRIBUTION']
value_sizes = [100]
cache_sizes = [int(s * 0.34) for s in dataset_sizes]
baselines = ["client_aware"]
one_sided_rdma_enableds = [True, False]
# one_sided_rdma_enableds = [True]
num_threads_ = [1]
num_clients_ = [3]
num_clients_per_thread_ = [4]
num_servers_ = [3]

def run(config):
    print(config)
    config_str = json.dumps(config)
    # logging = subprocess.PIPE
    logging = None
    subprocess.run(["python", "setup.py", "--step=12", "--preload_config", config_str], check=True, stdout=logging, stderr=logging)

    data_path = Path('results/data')
    if not data_path.exists():
        raise FileNotFoundError(f'Path {data_path} does not exist')

    try:
        new_data_path = Path(f'results/data_dataset_size_{config["dataset_size"]}_block_size_{config["block_size"]}_ops_size_{config["ops_size"]}_workload_{config["workload"]}_value_size_{config["value_size"]}_cache_size_{config["cache_size"]}_baseline_{config["baseline"]}_one_sided_rdma_enabled_{config["one_sided_rdma_enabled"]}_num_threads_{config["num_threads"]}_num_clients_{config["num_clients"]}_num_clients_per_thread_{config["num_clients_per_thread"]}_num_servers_{config["num_servers"]}')
        new_data_path = Path(f'results/workload_{config["workload"]}_num_clients_per_thread_{config["num_clients_per_thread"]}')
        data_path.rename(new_data_path)
    except Exception as e:
        print(e)
        pass

for dataset_size in dataset_sizes:
    for block_size in block_sizes:
        for ops_size in ops_sizes:
            for workload in workloads:
                for value_size in value_sizes:
                    for cache_size in cache_sizes:
                        for baseline in baselines:
                            for one_sided_rdma_enabled in one_sided_rdma_enableds:
                                for num_threads in num_threads_:
                                    for num_clients in num_clients_:
                                        for num_clients_per_thread in num_clients_per_thread_:
                                            for num_servers in num_servers_:
                                                config = {
                                                    'dataset_size': dataset_size,
                                                    'true_block_size': block_size,
                                                    'block_size': block_size,
                                                    'ops_size': ops_size,
                                                    'workload': workload,
                                                    'value_size': value_size,
                                                    'cache_size': cache_size,
                                                    'baseline': baseline,
                                                    'one_sided_rdma_enabled': one_sided_rdma_enabled,
                                                    'num_threads': num_threads,
                                                    'num_clients': num_clients,
                                                    'num_clients_per_thread': num_clients_per_thread,
                                                    'num_servers': num_servers
                                                }
                                                run(config)