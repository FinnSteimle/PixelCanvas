# Load Test Results

## Run 1
* **Status 200:** 89.41% (16,739 successes / 1,982 failures)
* **Average Request Duration:** 5.46ms
* **Max Request Duration:** 236.01ms
* **p(95) Request Duration:** 11.29ms
* **Total Requests:** 18,723
* **Data Transferred:** 73 MB Received / 5.4 MB Sent

## Run 2
* **Status 200:** 82.65% (15,763 successes / 3,308 failures)
* **Average Request Duration:** 3.54ms
* **Max Request Duration:** 183.20ms
* **p(95) Request Duration:** 9.91ms
* **Total Requests:** 19,073
* **Data Transferred:** 155 MB Received / 5.6 MB Sent

## Run 3
* **Status 200:** 99.89% (12,177 successes / 13 failures)
* **Average Request Duration:** 68.45ms
* **Max Request Duration:** 1m0s
* **p(95) Request Duration:** 8.51ms
* **Total Requests:** 12,192
* **Data Transferred:** 128 MB Received / 3.6 MB Sent

---

### Comparison Summary

| Metric | Run 1 | Run 2 | Run 3 |
| :--- | :--- | :--- | :--- |
| **Success Rate** | 89.41% | 82.65% | 99.89% |
| **Failures** | 1,982 | 3,308 | 13 |
| **Total Requests** | 18,723 | 19,073 | 12,192 |
| **Avg Duration** | 5.46ms | 3.54ms | 68.45ms |
| **P95 Duration** | 11.29ms | 9.91ms | 8.51ms |

**Analysis Note:** Run 3 shows an artificially high success rate (99.89%) and high average duration (68.45ms) because the total system throughput dropped significantly (only 12,192 requests processed). The 13 failed requests hung for the maximum 60 seconds, locking up threads and preventing the system from processing the ~19k requests seen in Runs 1 and 2.