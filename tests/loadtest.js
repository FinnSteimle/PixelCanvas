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

  http.post(`${BASE_URL}/register`, payload, params);

  const loginRes = http.post(`${BASE_URL}/login`, payload, params);

  return { token: loginRes.json("token") };
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
