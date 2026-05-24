const express = require("express");
const router = express.Router();
const alertController = require("../controllers/alertController");
const { authenticate } = require("../middleware/auth");
const requireRole = require("../middleware/roleMiddleware");

/**
 * Alert Routes
 * Handles system-wide alerts and alerts displays
 */

// GET /api/alerts - List all alerts
router.get("/", alertController.getAllAlerts);

// GET /api/alerts/active - List only active alerts (for display widgets)
router.get("/active", alertController.getActiveAlerts);

// POST /api/alerts - Create alert (Admin only)
router.post("/", authenticate, requireRole("admin"), alertController.createAlert);

// PUT /api/alerts/:id - Update alert (Admin only)
router.put("/:id", authenticate, requireRole("admin"), alertController.updateAlert);

// DELETE /api/alerts/:id - Dismiss alert (Admin only)
router.delete("/:id", authenticate, requireRole("admin"), alertController.deleteAlert);

module.exports = router;
