const Stop = require("../models/Stop");

/**
 * Get all stops with optional filters
 */
exports.getAllStops = async (req, res) => {
  try {
    const filter = {};
    if (req.query.district) filter.district = req.query.district;
    if (req.query.search) {
      filter.name = { $regex: req.query.search, $options: "i" };
    }

    const stops = await Stop.find(filter)
      .populate("connectedRoutes", "routeNumber name origin destination")
      .sort({ name: 1 })
      .limit(parseInt(req.query.limit) || 100);

    res.json(stops);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Find stops near a given GPS location
 */
exports.getNearbyStops = async (req, res) => {
  try {
    const { lat, lng, radius = 1000 } = req.query; // radius in meters

    if (!lat || !lng) {
      return res.status(400).json({ error: "lat and lng query parameters are required." });
    }

    const stops = await Stop.find({
      location: {
        $near: {
          $geometry: {
            type: "Point",
            coordinates: [parseFloat(lng), parseFloat(lat)],
          },
          $maxDistance: parseInt(radius),
        },
      },
      isActive: true,
    })
      .populate("connectedRoutes", "routeNumber name origin destination")
      .limit(20);

    res.json(stops);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Get a single stop profile
 */
exports.getStopById = async (req, res) => {
  try {
    const stop = await Stop.findById(req.params.id)
      .populate("connectedRoutes");

    if (!stop) return res.status(404).json({ error: "Stop not found" });
    res.json(stop);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Create a new stop (Admin only)
 */
exports.createStop = async (req, res) => {
  try {
    const { name, code, lat, lng, district, address } = req.body;

    const stop = new Stop({
      name,
      code,
      location: {
        type: "Point",
        coordinates: [parseFloat(lng), parseFloat(lat)], // GeoJSON coordinates are [longitude, latitude]
      },
      district,
      address,
    });

    await stop.save();
    res.status(201).json(stop);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Update an existing stop (Admin only)
 */
exports.updateStop = async (req, res) => {
  try {
    const updates = { ...req.body };

    // If lat/lng provided, format GeoJSON location correctly
    if (req.body.lat !== undefined && req.body.lng !== undefined) {
      updates.location = {
        type: "Point",
        coordinates: [parseFloat(req.body.lng), parseFloat(req.body.lat)],
      };
      delete updates.lat;
      delete updates.lng;
    }

    const stop = await Stop.findByIdAndUpdate(req.params.id, updates, {
      new: true,
      runValidators: true,
    });

    if (!stop) return res.status(404).json({ error: "Stop not found" });
    res.json(stop);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Delete a stop (Admin only)
 */
exports.deleteStop = async (req, res) => {
  try {
    const stop = await Stop.findByIdAndDelete(req.params.id);
    if (!stop) return res.status(404).json({ error: "Stop not found" });
    res.json({ success: true, message: "Stop deleted" });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};
