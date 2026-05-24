const mongoose = require("mongoose");

/**
 * Route Model
 * Defines bus routes with ordered stops, fare info, and distance
 */
const RouteStopSchema = new mongoose.Schema({
  stop: {
    type: mongoose.Schema.Types.ObjectId,
    ref: "Stop",
    required: true,
  },
  order: {
    type: Number,
    required: true,
  },
  distanceFromStart: {
    type: Number, // in km
    default: 0,
  },
  expectedTimeFromStart: {
    type: Number, // in minutes
    default: 0,
  },
});

const RouteSchema = new mongoose.Schema({
  routeNumber: {
    type: String,
    required: [true, "Route number is required"],
    unique: true,
    trim: true,
  },
  name: {
    type: String,
    required: [true, "Route name is required"],
    trim: true,
  },
  origin: {
    type: String,
    required: true,
    trim: true,
  },
  destination: {
    type: String,
    required: true,
    trim: true,
  },
  district: {
    type: String,
    trim: true,
  },
  stops: [RouteStopSchema],
  totalDistance: {
    type: Number, // in km
    default: 0,
  },
  estimatedDuration: {
    type: Number, // in minutes
    default: 0,
  },
  fare: {
    base: { type: Number, default: 10 },
    perKm: { type: Number, default: 1.5 },
  },
  type: {
    type: String,
    enum: ["ordinary", "express", "deluxe", "ac"],
    default: "ordinary",
  },
  isActive: {
    type: Boolean,
    default: true,
  },
}, {
  timestamps: true,
});

// Text index for search
RouteSchema.index({ name: "text", origin: "text", destination: "text" });

module.exports = mongoose.model("Route", RouteSchema);
