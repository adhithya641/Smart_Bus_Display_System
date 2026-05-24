/**
 * esp32Routes.js
 * Mount as: app.use("/api/esp32", require("./routes/esp32Routes")(io));
 *
 * Endpoints:
 *   GET  /api/esp32/display/:stopId   → Full display payload (buses + alerts + stop info)
 *   GET  /api/esp32/search?q=...      → Route search for touchscreen keyboard
 *   GET  /api/esp32/schedule          → Today's active schedules (lightweight)
 *   POST /api/esp32/register          → Register a display device (optional)
 *   GET  /api/esp32/ping              → Lightweight health check
 */

const express  = require("express");
const router   = express.Router();
const Bus      = require("../models/Bus");
const Stop     = require("../models/Stop");
const Alert    = require("../models/Alert");
const Route    = require("../models/Route");
const Schedule = require("../models/Schedule");
const { calculateETA } = require("../services/etaEngine");

module.exports = (io) => {

  // ── GET /api/esp32/ping ─────────────────────────────────────
  router.get("/ping", (req, res) => {
    res.json({ ok: true, ts: new Date().toISOString(), uptime: process.uptime() });
  });

  // ── GET /api/esp32/display/:stopId ──────────────────────────
  // Single endpoint — ESP32 calls this for everything on home screen
  router.get("/display/:stopId", async (req, res) => {
    try {
      const stop = await Stop.findById(req.params.stopId);
      if (!stop) return res.status(404).json({ error: "Stop not found" });

      const [sLng, sLat] = stop.location.coordinates;

      // Live buses within 50 km
      const activeBuses = await Bus.find({
        isOnTrip: true,
        latitude:  { $ne: null },
        longitude: { $ne: null },
      }).populate("assignedRoute", "routeNumber name");

      const upcomingBuses = activeBuses
        .map((bus) => {
          const eta = calculateETA({
            busLat: bus.latitude, busLng: bus.longitude,
            stopLat: sLat, stopLng: sLng,
            currentSpeed: bus.currentSpeed,
          });
          if (eta.distanceKm > 150) return null;
          return {
            busId:      bus.busId,
            name:       bus.name || bus.busId,
            route:      bus.routeName || bus.assignedRoute?.name || "Unknown",
            routeNum:   bus.assignedRoute?.routeNumber || "",
            type:       bus.type || "ordinary",
            eta:        eta.etaMinutes,
            distance:   +eta.distanceKm.toFixed(2),
            speed:      bus.currentSpeed || 0,
            status:     eta.status,
            confidence: eta.confidence,
          };
        })
        .filter(Boolean)
        .sort((a, b) => a.eta - b.eta)
        .slice(0, 10);

      // Active alerts (affects this stop or global)
      const alerts = await Alert.find({
        isActive: true,
        $or: [
          { affectedStops: stop._id },
          { affectedStops: { $size: 0 } },
        ],
      })
        .sort({ severity: -1 })
        .limit(5)
        .select("type title message severity");

      res.json({
        stop: {
          id:       stop._id,
          name:     stop.name,
          district: stop.district,
          code:     stop.code || "",
        },
        timestamp:  new Date().toISOString(),
        busCount:   upcomingBuses.length,
        buses:      upcomingBuses,
        alertCount: alerts.length,
        alerts:     alerts,
      });
    } catch (err) {
      res.status(500).json({ error: err.message });
    }
  });

  // ── GET /api/esp32/search?q= ────────────────────────────────
  router.get("/search", async (req, res) => {
    const q = req.query.q || "";
    if (q.length < 1) return res.json([]);
    try {
      const routes = await Route.find({
        isActive: true,
        $or: [
          { routeNumber:  { $regex: q, $options: "i" } },
          { name:         { $regex: q, $options: "i" } },
          { origin:       { $regex: q, $options: "i" } },
          { destination:  { $regex: q, $options: "i" } },
        ],
      })
        .limit(8)
        .select("routeNumber name origin destination");

      const result = await Promise.all(routes.map(async (r) => {
        const count = await Bus.countDocuments({
          assignedRoute: r._id, isOnTrip: true,
        });
        return {
          _id:          r._id,
          routeNumber:  r.routeNumber,
          name:         r.name,
          origin:       r.origin,
          destination:  r.destination,
          activeBuses:  count,
        };
      }));

      res.json(result);
    } catch (err) {
      res.status(500).json({ error: err.message });
    }
  });

  // ── GET /api/esp32/schedule ─────────────────────────────────
  router.get("/schedule", async (req, res) => {
    try {
      const day = new Date().toLocaleDateString("en-US", { weekday: "long" });
      const schedules = await Schedule.find({
        isActive: true,
        $or: [
          { frequency: "daily" },
          { daysOfOperation: day },
        ],
      })
        .populate("route", "routeNumber name origin destination")
        .populate("bus",   "busId name type")
        .sort({ departureTime: 1 })
        .limit(30);

      res.json(schedules.map((s) => ({
        routeNumber:   s.route?.routeNumber || "",
        routeName:     s.route?.name || "Unknown",
        origin:        s.route?.origin || "",
        destination:   s.route?.destination || "",
        busName:       s.bus?.name || s.bus?.busId || "?",
        busType:       s.bus?.type || "ordinary",
        departureTime: s.departureTime,
        arrivalTime:   s.arrivalTime,
        frequency:     s.frequency,
      })));
    } catch (err) {
      res.status(500).json({ error: err.message });
    }
  });

  // ── POST /api/esp32/register ────────────────────────────────
  // Lets a display announce itself (logs MAC, IP, stopId)
  router.post("/register", async (req, res) => {
    const { stopId, mac, ip, firmware } = req.body;
    console.log(`[ESP32] Display registered — Stop:${stopId} MAC:${mac} IP:${ip} FW:${firmware}`);
    // Optionally mark the stop as hasDisplay=true
    if (stopId) {
      await Stop.findByIdAndUpdate(stopId, {
        hasDisplay: true, displayId: mac || ip,
      }).catch(() => {});
    }
    // Notify admin dashboard via socket
    io.emit("displayRegistered", { stopId, mac, ip, firmware, ts: new Date() });
    res.json({ ok: true, message: "Display registered" });
  });

  return router;
};
