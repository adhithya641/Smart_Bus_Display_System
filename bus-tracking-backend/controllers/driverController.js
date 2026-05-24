const Driver = require("../models/Driver");
const Bus = require("../models/Bus");
const Alert = require("../models/Alert");

/**
 * Get all drivers (Admin only)
 */
exports.getAllDrivers = async (req, res) => {
  try {
    const drivers = await Driver.find()
      .populate("user", "name email phone")
      .populate("assignedBus", "busId name routeName");
    res.json(drivers);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Create a driver profile (Admin only)
 */
exports.createDriver = async (req, res) => {
  try {
    const driver = new Driver(req.body);
    await driver.save();
    res.status(201).json(driver);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Update a driver profile (Admin only)
 */
exports.updateDriver = async (req, res) => {
  try {
    const driver = await Driver.findByIdAndUpdate(req.params.id, req.body, { new: true })
      .populate("user", "name email phone")
      .populate("assignedBus");
    if (!driver) return res.status(404).json({ error: "Driver not found" });
    res.json(driver);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Remove driver profile (Admin only)
 */
exports.deleteDriver = async (req, res) => {
  try {
    const driver = await Driver.findByIdAndDelete(req.params.id);
    if (!driver) return res.status(404).json({ error: "Driver not found" });
    res.json({ success: true, message: "Driver removed" });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Start a tracking session (Driver only)
 * Sets status to online and creates a unique trackingSessionId to prevent multiple trackings
 */
exports.startTrip = async (req, res) => {
  try {
    const driver = await Driver.findOne({ user: req.userId });
    if (!driver) return res.status(404).json({ error: "Driver profile not found." });
    if (!driver.assignedBus) return res.status(400).json({ error: "No bus assigned." });

    // Generate a secure, unique tracking session ID
    const trackingSessionId = `${Date.now()}_${Math.random().toString(36).substring(2, 9)}`;

    driver.trackingStatus = "online";
    driver.activeTrackingSessionId = trackingSessionId;
    driver.currentTrip = {
      isOnTrip: true,
      startTime: new Date(),
      routeId: req.body.routeId || null,
      status: "active",
    };
    await driver.save();

    await Bus.findByIdAndUpdate(driver.assignedBus, { isOnTrip: true });

    res.json({
      success: true,
      trackingSessionId,
      message: "Trip started.",
    });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Stop a tracking session (Driver only)
 * Sets status to offline and clears active session ID
 */
exports.stopTrip = async (req, res) => {
  try {
    const driver = await Driver.findOne({ user: req.userId });
    if (!driver) return res.status(404).json({ error: "Driver profile not found." });

    driver.trackingStatus = "offline";
    driver.activeTrackingSessionId = null;
    driver.currentTrip = {
      isOnTrip: false,
      startTime: null,
      routeId: null,
      status: "completed",
    };
    driver.totalTrips += 1;
    await driver.save();

    if (driver.assignedBus) {
      await Bus.findByIdAndUpdate(driver.assignedBus, { isOnTrip: false, currentSpeed: 0 });
    }

    res.json({
      success: true,
      message: "Trip ended.",
    });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Send an emergency SOS (Driver only)
 */
exports.sendEmergency = async (req, res) => {
  try {
    const driver = await Driver.findOne({ user: req.userId }).populate("assignedBus");
    const alert = new Alert({
      type: "emergency",
      title: `Emergency: ${driver?.assignedBus?.name || "Unknown Bus"}`,
      message: req.body.message || "Driver has triggered an emergency alert.",
      severity: "critical",
      affectedBuses: driver?.assignedBus ? [driver.assignedBus._id] : [],
      createdBy: req.userId,
      expiresAt: new Date(Date.now() + 2 * 60 * 60 * 1000), // 2 hours
    });
    await alert.save();
    res.json({ success: true, alertId: alert._id });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};
