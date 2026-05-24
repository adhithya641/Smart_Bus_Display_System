const express = require("express");
const router = express.Router();
const driverController = require("../controllers/driverController");
const { authenticate } = require("../middleware/auth");
const requireRole = require("../middleware/roleMiddleware");

/**
 * Driver Routes
 * Handles driver management and trip start/stop cycles
 */

// GET /api/drivers - List all drivers (Admin only)
router.get("/", authenticate, requireRole("admin"), driverController.getAllDrivers);

// POST /api/drivers - Create driver profile (Admin only)
router.post("/", authenticate, requireRole("admin"), driverController.createDriver);

// PUT /api/drivers/:id - Update driver profile (Admin only)
router.put("/:id", authenticate, requireRole("admin"), driverController.updateDriver);

// DELETE /api/drivers/:id - Remove driver profile (Admin only)
router.delete("/:id", authenticate, requireRole("admin"), driverController.deleteDriver);

// POST /api/drivers/start-trip - Driver starts sharing live location
router.post("/start-trip", authenticate, requireRole("driver"), driverController.startTrip);

// POST /api/drivers/stop-trip - Driver stops location sharing
router.post("/stop-trip", authenticate, requireRole("driver"), driverController.stopTrip);

// POST /api/drivers/emergency - Driver triggers emergency broadcast
router.post("/emergency", authenticate, requireRole("driver"), driverController.sendEmergency);

module.exports = router;
