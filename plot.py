import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

data = pd.read_csv('large_grid_search.csv')


def create_surface_plots(data, value_column, title):
  fig = plt.figure(figsize=(14, 8))
  ax = fig.add_subplot(111, projection='3d')

  threads = sorted(data['CLIENT_THREADS'].unique())
  colors = plt.cm.viridis(np.linspace(0, 1, len(threads)))

  for c, t in zip(colors, threads):
    subset = data[data['CLIENT_THREADS'] == t]

    pivot_table = subset.pivot_table(values=value_column, index='PAGE_SIZE',
                                     columns='RING_SIZE', aggfunc='mean')

    X, Y = np.meshgrid(pivot_table.columns, pivot_table.index)
    Z = pivot_table.values

    ax.plot_surface(X, Y, Z, rstride=1, cstride=1, color=c, alpha=0.5,
                    linewidth=0, antialiased=False)

  ax.set_xlabel('RING_SIZE')
  ax.set_ylabel('PAGE_SIZE')
  ax.set_zlabel(value_column)
  ax.set_title(title)

  sm = plt.cm.ScalarMappable(cmap='viridis',
                             norm=plt.Normalize(vmin=min(threads),
                                                vmax=max(threads)))
  sm._A = []
  cbar = fig.colorbar(sm, ax=ax, shrink=0.5, aspect=10)
  cbar.set_label('CLIENT_THREADS')

  return fig, ax


fig_rate, ax_rate = create_surface_plots(data, 'AverageRate(it/s)',
                                         'Surface Plot for Average Rate')
fig_gbps, ax_gbps = create_surface_plots(data, 'AverageGbps',
                                         'Surface Plot for Average Gbps')

plt.show()
