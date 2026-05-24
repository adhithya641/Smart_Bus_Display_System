const Route = require("../models/Route");
const Stop = require("../models/Stop");
const Bus = require("../models/Bus");

/**
 * Get all routes with optional filters
 */
exports.getAllRoutes = async (req, res) => {
  try {
    const filter = {};
    if (req.query.district) filter.district = req.query.district;
    if (req.query.type) filter.type = req.query.type;
    if (req.query.active !== undefined) filter.isActive = req.query.active === "true";

    const routes = await Route.find(filter)
      .populate("stops.stop")
      .sort({ routeNumber: 1 });

    res.json(routes);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Search routes by origin and destination with active bus tracking
 */
exports.searchRoutes = async (req, res) => {
  try {
    const { from, to, district } = req.query;

    if (!from && !to) {
      return res.status(400).json({ error: "Provide 'from' or 'to' query parameter." });
    }

    const filter = { isActive: true };
    if (district) filter.district = district;

    let routes;
    if (from && to) {
      routes = await Route.find({
        ...filter,
        $or: [
          {
            origin: { $regex: from, $options: "i" },
            destination: { $regex: to, $options: "i" },
          },
          {
            origin: { $regex: to, $options: "i" },
            destination: { $regex: from, $options: "i" },
          },
          {
            name: { $regex: from, $options: "i" },
          },
          {
            name: { $regex: to, $options: "i" },
          },
        ],
      }).populate("stops.stop");
    } else {
      const query = from || to;
      routes = await Route.find({
        ...filter,
        $or: [
          { origin: { $regex: query, $options: "i" } },
          { destination: { $regex: query, $options: "i" } },
          { name: { $regex: query, $options: "i" } },
        ],
      }).populate("stops.stop");
    }

    // Attach active buses to each matching route
    const results = await Promise.all(
      routes.map(async (route) => {
        const buses = await Bus.find({
          assignedRoute: route._id,
          status: "active",
        }).select("busId name isOnTrip latitude longitude currentSpeed lastUpdated type");

        return {
          ...route.toObject(),
          activeBuses: buses,
        };
      })
    );

    res.json(results);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Get a single route profile by ID
 */
exports.getRouteById = async (req, res) => {
  try {
    const route = await Route.findById(req.params.id).populate("stops.stop");
    if (!route) return res.status(404).json({ error: "Route not found" });

    // Get buses active on this route
    const buses = await Bus.find({
      assignedRoute: route._id,
      status: "active",
    });

    res.json({ ...route.toObject(), buses });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Get active buses on a specific route with their live positions
 */
exports.getBusesOnRoute = async (req, res) => {
  try {
    const buses = await Bus.find({
      assignedRoute: req.params.id,
      status: "active",
    }).populate("assignedDriver");

    res.json(buses);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Create a new route (Admin only)
 */
exports.createRoute = async (req, res) => {
  try {
    const route = new Route(req.body);
    await route.save();

    // Map connected routes on stops
    if (route.stops && route.stops.length > 0) {
      const stopIds = route.stops.map((s) => s.stop);
      await Stop.updateMany(
        { _id: { $in: stopIds } },
        { $addToSet: { connectedRoutes: route._id } }
      );
    }

    res.status(201).json(route);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Update an existing route (Admin only)
 */
exports.updateRoute = async (req, res) => {
  try {
    const route = await Route.findByIdAndUpdate(req.params.id, req.body, {
      new: true,
      runValidators: true,
    }).populate("stops.stop");

    if (!route) return res.status(404).json({ error: "Route not found" });
    res.json(route);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Delete a route (Admin only)
 */
exports.deleteRoute = async (req, res) => {
  try {
    const route = await Route.findByIdAndDelete(req.params.id);
    if (!route) return res.status(404).json({ error: "Route not found" });

    // Clean up connections on stops
    await Stop.updateMany(
      { connectedRoutes: route._id },
      { $pull: { connectedRoutes: route._id } }
    );

    res.json({ success: true, message: "Route deleted" });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};
