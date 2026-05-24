require("dotenv").config();
const express = require("express");
const cors = require("cors");
const http = require("http");
const socketIo = require("socket.io");
const connectDB = require("./config/db");
const { initSocket } = require("./socket/socketHandler");

const app = express();
const server = http.createServer(app);
const io = socketIo(server, { cors: { origin: "*" } });

// Connect database
connectDB();

// Initialize socket handler
initSocket(io);

// Middleware
app.use(cors({
  origin: "*",
  credentials: true
}));
app.use(express.json());

// Request logging
app.use((req, res, next) => {
  if (req.method !== "GET") {
    console.log(`📨 ${req.method} ${req.path}`);
  }
  next();
});

// ===== Mount Routes =====
// Auth
app.use("/api/auth", require("./routes/authRoutes"));

// Core resources
app.use("/api/buses", require("./routes/busRoutes"));
app.use("/api/routes", require("./routes/routeRoutes"));
app.use("/api/stops", require("./routes/stopRoutes"));
app.use("/api/schedules", require("./routes/scheduleRoutes"));
app.use("/api/drivers", require("./routes/driverRoutes"));
app.use("/api/alerts", require("./routes/alertRoutes"));
app.use("/api/analytics", require("./routes/analyticsRoutes"));

// GPS routes (no longer need to pass io because of modular socket handler!)
app.use("/api/gps", require("./routes/gpsRoutes"));

// ESP32 display device routes
app.use("/api/esp32", require("./routes/esp32Routes")(io));

// Legacy route (backward compatibility)
const legacyApi = require("./routes/api")(io);
app.use("/api", legacyApi);

// Health check
app.get("/api/health", (req, res) => {
  res.json({ status: "ok", timestamp: new Date().toISOString() });
});

// Root route
app.get("/", (req, res) => {
  res.send("Smart Bus Tracking Backend Running Successfully");
});

// Global error handler
app.use((err, req, res, next) => {
  console.error("❌ Server error:", err.message);
  res.status(500).json({ error: "Internal server error" });
});

// Start server
const PORT = process.env.PORT || 5000;
server.listen(PORT, () => console.log(`🚀 Server running on port ${PORT}`));
