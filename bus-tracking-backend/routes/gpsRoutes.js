const express = require("express");
const router = express.Router();
const gpsController = require("../controllers/gpsController");
const { authenticate } = require("../middleware/auth");
const requireRole = require("../middleware/roleMiddleware");

/**
 * GPS Routes
 * Real-time GPS stream inputs and public status outputs
 */

// POST /api/gps/update - Receive live GPS stream (Driver & Admin allowed)
router.post("/update", authenticate, requireRole("driver", "admin"), gpsController.updateGPS);

// GET /api/gps/bus/:busId - Get latest coordinate position for a specific bus
router.get("/bus/:busId", gpsController.getLatestBusPosition);

// GET /api/gps/bus/:busId/history - Get historical coordinate breadcrumbs for a specific bus
router.get("/bus/:busId/history", gpsController.getBusHistory);

// GET /api/gps/esp32/:stopId - Upcoming buses for ESP32 hardware displays
router.get("/esp32/:stopId", gpsController.getESP32StopDisplay);

// GET /api/gps/eta - Get dynamic ETA between a specific bus and stop
router.get("/eta", gpsController.getETA);

module.exports = router;
