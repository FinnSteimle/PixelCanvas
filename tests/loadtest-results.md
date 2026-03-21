# Load Test Results

Performance metrics for the PixelCanvas distributed system across different concurrency levels.

## 100 Virtual Users (Baseline - 3 Runs)

| Metric | Run 1 | Run 2 | Run 3 |
| :--- | :--- | :--- | :--- |
| **Success Rate** | 100.00% | 100.00% | 100.00% |
| **Failed Requests** | 0 | 0 | 0 |
| **Total Requests** | 6,620 | 6,664 | 6,660 |
| **Avg Latency** | 5.12ms | 2.07ms | 2.28ms |
| **p95 Latency** | 10.07ms | 5.57ms | 5.40ms |
| **Throughput** | 130.75 reqs/s | 132.11 reqs/s | 131.98 reqs/s |
| **WS Messages** | 10,011 msgs/s | 10,259 msgs/s | 10,138 msgs/s |

## 500 Virtual Users (High Load)

| Metric | Result |
| :--- | :--- |
| **Success Rate** | 99.99% |
| **Failed Requests** | 1 |
| **Total Requests** | 23,725 |
| **Avg Latency** | 29.58ms |
| **p95 Latency** | 80.52ms |
| **Throughput** | 470.53 reqs/s |
| **WS Messages** | 124,763 msgs/s |
| **Network Traffic** | 2.2 GB (Received) |

## 1000 Virtual Users (Extreme Load)

| Metric | Result |
| :--- | :--- |
| **Success Rate** | 99.98% |
| **Failed Requests** | 4 |
| **Total Requests** | 22,129 |
| **Avg Latency** | 51.74ms |
| **p95 Latency** | 124.2ms |
| **Throughput** | 437.15 reqs/s |
| **WS Messages** | 101,449 msgs/s |
| **Network Traffic** | 2.0 GB (Received) |
