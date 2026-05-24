const express = require("express");
const router = express.Router();
const Bus = require("../models/Bus");

module.exports = (io) => {

  // GET all buses (optional ?district= filter)
  router.get("/buses", async (req, res) => {
    try {
      const filter = {};
      if (req.query.district) {
        filter.district = req.query.district;
      }
      const buses = await Bus.find(filter);
      res.json(buses);
    } catch (err) {
      res.status(500).json({ error: err.message });
    }
  });

  // GET single bus
  router.get("/buses/:id", async (req, res) => {
    try {
      const bus = await Bus.findById(req.params.id);
      if (!bus) return res.status(404).json({ error: "Bus not found" });
      res.json(bus);
    } catch (err) {
      res.status(500).json({ error: err.message });
    }
  });

  // POST create a new bus
  router.post("/buses", async (req, res) => {
    try {
      const bus = new Bus(req.body);
      await bus.save();
      res.status(201).json(bus);
    } catch (err) {
      res.status(500).json({ error: err.message });
    }
  });

  // PUT update a bus
  router.put("/buses/:id", async (req, res) => {
    try {
      const bus = await Bus.findByIdAndUpdate(req.params.id, req.body, { new: true });
      if (!bus) return res.status(404).json({ error: "Bus not found" });
      res.json(bus);
    } catch (err) {
      res.status(500).json({ error: err.message });
    }
  });

  // DELETE a bus
  router.delete("/buses/:id", async (req, res) => {
    try {
      const bus = await Bus.findByIdAndDelete(req.params.id);
      if (!bus) return res.status(404).json({ error: "Bus not found" });
      res.json({ success: true });
    } catch (err) {
      res.status(500).json({ error: err.message });
    }
  });

  // Receive GPS location update (from ESP32 or driver phone)
  router.post("/update-location", async (req, res) => {
    const { busId, latitude, longitude } = req.body;

    try {
      let bus = await Bus.findOne({ busId });

      if (!bus) {
        bus = new Bus({ busId, latitude, longitude });
      } else {
        bus.latitude = latitude;
        bus.longitude = longitude;
        bus.lastUpdated = Date.now();
      }

      await bus.save();

      // Send live update to frontend
      io.emit("busLocationUpdate", bus);

      console.log(`📡 Update received for ${busId}`);
      res.json({ success: true });
    } catch (err) {
      res.status(500).json({ error: err.message });
    }
  });

  return router;
};
