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
        // Store JWT in sessionStorage for subsequent requests
        sessionStorage.setItem("pixel_token", data.token);
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
function connectWebSocket() {
  const token = sessionStorage.getItem("pixel_token");
  if (!token) return;

  // We pass the token in the query string so the backend can verify it in the handshake
  socket = new WebSocket(`ws://${window.location.host}/ws?token=${token}`);

  socket.onopen = () => {
    status.innerText = "● Online";
    status.style.color = "lime";

    // Fetch the full canvas state from the database upon successful connection
    fetch("/canvas", {
      headers: { "Authorization": `Bearer ${token}` }
    })
      .then((res) => res.json())
      .then((data) => {
        // Clear canvas with white background
        ctx.fillStyle = "#FFFFFF";
        ctx.fillRect(0, 0, CANVAS_SIZE, CANVAS_SIZE);
        // Render all pixels from the database
        data.forEach((p) => drawPixel(p.x, p.y, p.color));
      });
  };

  // Handle incoming pixel updates from other users
  socket.onmessage = (event) => {
    const msg = JSON.parse(event.data);
    drawPixel(msg.x, msg.y, msg.color);
  };

  socket.onclose = (e) => {
    status.innerText = "● Offline - Reconnecting...";
    status.style.color = "orange";

    // If the backend closes the connection due to token issues, force a re-login
    if (
      e.reason.includes("Token") ||
      e.reason.includes("Unauthorized") ||
      e.code === 4001
    ) {
      sessionStorage.removeItem("pixel_token");
      authOverlay.style.display = "flex";
    } else {
      // Automatic reconnection attempt after a short delay
      setTimeout(connectWebSocket, 2000);
    }
  };
}

// --- UI EVENT LISTENERS ---

// Trigger auth when buttons are clicked
loginBtn.onclick = () => performAuth("login");
registerBtn.onclick = () => performAuth("register");

// Log out by clearing the token and reloading the page
logoutBtn.onclick = () => {
  sessionStorage.removeItem("pixel_token");
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
// Automatically log in if a valid token exists in sessionStorage
if (sessionStorage.getItem("pixel_token")) {
  authOverlay.style.display = "none";
  connectWebSocket();
}