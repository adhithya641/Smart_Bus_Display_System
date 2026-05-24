const mongoose = require("mongoose");

/**
 * LiveGPS Model
 * Stores real-time GPS data from driver mobile phones
 * TTL index auto-deletes records after 24 hours to save storage
 */
const LiveGPSSchema = new mongoose.Schema({
  bus: {
    type: mongoose.Schema.Types.ObjectId,
    ref: "Bus",
    required: true,
  },
  driver: {
    type: mongoose.Schema.Types.ObjectId,
    ref: "Driver",
  },
  driverId: {
    type: mongoose.Schema.Types.ObjectId,
    ref: "Driver",
  },
  busId: {
    type: String,
    required: true,
  },
  latitude: {
    type: Number,
    required: true,
  },
  longitude: {
    type: Number,
    required: true,
  },
  location: {
    type: {
      type: String,
      enum: ["Point"],
      default: "Point",
    },
    coordinates: {
      type: [Number], // [longitude, latitude]
      required: true,
    },
  },
  speed: {
    type: Number, // km/h
    default: 0,
  },
  heading: {
    type: Number, // degrees 0-360
    default: 0,
  },
  accuracy: {
    type: Number, // meters
    default: 0,
  },
  tripId: {
    type: String,
  },
  trackingStatus: {
    type: String,
    default: "active",
  },
  timestamp: {
    type: Date,
    default: Date.now,
  },
}, {
  timestamps: false,
});

// TTL index: auto-delete GPS records after 24 hours
LiveGPSSchema.index({ timestamp: 1 }, { expireAfterSeconds: 86400 });

// Geospatial index
LiveGPSSchema.index({ location: "2dsphere" });

// Index for fast bus lookups
LiveGPSSchema.index({ bus: 1, timestamp: -1 });

module.exports = mongoose.model("LiveGPS", LiveGPSSchema);
