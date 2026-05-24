const Alert = require("../models/Alert");

/**
 * Get all alerts with filters
 */
exports.getAllAlerts = async (req, res) => {
  try {
    const filter = {};
    if (req.query.active !== undefined) filter.isActive = req.query.active === "true";
    if (req.query.severity) filter.severity = req.query.severity;
    if (req.query.type) filter.type = req.query.type;

    const alerts = await Alert.find(filter)
      .populate("createdBy", "name")
      .sort({ createdAt: -1 })
      .limit(50);
    res.json(alerts);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Get active alerts (prioritized by severity)
 */
exports.getActiveAlerts = async (req, res) => {
  try {
    const alerts = await Alert.find({ isActive: true })
      .sort({ severity: -1, createdAt: -1 })
      .limit(20);
    res.json(alerts);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Create a new alert (Admin only)
 */
exports.createAlert = async (req, res) => {
  try {
    const alert = new Alert({ ...req.body, createdBy: req.userId });
    await alert.save();
    res.status(201).json(alert);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Update an existing alert (Admin only)
 */
exports.updateAlert = async (req, res) => {
  try {
    const alert = await Alert.findByIdAndUpdate(req.params.id, req.body, { new: true });
    if (!alert) return res.status(404).json({ error: "Alert not found" });
    res.json(alert);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Delete an alert (Admin only)
 */
exports.deleteAlert = async (req, res) => {
  try {
    const alert = await Alert.findByIdAndDelete(req.params.id);
    if (!alert) return res.status(404).json({ error: "Alert not found" });
    res.json({ success: true, message: "Alert deleted" });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};
