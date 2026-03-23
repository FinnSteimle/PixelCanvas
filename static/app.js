// --- CONFIGURATION AND CONSTANTS ---
// Grid is 50x50, displayed on a 500x500 canvas
const PIXEL_COUNT = 50;
const CANVAS_SIZE = 500;
const CELL_SIZE = CANVAS_SIZE / PIXEL_COUNT;

// --- DOM ELEMENT REFERENCES ---
const canvas = document.getElementById("canvas");
const ctx = canvas.getContext("2d");
const colorPicker = document.getElementById("colorPicker");
const status = document.getElementById("status");
const instanceIdDisplay = document.getElementById("instance-id");
const authOverlay = document.getElementById("auth-overlay");
const authStatus = document.getElementById("auth-status");
const loginBtn = document.getElementById("loginBtn");
const registerBtn = document.getElementById("registerBtn");
const logoutBtn = document.getElementById("logoutBtn");

let socket; // Global WebSocket instance

// --- CANVAS RENDERING HELPERS ---
/**
 * Draws a single pixel on the canvas grid.
 * @param {number} x Grid X-coordinate (0-49)
 * @param {number} y Grid Y-coordinate (0-49)
 * @param {string} color Hex color string
 */
function drawPixel(x, y, color) {
  ctx.fillStyle = color;
  ctx.fillRect(x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE);
}

// --- TOKEN MANAGEMENT ---

function getAccessToken() {
  return sessionStorage.getItem("pixel_access_token");
}

function getRefreshToken() {
  return sessionStorage.getItem("pixel_refresh_token");
}

function storeTokens(accessToken, refreshToken) {
  sessionStorage.setItem("pixel_access_token", accessToken);
  if (refreshToken) {
    sessionStorage.setItem("pixel_refresh_token", refreshToken);
  }
}

function clearTokens() {
  sessionStorage.removeItem("pixel_access_token");
  sessionStorage.removeItem("pixel_refresh_token");
}

/**
 * Attempts to refresh the access token using the stored refresh token.
 * @returns {string|null} New access token, or null if refresh failed.
 */
async function refreshAccessToken() {
  const refreshToken = getRefreshToken();
  if (!refreshToken) return null;

  try {
    const res = await fetch("/refresh", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ refresh_token: refreshToken }),
    });

    if (!res.ok) return null;

    const data = await res.json();
    storeTokens(data.access_token);
    return data.access_token;
  } catch {
    return null;
  }
}

/**
 * Fetch wrapper that automatically retries with a refreshed access token on 401.
 * @param {string} url The URL to fetch.
 * @param {object} options Fetch options (headers will have Authorization injected).
 * @returns {Response} The fetch response.
 */
async function authFetch(url, options = {}) {
  const token = getAccessToken();
  options.headers = { ...options.headers, Authorization: `Bearer ${token}` };

  let res = await fetch(url, options);

  if (res.status === 401) {
    const newToken = await refreshAccessToken();
    if (newToken) {
      options.headers.Authorization = `Bearer ${newToken}`;
      res = await fetch(url, options);
    }
  }

  return res;
}

// --- AUTHENTICATION LOGIC ---
/**
 * Handles user login or registration requests via REST API.
 * @param {string} endpoint Either 'login' or 'register'
 */
async function performAuth(endpoint) {
  const user = document.getElementById("username").value;
  const pass = document.getElementById("password").value;

  if (!user || !pass) {
    authStatus.innerText = "Please fill all fields.";
    return;
  }

  authStatus.style.color = "white";
  authStatus.innerText =
    endpoint === "login" ? "Logging in..." : "Registering...";

  try {
    const response = await fetch(`/${endpoint}`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ username: user, password: pass }),
    });

    if (response.ok) {
      if (endpoint === "login") {
        const data = await response.json();
        // Store both tokens in sessionStorage
        storeTokens(data.access_token, data.refresh_token);
        // Hide overlay and start real-time sync
        authOverlay.style.display = "none";
        connectWebSocket();
      } else {
        authStatus.style.color = "lime";
        authStatus.innerText = "Registered! Now please login.";
      }
    } else {
      authStatus.style.color = "#ff5555";
      authStatus.innerText =
        endpoint === "login" ? "Invalid login" : "User already exists";
    }
  } catch (err) {
    authStatus.innerText = "Server connection failed.";
  }
}

// --- WEBSOCKET REAL-TIME LOGIC ---
/**
 * Establishes a WebSocket connection for real-time pixel updates.
 * Verifies the JWT token and fetches the initial canvas state.
 */
async function connectWebSocket() {
  let token = getAccessToken();
  if (!token) return;

  // We pass the token in the query string so the backend can verify it in the handshake
  socket = new WebSocket(`ws://${window.location.host}/ws?token=${token}`);

  socket.onopen = () => {
    status.innerText = "● Online";
    status.style.color = "lime";

    // Fetch the full canvas state from the database upon successful connection
    authFetch("/canvas")
      .then((res) => {
        if (!res.ok) throw new Error("Canvas fetch failed");
        return res.json();
      })
      .then((data) => {
        // Clear canvas with white background
        ctx.fillStyle = "#FFFFFF";
        ctx.fillRect(0, 0, CANVAS_SIZE, CANVAS_SIZE);
        // Render all pixels from the database
        data.forEach((p) => drawPixel(p.x, p.y, p.color));
      })
      .catch(() => {});
  };

  // Handle incoming messages (pixel updates or backend identification)
  socket.onmessage = (event) => {
    const msg = JSON.parse(event.data);

    // Check if this is an identification message from the backend
    if (msg.instanceId) {
      instanceIdDisplay.innerText = msg.instanceId;
      return;
    }

    // Otherwise, treat as a pixel update
    drawPixel(msg.x, msg.y, msg.color);
  };

  socket.onclose = async (e) => {
    status.innerText = "● Offline - Reconnecting...";
    status.style.color = "orange";
    instanceIdDisplay.innerText = "---";

    // Always try to refresh the access token before reconnecting,
    // since the most common disconnect cause is token expiry.
    const newToken = await refreshAccessToken();
    if (newToken) {
      setTimeout(connectWebSocket, 500);
    } else if (getRefreshToken()) {
      // Refresh token also expired — force re-login
      clearTokens();
      authOverlay.style.display = "flex";
    } else {
      clearTokens();
      authOverlay.style.display = "flex";
    }
  };
}

// --- UI EVENT LISTENERS ---

// Trigger auth when buttons are clicked
loginBtn.onclick = () => performAuth("login");
registerBtn.onclick = () => performAuth("register");

// Log out by clearing the tokens and reloading the page
logoutBtn.onclick = () => {
  clearTokens();
  location.reload();
};

// Handle pixel painting when clicking on the canvas
canvas.addEventListener("mousedown", (e) => {
  if (socket && socket.readyState === WebSocket.OPEN) {
    const rect = canvas.getBoundingClientRect();
    // Calculate grid coordinates based on click position
    const x = Math.floor((e.clientX - rect.left) / CELL_SIZE);
    const y = Math.floor((e.clientY - rect.top) / CELL_SIZE);

    const payload = {
      x: x,
      y: y,
      color: colorPicker.value,
    };
    // Send the pixel update to the backend
    socket.send(JSON.stringify(payload));
  }
});

// --- INITIALIZATION ---
// Automatically connect if valid tokens exist in sessionStorage
if (getAccessToken()) {
  authOverlay.style.display = "none";
  connectWebSocket();
}
