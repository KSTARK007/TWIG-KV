import os
import json
import seaborn as sns
import matplotlib.pyplot as plt
from pathlib import Path

# Directory containing the workload files
results_dir = "results"
plot_path = Path('results/plots')

# List to store the data
workload_data = []

# Iterate over the files in the directory
for filename in os.listdir(results_dir):
    if filename.startswith("workload_"):
        # Extract the workload type and number of clients from the filename
        workload_type = ' '.join(filename.split("_")[1:-5])
        print(workload_type)
        num_clients = filename.split("_")[-1]
        
        # Read the average throughput from metrics_0.json
        metrics_file = os.path.join(results_dir, filename, "metrics_0.json")
        with open(metrics_file) as f:
            metrics_data = json.load(f)
            average_throughput = metrics_data["average_throughput_s"]
        
        # Append the data to the list
        if workload_type == "SINGLE NODE HOT KEYS TO SECOND NODE ONLY":
            workload_type = "All nodes"
        else:
            workload_type = "Only second node"
        workload_data.append((workload_type, num_clients, average_throughput))

# Convert the data to a pandas DataFrame
import pandas as pd
df = pd.DataFrame(workload_data, columns=["Workload Type", "Number of Clients", "Average Throughput"])

# Sort the DataFrame by "Number of Clients" column
df["Number of Clients"] = pd.to_numeric(df["Number of Clients"])
df = df.sort_values("Number of Clients", ascending=True)

# Plot the graph using seaborn
sns.lineplot(data=df, x="Number of Clients", y="Average Throughput", hue="Workload Type")
plt.savefig(plot_path / "nodes.png")

# plt.show()
