import http from "k6/http";
import ws from "k6/ws";
import { check, sleep } from "k6";

/**
 * Load test configuration options.
 * Defines stages for ramping up and sustaining virtual users (VUs).
 */
export const options = {
  stages: [
    { duration: "10s", target: 100 },
    { duration: "30s", target: 100 },
    { duration: "10s", target: 0 },
  ],
};

const BASE_URL = "http://localhost:8080";
const WS_URL = "ws://localhost:8080/ws";

/**
 * Setup function: Executed once before the main test loop.
 * Registers a unique user and logs in to obtain a JWT for use in the test.
 * @returns {object} Data to be passed to each VU execution.
 */
export function setup() {
  const uniqueUser = `loadtest_${Date.now()}`;
  const payload = JSON.stringify({
    username: uniqueUser,
    password: "loadtestpassword",
  });

  const params = {
    headers: { "Content-Type": "application/json" },
  };

  // 1. Register a new user
  const regRes = http.post(`${BASE_URL}/register`, payload, params);

  // 2. Login to get the JWT token
  const loginRes = http.post(`${BASE_URL}/login`, payload, params);
  const token = loginRes.json("token");

  if (!token) {
    throw new Error("Setup failed: No token received. Aborting test.");
  }

  return { token: token };
}

/**
 * Main execution loop for each virtual user.
 * Fetches the canvas and opens a WebSocket connection to simulate pixel activity.
 * @param {object} data The setup data (contains the token).
 */
export default function (data) {
  // Perform a GET request to the protected /canvas endpoint
  const res = http.get(`${BASE_URL}/canvas`, {
    headers: {
      Authorization: `Bearer ${data.token}`,
    },
  });

  check(res, {
    "status is 200": (r) => r.status === 200,
  });

  // Simulate WebSocket pixel activity
  const url = `${WS_URL}?token=${data.token}`;

  ws.connect(url, {}, function (socket) {
    socket.on("open", function () {
      // Simulate a user "drawing" a pixel
      socket.send(JSON.stringify({
        x: Math.floor(Math.random() * 50),
        y: Math.floor(Math.random() * 50),
        color: "#FF0000"
      }));

      // Close after a brief period of listening for updates
      socket.setTimeout(function () {
        socket.close();
      }, 500);
    });
  });

  sleep(0.1);
}
