const express = require("express");
const router = express.Router();
const Bus = require("../models/Bus");
const Route = require("../models/Route");
const Driver = require("../models/Driver");
const Schedule = require("../models/Schedule");
const Alert = require("../models/Alert");
const AdminLog = require("../models/AdminLog");
const { authenticate, requireRole } = require("../middleware/authMiddleware");

/**
 * Analytics Routes — Dashboard statistics and reporting
 */

// GET /api/analytics/overview — Overall system stats
router.get("/overview", authenticate, requireRole("admin"), async (req, res) => {
  try {
    const [totalBuses, activeBuses, onTripBuses, totalRoutes, totalDrivers,
           activeDrivers, activeAlerts, totalSchedules] = await Promise.all([
      Bus.countDocuments(),
      Bus.countDocuments({ status: "active" }),
      Bus.countDocuments({ isOnTrip: true }),
      Route.countDocuments({ isActive: true }),
      Driver.countDocuments(),
      Driver.countDocuments({ "currentTrip.isOnTrip": true }),
      Alert.countDocuments({ isActive: true }),
      Schedule.countDocuments({ isActive: true }),
    ]);

    res.json({
      buses: { total: totalBuses, active: activeBuses, onTrip: onTripBuses },
      routes: { total: totalRoutes },
      drivers: { total: totalDrivers, active: activeDrivers },
      alerts: { active: activeAlerts },
      schedules: { total: totalSchedules },
    });
  } catch (err) { res.status(500).json({ error: err.message }); }
});

// GET /api/analytics/logs — Admin activity logs
router.get("/logs", authenticate, requireRole("admin"), async (req, res) => {
  try {
    const logs = await AdminLog.find()
      .populate("admin", "name email")
      .sort({ createdAt: -1 })
      .limit(parseInt(req.query.limit) || 50);
    res.json(logs);
  } catch (err) { res.status(500).json({ error: err.message }); }
});

// GET /api/analytics/buses-by-district — Bus count per district
router.get("/buses-by-district", authenticate, requireRole("admin"), async (req, res) => {
  try {
    const result = await Bus.aggregate([
      { $match: { status: "active" } },
      { $group: { _id: "$district", count: { $sum: 1 } } },
      { $sort: { count: -1 } },
    ]);
    res.json(result);
  } catch (err) { res.status(500).json({ error: err.message }); }
});

module.exports = router;
