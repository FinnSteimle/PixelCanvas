// --- CONFIGURATION ---
const PIXEL_COUNT = 50;
const CANVAS_SIZE = 500;
const CELL_SIZE = CANVAS_SIZE / PIXEL_COUNT;

// --- DOM ELEMENTS ---
const canvas = document.getElementById("canvas");
const ctx = canvas.getContext("2d");
const colorPicker = document.getElementById("colorPicker");
const status = document.getElementById("status");
const authOverlay = document.getElementById("auth-overlay");
const authStatus = document.getElementById("auth-status");
const loginBtn = document.getElementById("loginBtn");
const registerBtn = document.getElementById("registerBtn");
const logoutBtn = document.getElementById("logoutBtn");

let socket;

// --- CANVAS HELPERS ---
function drawPixel(x, y, color) {
  ctx.fillStyle = color;
  ctx.fillRect(x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE);
}

// --- AUTHENTICATION LOGIC ---
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
        localStorage.setItem("pixel_token", data.token);
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

// --- WEBSOCKET LOGIC ---
function connectWebSocket() {
  const token = localStorage.getItem("pixel_token");
  if (!token) return;

  // We pass the token in the query string so the backend can verify it in .onopen
  socket = new WebSocket(`ws://${window.location.host}/ws?token=${token}`);

  socket.onopen = () => {
    status.innerText = "● Online";
    status.style.color = "lime";

    // Fetch full canvas state from the DB upon connection
    fetch("/canvas", {
  headers: { "Authorization": `Bearer ${token}` }})
      .then((res) => res.json())
      .then((data) => {
        ctx.fillStyle = "#FFFFFF";
        ctx.fillRect(0, 0, CANVAS_SIZE, CANVAS_SIZE);
        data.forEach((p) => drawPixel(p.x, p.y, p.color));
      });
  };

  socket.onmessage = (event) => {
    const msg = JSON.parse(event.data);
    drawPixel(msg.x, msg.y, msg.color);
  };

  socket.onclose = (e) => {
    status.innerText = "● Offline - Reconnecting...";
    status.style.color = "orange";

    // If the backend closes the connection due to token issues, force re-login
    if (
      e.reason.includes("Token") ||
      e.reason.includes("Unauthorized") ||
      e.code === 4001
    ) {
      localStorage.removeItem("pixel_token");
      authOverlay.style.display = "flex";
    } else {
      setTimeout(connectWebSocket, 2000);
    }
  };
}

// --- EVENT LISTENERS ---
loginBtn.onclick = () => performAuth("login");
registerBtn.onclick = () => performAuth("register");

logoutBtn.onclick = () => {
  localStorage.removeItem("pixel_token");
  location.reload();
};

canvas.addEventListener("mousedown", (e) => {
  if (socket && socket.readyState === WebSocket.OPEN) {
    const rect = canvas.getBoundingClientRect();
    const x = Math.floor((e.clientX - rect.left) / CELL_SIZE);
    const y = Math.floor((e.clientY - rect.top) / CELL_SIZE);

    const payload = {
      x: x,
      y: y,
      color: colorPicker.value,
    };
    socket.send(JSON.stringify(payload));
  }
});

// --- INITIALIZATION ---
// Check if the user is already logged in when the page loads
if (localStorage.getItem("pixel_token")) {
  authOverlay.style.display = "none";
  connectWebSocket();
}
