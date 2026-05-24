const Schedule = require("../models/Schedule");

/**
 * Get all schedules with filters
 */
exports.getAllSchedules = async (req, res) => {
  try {
    const filter = {};
    if (req.query.route) filter.route = req.query.route;
    if (req.query.bus) filter.bus = req.query.bus;
    if (req.query.active !== undefined) filter.isActive = req.query.active === "true";
    if (req.query.day) filter.daysOfOperation = req.query.day;

    const schedules = await Schedule.find(filter)
      .populate("route", "routeNumber name origin destination")
      .populate("bus", "busId name type")
      .populate("driver")
      .sort({ departureTime: 1 });
      
    res.json(schedules);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Get a single schedule profile
 */
exports.getScheduleById = async (req, res) => {
  try {
    const s = await Schedule.findById(req.params.id)
      .populate("route")
      .populate("bus")
      .populate("driver");
      
    if (!s) return res.status(404).json({ error: "Schedule not found" });
    res.json(s);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Create a new schedule (Admin only)
 */
exports.createSchedule = async (req, res) => {
  try {
    const s = new Schedule(req.body);
    await s.save();
    res.status(201).json(s);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Update an existing schedule (Admin only)
 */
exports.updateSchedule = async (req, res) => {
  try {
    const s = await Schedule.findByIdAndUpdate(req.params.id, req.body, { new: true });
    if (!s) return res.status(404).json({ error: "Schedule not found" });
    res.json(s);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Delete a schedule (Admin only)
 */
exports.deleteSchedule = async (req, res) => {
  try {
    const s = await Schedule.findByIdAndDelete(req.params.id);
    if (!s) return res.status(404).json({ error: "Schedule not found" });
    res.json({ success: true, message: "Schedule deleted" });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};
