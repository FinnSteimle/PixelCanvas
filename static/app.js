const canvas = document.getElementById("canvas");
const ctx = canvas.getContext("2d");
const colorPicker = document.getElementById("colorPicker");
const status = document.getElementById("status");

const PIXEL_COUNT = 50;
const CANVAS_SIZE = 500;
const CELL_SIZE = CANVAS_SIZE / PIXEL_COUNT; // Each logical pixel is 10x10 real pixels

// Draw a pixel on the canvas
function drawPixel(x, y, color) {
  ctx.fillStyle = color;
  ctx.fillRect(x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE);
}

let socket; // Declare globally so the click listener can use it

function connectWebSocket() {
  socket = new WebSocket(`ws://${window.location.host}/ws`);

  socket.onopen = () => {
    status.innerText = "● Online";
    status.style.color = "lime";
    console.log("WebSocket Connected!");

    // Every time we connect (or reconnect), fetch the latest state from DB
    fetch("/canvas")
      .then((response) => response.json())
      .then((data) => {
        // Wipe everything
        ctx.clearRect(0, 0, CANVAS_SIZE, CANVAS_SIZE);

        // Fill the entire background with white
        ctx.fillStyle = "#FFFFFF";
        ctx.fillRect(0, 0, CANVAS_SIZE, CANVAS_SIZE);
        data.forEach((pixel) => {
          drawPixel(pixel.x, pixel.y, pixel.color);
        });
        console.log("Canvas synced after connection.");
      })
      .catch((err) => console.error("Could not sync canvas:", err));
  };

  socket.onmessage = (event) => {
    const msg = JSON.parse(event.data);
    drawPixel(msg.x, msg.y, msg.color);
  };

  socket.onclose = () => {
    status.innerText = "● Offline - Reconnecting...";
    status.style.color = "orange";
    console.warn("WebSocket dropped. Reconnecting in 2 seconds...");
    // Auto-reconnect loop
    setTimeout(connectWebSocket, 2000);
  };

  socket.onerror = (err) => {
    console.error("WebSocket Error:", err);
    socket.close(); // Force the close event to trigger the reconnect
  };
}

// Start the connection immediately
connectWebSocket();

// Handle user clicks
canvas.addEventListener("mousedown", (e) => {
  // Safety check: Only send if the socket is actually connected
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
  } else {
    console.warn("Wait for reconnection before drawing.");
  }
});
