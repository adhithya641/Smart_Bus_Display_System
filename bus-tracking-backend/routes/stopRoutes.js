const express = require("express");
const router = express.Router();
const stopController = require("../controllers/stopController");
const { authenticate } = require("../middleware/auth");
const requireRole = require("../middleware/roleMiddleware");

/**
 * Stop Routes
 * Handles bus stops and nearby searches
 */

// GET /api/stops - List all stops with optional search
router.get("/", stopController.getAllStops);

// GET /api/stops/nearby - Find stops near coordinate point
router.get("/nearby", stopController.getNearbyStops);

// GET /api/stops/:id - Get stop profile
router.get("/:id", stopController.getStopById);

// POST /api/stops - Create a stop (Admin only)
router.post("/", authenticate, requireRole("admin"), stopController.createStop);

// PUT /api/stops/:id - Update stop profile (Admin only)
router.put("/:id", authenticate, requireRole("admin"), stopController.updateStop);

// DELETE /api/stops/:id - Delete stop profile (Admin only)
router.delete("/:id", authenticate, requireRole("admin"), stopController.deleteStop);

module.exports = router;
