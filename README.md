# PixelCanvas: Distributed Collaborative Canvas

PixelCanvas is a fault-tolerant, real-time, distributed clone of r/place. It allows multiple users to paint simultaneously on a shared 50x50 grid. The system is designed to survive the sudden failure of backend nodes without data loss or service interruption, meeting all requirements for the Distributed Systems Challenge Task.

## Features & Requirement Mapping

* **Distributed System:** Runs multiple C++ backend instances behind an Nginx load balancer.
* **Fault Tolerance:** If a backend node is terminated, Nginx routes traffic to the surviving node, and clients automatically reconnect their WebSockets.
* **JWT Authentication:** Users must register and log in via REST endpoints. The resulting JWT is strictly required to open a WebSocket connection.
* **Persistent Storage:** PostgreSQL stores user credentials (hashed) and the state of the 2,500-pixel canvas.
* **Real-Time Synchronization:** Redis Pub/Sub broadcasts pixel updates across all isolated backend instances.
* **Single-Command Deployment:** The entire stack, including database seeding, builds and starts via a single Docker Compose command.

## Architecture Stack

* **Frontend:** HTML5 Canvas, Vanilla JS, CSS (Served directly by Nginx)
* **Backend:** Modern C++ (C++17) using the Crow v1.3.1 microframework
* **Database:** PostgreSQL 18 (via `libpqxx`)
* **Message Broker:** Redis 8.6 (via `redis-plus-plus` 1.3.15)
* **Security:** Argon2id password hashing (via `libsodium`), JWT (`jwt-cpp` v0.7.2)
* **Infrastructure:** Docker, Docker Compose, Nginx 1.28.2-alpine (Reverse Proxy & Load Balancer)

## Prerequisites

* Docker and Docker Compose installed.
* Port `8080` available on your local machine.

## Quick Start

1. **Configure Secrets:**
   ```bash
   cp .env.example .env
   ```

2. **Launch the System:**
   ```bash
   docker-compose up --build -d
   ```

3. **Access the Application:**
   Open your browser and navigate to `http://localhost:8080`.

## Testing the System

### 1. Authentication & Drawing
* Click **Register** to create a new user.
* Click **Login** to authenticate and receive a JWT.
* Select a color and click on the canvas. Open a second browser window to see real-time synchronization.

### 2. Fault Tolerance (Failover Demo)
To prove the system survives a node failure:
1. Open the application and log in.
2. In your terminal, kill one of the backend nodes:
   ```bash
   docker stop pixelcanvas-backend1-1
   ```
3. Observe the frontend UI: The connection status will briefly drop to `Offline - Reconnecting...` before turning back to `Online` as it transparently reconnects to `backend2` via Nginx.
4. Continue drawing. The system remains fully operational.

### 3. Load Testing & Architectural Evolution
To evaluate the system's performance and identify physical limits, a load test is executed against the JWT-protected `GET /canvas` endpoint using **k6** (v0.50.0). 

**Execution:**
Ensure the cluster is running, then execute the test script from the project root:
```bash
k6 run tests/loadtest.js
```

**Test Profile & Results (Post-Optimization):**
* **Load:** Ramped up to 50 concurrent Virtual Users (VUs) sustained over 50 seconds.
* **Peak Throughput:** ~370 requests/second.
* **Average Latency:** ~7ms.
* **Peak P(95) Latency:** 11.8ms to 13.87ms.
* **Total Requests:** ~18,500+ requests per run, achieving a flawless **100.00% success rate** under maximum stress.

**Architectural Resolution: Thread-Safe Connection Pools & Poison Control**
Initial load testing revealed a critical vulnerability: under extreme CPU starvation (caused by the overhead of cryptographic Argon2id and JWT verification), the C++ backend containers would crash. While Docker successfully restarted the containers, a **cascading database failure** occurred due to "Connection Pool Poisoning." Crashed instances left dirty, uncommitted transactions (`pqxx::work`) hanging, which the newly rebooted instances tried to reuse, resulting in 500 and 409 errors.

To achieve true fault tolerance, the backend architecture was upgraded to include:
1. **Thread-Safe Connection Pooling:** Safely manages concurrent database access across Crow's multi-threaded workers.
2. **Strict RAII Transaction Scoping:** Guarantees automatic PostgreSQL transaction rollbacks if a C++ thread crashes mid-request.
3. **Poison Control (Fail-Fast Validation):** Explicitly catches `pqxx::broken_connection` exceptions and permanently drops dead sockets instead of returning them to the pool.

**Conclusion:** Following these implementations, the system gracefully handles the sudden death and restart of nodes under high load, successfully serving 55,719 requests across three consecutive load tests without a single dropped connection or failed database query.