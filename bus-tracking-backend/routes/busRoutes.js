const express = require("express");
const router = express.Router();
const busController = require("../controllers/busController");
const { authenticate, optionalAuth } = require("../middleware/auth");
const requireRole = require("../middleware/roleMiddleware");

/**
 * Bus Routes
 * CRUD operations for bus management
 */

// GET /api/buses - Get all buses with optional filters
router.get("/", optionalAuth, busController.getAllBuses);

// GET /api/buses/live - Get all buses currently on trip with live coordinates
router.get("/live", busController.getLiveBuses);

// GET /api/buses/stats - Get fleet status statistics
router.get("/stats", busController.getBusStats);

// GET /api/buses/:id - Get single bus profile
router.get("/:id", busController.getBusById);

// POST /api/buses - Create new bus (Admin only)
router.post("/", authenticate, requireRole("admin"), busController.createBus);

// PUT /api/buses/:id - Update bus profile (Admin only)
router.put("/:id", authenticate, requireRole("admin"), busController.updateBus);

// DELETE /api/buses/:id - Delete bus profile (Admin only)
router.delete("/:id", authenticate, requireRole("admin"), busController.deleteBus);

module.exports = router;
