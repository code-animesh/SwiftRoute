# SwiftRoute — Smart City-to-City Message Delivery System

A C++17 desktop application featuring a native Windows GUI that uses **Weighted Graphs** and **Dijkstra's Shortest Path Algorithm** ($O((V+E)\log V)$) to deliver messages across 9 Indian cities via the fastest available route, automatically rerouting around blocked connections.

## Features
- **9-City Interactive Network Map:** Visualizes servers across `Chandigarh`, `Delhi`, `Lucknow`, `Jaipur`, `Ahmedabad`, `Kolkata`, `Mumbai`, `Hyderabad`, and `Bengaluru`.
- **Automatic Fastest-Route Finder:** Computes minimum delivery time (ms) using Dijkstra's algorithm and highlights the chosen path in green.
- **Live Roadblock & Speed Simulation:** Block/unblock connections, change delivery speeds, or add new direct links between any two cities and watch the route adapt immediately.
- **Transparent Route Breakdown:** Displays the complete delivery history and the city-by-city shortest-time calculation table.

## How to Run (Windows)
Double-click `run.bat` to compile `src/main.cpp` and launch the GUI.
