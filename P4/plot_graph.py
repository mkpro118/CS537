import numpy as np
import matplotlib.pyplot as plt


data = np.genfromtxt('data.csv', delimiter=',')
proc_ticks, global_ticks, pids = data[:, 0], data[:, 2], data[:, 1]

unique_pids = np.unique(pids)

fig, ax = plt.subplots(nrows=1, ncols=1)

for pid in unique_pids:
    idxs = (pids == pid)

    ax.plot(proc_ticks[idxs], global_ticks[idxs], label=f'pid = {pid}')

plt.show()
