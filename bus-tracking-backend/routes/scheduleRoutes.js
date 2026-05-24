const express = require("express");
const router = express.Router();
const scheduleController = require("../controllers/scheduleController");
const { authenticate } = require("../middleware/auth");
const requireRole = require("../middleware/roleMiddleware");

/**
 * Schedule Routes
 * Handles timing schedule allocations
 */

// GET /api/schedules - Get all schedules with operational filters
router.get("/", scheduleController.getAllSchedules);

// GET /api/schedules/:id - Get a schedule
router.get("/:id", scheduleController.getScheduleById);

// POST /api/schedules - Create new schedule (Admin only)
router.post("/", authenticate, requireRole("admin"), scheduleController.createSchedule);

// PUT /api/schedules/:id - Update schedule details (Admin only)
router.put("/:id", authenticate, requireRole("admin"), scheduleController.updateSchedule);

// DELETE /api/schedules/:id - Remove schedule (Admin only)
router.delete("/:id", authenticate, requireRole("admin"), scheduleController.deleteSchedule);

module.exports = router;
