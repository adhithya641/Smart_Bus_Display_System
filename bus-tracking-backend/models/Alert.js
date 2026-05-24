const mongoose = require("mongoose");

/**
 * Alert Model
 * System-wide alerts for delays, emergencies, weather, etc.
 * Shown on bus stop displays, passenger dashboard, and admin panel
 */
const AlertSchema = new mongoose.Schema({
  type: {
    type: String,
    enum: ["delay", "breakdown", "diversion", "emergency", "weather", "info"],
    required: true,
  },
  title: {
    type: String,
    required: [true, "Alert title is required"],
    trim: true,
    maxlength: 200,
  },
  message: {
    type: String,
    required: [true, "Alert message is required"],
    trim: true,
    maxlength: 1000,
  },
  severity: {
    type: String,
    enum: ["info", "warning", "critical"],
    default: "info",
  },
  affectedRoutes: [{
    type: mongoose.Schema.Types.ObjectId,
    ref: "Route",
  }],
  affectedStops: [{
    type: mongoose.Schema.Types.ObjectId,
    ref: "Stop",
  }],
  affectedBuses: [{
    type: mongoose.Schema.Types.ObjectId,
    ref: "Bus",
  }],
  isActive: {
    type: Boolean,
    default: true,
  },
  createdBy: {
    type: mongoose.Schema.Types.ObjectId,
    ref: "User",
  },
  expiresAt: {
    type: Date,
  },
}, {
  timestamps: true,
});

// TTL index: auto-delete expired alerts
AlertSchema.index({ expiresAt: 1 }, { expireAfterSeconds: 0 });

// Index for active alerts query
AlertSchema.index({ isActive: 1, severity: 1 });

module.exports = mongoose.model("Alert", AlertSchema);
