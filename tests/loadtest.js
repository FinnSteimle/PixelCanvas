import http from "k6/http";
import { check, sleep } from "k6";

export const options = {
  stages: [
    { duration: "10s", target: 50 },
    { duration: "30s", target: 50 },
    { duration: "10s", target: 0 },
  ],
};

const BASE_URL = "http://localhost:8080";

export function setup() {
  const uniqueUser = `loadtest_${Date.now()}`;
  const payload = JSON.stringify({
    username: uniqueUser,
    password: "loadtestpassword",
  });

  const params = {
    headers: { "Content-Type": "application/json" },
  };

  const regRes = http.post(`${BASE_URL}/register`, payload, params);
  if (regRes.status !== 200 && regRes.status !== 201) {
    console.error(`Registration Failed: ${regRes.status} - ${regRes.body}`);
  }

  const loginRes = http.post(`${BASE_URL}/login`, payload, params);
  if (loginRes.status !== 200) {
    console.error(`Login Failed: ${loginRes.status} - ${loginRes.body}`);
  }

  const token = loginRes.json("token");
  if (!token) {
    throw new Error("Setup failed: No token received. Aborting test.");
  }

  return { token: token };
}

export default function (data) {
  const res = http.get(`${BASE_URL}/canvas`, {
    headers: {
      Authorization: `Bearer ${data.token}`,
    },
  });

  check(res, {
    "is status 200": (r) => r.status === 200,
  });

  sleep(0.1);
}
