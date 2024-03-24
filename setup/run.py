import json
import subprocess
from pathlib import Path
from tqdm import tqdm

dataset_sizes = [100000]
block_sizes = [4096]
ops_sizes = [s * 10 for s in dataset_sizes]
workloads = ['RANDOM_DISTRIBUTION', 'SINGLE_NODE_HOT_KEYS', 'SINGLE_NODE_HOT_KEYS_TO_SECOND_NODE_ONLY', 'SINGLE_NODE_HOT_KEYS_TO_SECOND_NODE_SPLIT', 'SINGLE_NODE_HOT_KEYS_TO_SECOND_NODE_RDMA_ONLY']
# workloads = ['RANDOM_DISTRIBUTION']
# workloads = ['SINGLE_NODE_HOT_KEYS']
workloads = ['SINGLE_NODE_HOT_KEYS', 'YCSB', 'RANDOM_DISTRIBUTION']
value_sizes = [100]
cache_sizes = [int(s * 0.34) for s in dataset_sizes]
baselines = ["client_aware"]
one_sided_rdma_enableds = [True, False]
# one_sided_rdma_enableds = [True]
num_threads_ = [5]
# num_clients_ = [i for i in range(1, 4)]
num_clients_ = [3]
num_clients_per_thread_ = [i for i in range(5, 6)]
num_servers_ = [3]
# disk_async_ = [True, False]
disk_async_ = [True]
operations_pollute_cache_ = [True, False]

def run(config):
    print(config)
    config_str = json.dumps(config)
    # logging = subprocess.PIPE
    logging = None
    # logging = subprocess.DEVNULL
    for i in range(3):
        try:
            subprocess.run(["python", "setup.py", "--step=12", "--preload_config", config_str], check=True, stdout=logging, stderr=logging, timeout=60 * 10)
            break
        except Exception as e:
            print(e)
            pass

    # data_path = Path('results/data')
    # if not data_path.exists():
    #     raise FileNotFoundError(f'Path {data_path} does not exist')

    # try:
    #     new_data_path = Path(f'results/data_dataset_size_{config["dataset_size"]}_block_size_{config["block_size"]}_ops_size_{config["ops_size"]}_workload_{config["workload"]}_value_size_{config["value_size"]}_cache_size_{config["cache_size"]}_baseline_{config["baseline"]}_one_sided_rdma_enabled_{config["one_sided_rdma_enabled"]}_num_threads_{config["num_threads"]}_num_clients_{config["num_clients"]}_num_clients_per_thread_{config["num_clients_per_thread"]}_num_servers_{config["num_servers"]}')
    #     new_data_path = Path(f'results/workload_{config["workload"]}_num_clients_per_thread_{config["num_clients_per_thread"]}')
    #     data_path.rename(new_data_path)
    # except Exception as e:
    #     print(e)
    #     pass

generated_configs = []
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
                                            for disk_async in disk_async_:
                                                for num_servers in num_servers_:
                                                    for operations_pollute_cache in operations_pollute_cache_:
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
                                                            'num_servers': num_servers,
                                                            'disk_async': disk_async,
                                                            'operations_pollute_cache': operations_pollute_cache,
                                                        }
                                                        generated_configs.append(config)

for config in tqdm(generated_configs):
    run(config)