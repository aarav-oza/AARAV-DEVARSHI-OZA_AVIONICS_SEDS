import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

# --- 1. Grab the data ---
data = pd.read_csv("Depth Data.csv")
data["Depth (m)"] = pd.to_numeric(data["Depth (m)"], errors="coerce")

# --- 2. Handle erratic/corrupted readings ---
# Flag values way outside a sane range as bad, instead of hardcoding point numbers.
# (tune the bounds to what the sensor should realistically read)
lower_bound, upper_bound = 0, 200
data.loc[
    (data["Depth (m)"] < lower_bound) | (data["Depth (m)"] > upper_bound),
    "Depth (m)"
] = np.nan

# Fill gaps left by corrupted/missing readings
data["Depth (m)"] = data["Depth (m)"].interpolate().bfill().ffill()

# --- 3. Reduce random noise (brownie points) ---
data["Smoothed Depth (m)"] = (
    data["Depth (m)"].rolling(window=7, center=True).mean().bfill().ffill()
)

# --- Static depth-vs-time graph, clearly labeled ---
plt.figure(figsize=(10, 5))
plt.plot(data["Point"], data["Depth (m)"], label="Cleaned", alpha=0.5)
plt.plot(data["Point"], data["Smoothed Depth (m)"], label="Smoothed", linewidth=2)
plt.xlabel("Time (s)")
plt.ylabel("Depth (m)")
plt.title("Ship Depth Over Time")
plt.legend()
plt.grid(True)
plt.savefig("depth_data.png")

# --- Animated graph: one new point per second ---
fig, ax = plt.subplots(figsize=(10, 5))
ax.set_xlim(data["Point"].min(), data["Point"].max())
ax.set_ylim(data["Smoothed Depth (m)"].min() - 10, data["Smoothed Depth (m)"].max() + 10)
ax.set_xlabel("Time (s)")
ax.set_ylabel("Depth (m)")
ax.set_title("Animated Ship Depth")
ax.grid(True)
line, = ax.plot([], [], linewidth=2)

def update(frame):
    line.set_data(data["Point"].iloc[:frame + 1], data["Smoothed Depth (m)"].iloc[:frame + 1])
    return line,

anim = FuncAnimation(fig, update, frames=len(data), interval=1000, blit=True)
anim.save("depth_animation.gif", writer="pillow", fps=1)