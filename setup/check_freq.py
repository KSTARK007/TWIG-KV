import os
import numpy as np
import matplotlib.pyplot as plt


def plot_cdf_graph(directory):
    keys = []

    # Iterate over each file in the directory
    for filename in os.listdir(directory):
        if filename.startswith("client_") and filename.endswith(".txt"):
            filepath = os.path.join(directory, filename)
            with open(filepath, 'r') as file:
                for line in file:
                    # print(line)
                    key, _ = line.split()
                    keys.append(int(key))
                    # _, key = line.split()
                    # keys.append(int(key.replace('user', '')))

    data_sorted = np.sort(keys)
    # x = np.array(data_sorted)
    unique, counts = np.unique(data_sorted, return_counts=True)
    # count_sort_ind = np.argsort(-counts)
    sorted_indices = np.argsort(-counts)
    unique_sorted = unique[sorted_indices]
    counts_sorted = counts[sorted_indices]

    # Writing to a file
    with open(f'{directory}/unique_counts_reversed.txt', "w") as file:
        for u, c in zip(unique_sorted, counts_sorted):
            file.write(f"{u}: {c}\n")

    # p = 1. * np.arange(len(data_sorted)) / (len(data_sorted) - 1)
    cumsum =  0 
    c = []
    for i in counts_sorted:
        cumsum += i
        c.append(cumsum)
    c = c / cumsum

    # Plot the CDF
    fig = plt.figure(figsize=(12, 6))
    ax1 = fig.add_subplot(121)
    ax1.plot(c)
    ax1.set_xlabel('$p$')
    ax1.set_ylabel('$x$')
    ax1.set_title('CDF of Keys')

    plt.tight_layout()
    plt.savefig(f'{directory}/cdf_grap.png')