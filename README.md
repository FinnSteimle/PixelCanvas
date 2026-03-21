# PixelCanvas: Distributed Collaborative Canvas

PixelCanvas is a fault-tolerant, real-time, distributed clone of r/place. It allows multiple users to paint simultaneously on a shared 50x50 grid. The system is designed for high concurrency and fault tolerance.

## Architecture & Scalability

The system uses a low-latency, "read-optimized" architecture designed to handle thousands of concurrent users:

*   **In-Memory Cache:** The backend serves the 50x50 canvas directly from memory, eliminating database scans for the read path.
*   **Write Batching:** Pixel updates are queued and persisted to the PostgreSQL database in batches every 50ms to reduce I/O overhead.
*   **Real-Time Synchronization:** Redis Pub/Sub synchronizes state across multiple backend instances behind a round-robin Nginx load balancer.
*   **Fault Tolerance:** Automatic database connection pooling with a 2-second timeout and health checks ensures resilience during node failures or restarts.

## Performance Benchmarks

*   **Extreme Scalability:** 1,000 concurrent Virtual Users (VUs) with a 99.98% success rate.
*   **Throughput:** 101,449+ pixel updates per second broadcasted via Redis with 1,000 concurrent VUs.
*   **Low Latency:** 2.07ms - 5.12ms average request duration at baseline load (100 VUs).
*   **Efficiency:** 2.2GB of real-time JSON traffic handled under high load (500 VUs).

## Features

*   **Distributed Backends:** Scalable C++ instances behind Nginx.
*   **JWT Security:** Authentication is required for all state-modifying actions.
*   **Persistent Storage:** PostgreSQL stores user accounts and the canvas state.
*   **Docker-Ready:** Deploys with a single command via Docker Compose.

## Technical Stack

*   **Frontend:** HTML5 Canvas, Vanilla JS, CSS
*   **Backend:** C++20 (Crow Microframework)
*   **Database:** PostgreSQL 18
*   **Messaging:** Redis 8.6
*   **Auth:** Argon2 (libsodium), JWT (jwt-cpp)
*   **Orchestration:** Docker, Nginx

## Deployment

1.  **Configure Environment:** `cp .env.example .env`
2.  **Run with Docker Compose:** `docker-compose up --build -d`
3.  **Access App:** `http://localhost:8080`

## Testing

### Load Testing
To execute the load test using **k6**:
```bash
k6 run tests/loadtest.js
```
The test simulates a complete user lifecycle: registration, login, canvas retrieval, and real-time drawing via WebSockets.

### Failover Demo
Stop a backend instance: `docker stop pixelcanvas-backend1-1`. The system will continue to function as Nginx reroutes traffic and clients automatically reconnect.
