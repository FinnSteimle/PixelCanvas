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

// Setup WebSocket
const socket = new WebSocket(`ws://${window.location.host}/ws`);

socket.onopen = () => {
  status.innerText = "● Online";
  status.style.color = "lime";
};

socket.onmessage = (event) => {
  const msg = JSON.parse(event.data);
  // Crow will send: { "x": 0-49, "y": 0-49, "color": "#HEX" }
  drawPixel(msg.x, msg.y, msg.color);
};

canvas.addEventListener("mousedown", (e) => {
  const rect = canvas.getBoundingClientRect();
  const x = Math.floor((e.clientX - rect.left) / CELL_SIZE);
  const y = Math.floor((e.clientY - rect.top) / CELL_SIZE);

  const payload = {
    x: x,
    y: y,
    color: colorPicker.value,
  };

  socket.send(JSON.stringify(payload));
});
