const Bus = require("../models/Bus");

/**
 * Get all buses with optional filters
 */
exports.getAllBuses = async (req, res) => {
  try {
    const filter = {};
    if (req.query.district) filter.district = req.query.district;
    if (req.query.status) filter.status = req.query.status;
    if (req.query.type) filter.type = req.query.type;
    if (req.query.onTrip) filter.isOnTrip = req.query.onTrip === "true";

    const buses = await Bus.find(filter)
      .populate("assignedDriver")
      .populate("assignedRoute")
      .sort({ busId: 1 });

    res.json(buses);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Get all live buses currently on a trip
 */
exports.getLiveBuses = async (req, res) => {
  try {
    const buses = await Bus.find({
      isOnTrip: true,
      latitude: { $ne: null },
      longitude: { $ne: null },
    }).select("busId name routeName district latitude longitude currentSpeed heading lastUpdated isOnTrip type");

    res.json(buses);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Get statistics of the bus fleet
 */
exports.getBusStats = async (req, res) => {
  try {
    const [total, active, onTrip, maintenance] = await Promise.all([
      Bus.countDocuments(),
      Bus.countDocuments({ status: "active" }),
      Bus.countDocuments({ isOnTrip: true }),
      Bus.countDocuments({ status: "maintenance" }),
    ]);

    res.json({ total, active, onTrip, maintenance });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Get single bus details
 */
exports.getBusById = async (req, res) => {
  try {
    const bus = await Bus.findById(req.params.id)
      .populate("assignedDriver")
      .populate("assignedRoute");

    if (!bus) return res.status(404).json({ error: "Bus not found" });
    res.json(bus);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Create a new bus profile (Admin)
 */
exports.createBus = async (req, res) => {
  try {
    const bus = new Bus(req.body);
    await bus.save();
    res.status(201).json(bus);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Update an existing bus profile (Admin)
 */
exports.updateBus = async (req, res) => {
  try {
    const bus = await Bus.findByIdAndUpdate(req.params.id, req.body, {
      new: true,
      runValidators: true,
    });
    if (!bus) return res.status(404).json({ error: "Bus not found" });
    res.json(bus);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Delete a bus profile (Admin)
 */
exports.deleteBus = async (req, res) => {
  try {
    const bus = await Bus.findByIdAndDelete(req.params.id);
    if (!bus) return res.status(404).json({ error: "Bus not found" });
    res.json({ success: true, message: "Bus deleted" });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};
