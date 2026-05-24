const express = require("express");
const router = express.Router();
const routeController = require("../controllers/routeController");
const { authenticate } = require("../middleware/auth");
const requireRole = require("../middleware/roleMiddleware");

/**
 * Route Routes
 * Manages bus routes and stop paths
 */

// GET /api/routes - List all routes with optional filters
router.get("/", routeController.getAllRoutes);

// GET /api/routes/search - Search routes with active bus overlays
router.get("/search", routeController.searchRoutes);

// GET /api/routes/:id - Get detailed route profile
router.get("/:id", routeController.getRouteById);

// GET /api/routes/:id/buses - Get active buses on a route
router.get("/:id/buses", routeController.getBusesOnRoute);

// POST /api/routes - Create new route (Admin only)
router.post("/", authenticate, requireRole("admin"), routeController.createRoute);

// PUT /api/routes/:id - Update route (Admin only)
router.put("/:id", authenticate, requireRole("admin"), routeController.updateRoute);

// DELETE /api/routes/:id - Delete route (Admin only)
router.delete("/:id", authenticate, requireRole("admin"), routeController.deleteRoute);

module.exports = router;
