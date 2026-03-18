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
* **Backend:** Modern C++ (C++17) using the Crow microframework
* **Database:** PostgreSQL 15 (via `libpqxx`)
* **Message Broker:** Redis 7 (via `redis-plus-plus`)
* **Infrastructure:** Docker, Docker Compose, Nginx (Reverse Proxy & Load Balancer)

## Prerequisites

* Docker and Docker Compose installed.
* Port `8080` available on your local machine.

## Quick Start

1. **Configure Secrets:**
   Copy the example environment file and set your secure credentials.
   ```bash
   cp .env.example .env
   ```

2. **Launch the System:**
   Build and start the entire cluster in detached mode.
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

### 3. Load Testing & Bottleneck Analysis
To evaluate the system's performance and identify physical limits, a load test is executed against the JWT-protected `GET /canvas` endpoint using **k6**. 

**Execution:**
Ensure the cluster is running, then execute the test script from the project root:
```bash
k6 run tests/loadtest.js
```

**Test Profile & Results:**
* **Load:** Ramped up to 50 concurrent Virtual Users (VUs) sustained over 50 seconds.
* **Peak Throughput:** ~371 requests/second.
* **Peak P(95) Latency:** 8.51ms to 11.29ms for successful requests.
* **Total Requests:** Up to 19,073 requests per run, achieving an 82.65% to 99.89% success rate under maximum stress.

**Bottleneck Identification: Graceful Degradation & Thread Exhaustion**
Real-time monitoring via `docker stats` and Nginx logs revealed that the system bottleneck is **CPU starvation** on the C++ backend nodes, caused by the computational overhead of cryptographic JWT verification (`SHA-256`).

Under sustained extreme load, thread exhaustion occurs as the backend processes thousands of cryptographic validations. However, the architecture is designed to **degrade gracefully**. Instead of crashing the C++ containers or triggering a cascading database lockup, the system processes the vast majority of requests flawlessly while safely dropping the excess requests that exceed thread capacity (returning timeouts). Container telemetry confirms `"OOMKilled": false`, proving the limitation is strictly CPU/Thread bound, not a memory leak.