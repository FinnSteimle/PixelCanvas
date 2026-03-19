# Load Test Results

Performance metrics for the PixelCanvas distributed system across different concurrency levels.

## 100 Virtual Users (Baseline - 3 Runs)

| Metric | Run 1 | Run 2 | Run 3 |
| :--- | :--- | :--- | :--- |
| **Success Rate** | 100.00% | 99.98% | 99.98% |
| **Failed Requests** | 0 | 1 | 1 |
| **Total Requests** | 6,651 | 6,644 | 6,615 |
| **Avg Latency** | 3.24ms | 3.45ms | 4.94ms |
| **p95 Latency** | 6.75ms | 7.69ms | 13.33ms |
| **Throughput** | 131.76 reqs/s | 131.48 reqs/s | 131.78 reqs/s |
| **WS Messages** | 10,054 msgs/s | 10,058 msgs/s | 9,452 msgs/s |

## 500 Virtual Users (High Load)
* **Success Rate:** 100.00%
* **Requests:** 21,009
* **Average Latency:** 39.25ms
* **p95 Latency:** 110.87ms
* **Throughput:** 416.3 reqs/s
* **WebSocket Messages:** 95,400 msgs/s
* **Network Traffic:** 1.9 GB (Received)

## 1000 Virtual Users (Extreme Load)
* **Success Rate:** 99.98%
* **Requests:** 20,369
* **Average Latency:** 116.9ms
* **p95 Latency:** 442.57ms
* **Throughput:** 402.6 reqs/s
* **WebSocket Messages:** 74,685 msgs/s
