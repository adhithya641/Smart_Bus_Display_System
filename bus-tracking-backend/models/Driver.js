const mongoose = require("mongoose");

/**
 * Driver Model
 * Links to User model for auth, tracks assigned bus and trip status
 */
const DriverSchema = new mongoose.Schema({
  user: {
    type: mongoose.Schema.Types.ObjectId,
    ref: "User",
    required: true,
    unique: true,
  },
  licenseNumber: {
    type: String,
    required: [true, "License number is required"],
    unique: true,
    trim: true,
  },
  assignedBus: {
    type: mongoose.Schema.Types.ObjectId,
    ref: "Bus",
    default: null,
  },
  phone: {
    type: String,
    required: [true, "Phone number is required"],
    trim: true,
  },
  isActive: {
    type: Boolean,
    default: true,
  },
  trackingStatus: {
    type: String,
    enum: ["online", "offline"],
    default: "offline",
  },
  activeTrackingSessionId: {
    type: String,
    default: null,
  },
  currentTrip: {
    isOnTrip: { type: Boolean, default: false },
    startTime: { type: Date, default: null },
    routeId: { type: mongoose.Schema.Types.ObjectId, ref: "Route", default: null },
    status: {
      type: String,
      enum: ["idle", "active", "paused", "completed"],
      default: "idle",
    },
  },
  totalTrips: {
    type: Number,
    default: 0,
  },
  rating: {
    type: Number,
    default: 5.0,
    min: 0,
    max: 5,
  },
}, {
  timestamps: true,
});

module.exports = mongoose.model("Driver", DriverSchema);
