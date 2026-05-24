const Bus = require("../models/Bus");
const LiveGPS = require("../models/LiveGPS");
const Driver = require("../models/Driver");
const Stop = require("../models/Stop");
const { calculateETA } = require("../services/etaEngine");
const { emitBusLocation } = require("../socket/socketHandler");

/**
 * Receive live GPS coordinates from the driver's phone
 * Enforces session locks to prevent multiple tracking instances per driver
 */
exports.updateGPS = async (req, res) => {
  const { busId, latitude, longitude, speed, heading, accuracy, tripId, trackingSessionId } = req.body;

  if (!busId || latitude === undefined || longitude === undefined) {
    return res.status(400).json({ error: "busId, latitude, longitude are required." });
  }

  try {
    let driverId = null;
    let driver = null;

    // Check if the submitter is a driver
    if (req.user.role === "driver") {
      driver = await Driver.findOne({ user: req.userId });
      if (!driver) {
        return res.status(403).json({ error: "Driver profile not found." });
      }
      driverId = driver._id;

      // Enforce active tracking session check to prevent duplicate tracking instances
      if (driver.activeTrackingSessionId && driver.activeTrackingSessionId !== trackingSessionId) {
        return res.status(400).json({
          error: "Tracking session invalidated. Another session is active on a different device.",
          code: "SESSION_INVALIDATED"
        });
      }
    }

    let bus = await Bus.findOne({ busId });
    if (!bus) {
      bus = new Bus({ 
        busId, 
        name: busId, 
        latitude, 
        longitude,
        assignedDriver: driverId
      });
    }

    bus.latitude = latitude;
    bus.longitude = longitude;
    bus.currentSpeed = speed || 0;
    bus.heading = heading || 0;
    bus.lastUpdated = Date.now();
    bus.isOnTrip = true;
    await bus.save();

    // Store in LiveGPS collection matching exact fields of Requirement 12
    await LiveGPS.create({
      bus: bus._id,
      driver: driverId,
      driverId: driverId,
      busId: bus.busId,
      latitude,
      longitude,
      location: { type: "Point", coordinates: [longitude, latitude] },
      speed: speed || 0,
      heading: heading || 0,
      accuracy: accuracy || 0,
      tripId: tripId || (driver ? driver.activeTrackingSessionId : "simulated"),
      trackingStatus: "active",
      timestamp: new Date()
    });

    // Broadcast update to Socket.IO clients
    emitBusLocation({
      _id: bus._id,
      busId: bus.busId,
      name: bus.name,
      latitude,
      longitude,
      speed: speed || 0,
      heading: heading || 0,
      lastUpdated: bus.lastUpdated,
      isOnTrip: true,
    });

    res.json({ success: true, trackingStatus: "active" });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Get latest GPS position of a specific bus
 */
exports.getLatestBusPosition = async (req, res) => {
  try {
    const bus = await Bus.findOne({ busId: req.params.busId })
      .select("busId name latitude longitude currentSpeed heading lastUpdated isOnTrip routeName type");
    if (!bus) return res.status(404).json({ error: "Bus not found" });
    res.json(bus);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Get the historical trail of a bus
 */
exports.getBusHistory = async (req, res) => {
  try {
    const bus = await Bus.findOne({ busId: req.params.busId });
    if (!bus) return res.status(404).json({ error: "Bus not found" });
    const history = await LiveGPS.find({ bus: bus._id }).sort({ timestamp: -1 }).limit(100);
    res.json(history);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Get upcoming buses for ESP32 physical bus stop display
 */
exports.getESP32StopDisplay = async (req, res) => {
  try {
    const stop = await Stop.findById(req.params.stopId);
    if (!stop) return res.status(404).json({ error: "Stop not found" });
    const stopLat = stop.location.coordinates[1];
    const stopLng = stop.location.coordinates[0];

    const activeBuses = await Bus.find({
      isOnTrip: true,
      latitude: { $ne: null },
      longitude: { $ne: null },
    }).populate("assignedRoute");

    const upcomingBuses = activeBuses
      .map((bus) => {
        const eta = calculateETA({
          busLat: bus.latitude,
          busLng: bus.longitude,
          stopLat,
          stopLng,
          currentSpeed: bus.currentSpeed,
        });
        if (eta.distanceKm > 50) return null;
        return {
          busId: bus.busId,
          name: bus.name || bus.busId,
          route: bus.routeName || bus.assignedRoute?.name || "Unknown",
          type: bus.type || "ordinary",
          eta: eta.etaMinutes,
          distance: eta.distanceKm,
          speed: bus.currentSpeed,
          status: eta.status,
          confidence: eta.confidence,
        };
      })
      .filter(Boolean)
      .sort((a, b) => a.eta - b.eta)
      .slice(0, 10);

    res.json({
      stop: { id: stop._id, name: stop.name, district: stop.district },
      timestamp: new Date().toISOString(),
      busCount: upcomingBuses.length,
      buses: upcomingBuses,
    });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Calculate dynamic ETA between a specific bus and stop
 */
exports.getETA = async (req, res) => {
  try {
    const { busId, stopId } = req.query;
    if (!busId || !stopId) return res.status(400).json({ error: "busId and stopId are required." });
    
    const bus = await Bus.findOne({ busId });
    const stop = await Stop.findById(stopId);
    
    if (!bus || !bus.latitude) return res.status(404).json({ error: "Bus not found or has no active GPS signal." });
    if (!stop) return res.status(404).json({ error: "Stop not found." });
    
    const eta = calculateETA({
      busLat: bus.latitude,
      busLng: bus.longitude,
      stopLat: stop.location.coordinates[1],
      stopLng: stop.location.coordinates[0],
      currentSpeed: bus.currentSpeed,
    });
    
    res.json({ 
      bus: { busId: bus.busId, name: bus.name }, 
      stop: { name: stop.name }, 
      ...eta 
    });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};
