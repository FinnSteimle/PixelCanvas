import http from "k6/http";
import { check, sleep } from "k6";

/**
 * Load test configuration options.
 * Defines stages for ramping up and sustaining virtual users (VUs).
 */
export const options = {
  stages: [
    { duration: "10s", target: 50 }, // Ramp up to 50 VUs over 10 seconds
    { duration: "30s", target: 50 }, // Sustain 50 VUs for 30 seconds
    { duration: "10s", target: 0 },  // Ramp down to 0 VUs over 10 seconds
  ],
};

const BASE_URL = "http://localhost:8080";

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
  if (regRes.status !== 200 && regRes.status !== 201) {
    console.error(`Registration Failed: ${regRes.status} - ${regRes.body}`);
  }

  // 2. Login to get the JWT token
  const loginRes = http.post(`${BASE_URL}/login`, payload, params);
  if (loginRes.status !== 200) {
    console.error(`Login Failed: ${loginRes.status} - ${loginRes.body}`);
  }

  const token = loginRes.json("token");
  if (!token) {
    throw new Error("Setup failed: No token received. Aborting test.");
  }

  // Pass the token to all VUs
  return { token: token };
}

/**
 * Main execution loop for each virtual user.
 * Repeatedly fetches the current canvas state using the JWT.
 * @param {object} data The setup data (contains the token).
 */
export default function (data) {
  // Perform a GET request to the protected /canvas endpoint
  const res = http.get(`${BASE_URL}/canvas`, {
    headers: {
      Authorization: `Bearer ${data.token}`,
    },
  });

  // Verify that the response status is 200 OK
  check(res, {
    "is status 200": (r) => r.status === 200,
  });

  // Short pause between requests to simulate user behavior
  sleep(0.1);
}
