const mongoose = require("mongoose");

/**
 * Bus Model (Enhanced)
 * Core entity representing a physical bus vehicle
 * Links to assigned driver, route, and tracks live status
 */
const StopSchema = new mongoose.Schema({
  name: String,
  order: Number,
  expectedArrival: String,
  lat: Number,
  lng: Number,
});

const BusSchema = new mongoose.Schema({
  busId: {
    type: String,
    required: [true, "Bus ID is required"],
    unique: true,
    trim: true,
  },
  name: {
    type: String,
    required: [true, "Bus name is required"],
    trim: true,
  },
  registrationNumber: {
    type: String,
    trim: true,
  },
  routeName: {
    type: String,
    trim: true,
  },
  district: {
    type: String,
    trim: true,
  },
  timing: {
    type: String,
    trim: true,
  },
  type: {
    type: String,
    enum: ["ordinary", "express", "deluxe", "ac"],
    default: "ordinary",
  },
  capacity: {
    type: Number,
    default: 50,
  },
  status: {
    type: String,
    enum: ["active", "maintenance", "retired"],
    default: "active",
  },
  assignedDriver: {
    type: mongoose.Schema.Types.ObjectId,
    ref: "Driver",
    default: null,
  },
  assignedRoute: {
    type: mongoose.Schema.Types.ObjectId,
    ref: "Route",
    default: null,
  },
  // Live tracking fields
  latitude: {
    type: Number,
    default: null,
  },
  longitude: {
    type: Number,
    default: null,
  },
  currentSpeed: {
    type: Number,
    default: 0,
  },
  heading: {
    type: Number,
    default: 0,
  },
  isOnTrip: {
    type: Boolean,
    default: false,
  },
  lastUpdated: {
    type: Date,
    default: Date.now,
  },
  // Legacy stops (kept for backward compatibility)
  stops: [StopSchema],
}, {
  timestamps: true,
});

// Index for district + status queries
BusSchema.index({ district: 1, status: 1 });

module.exports = mongoose.model("Bus", BusSchema);
