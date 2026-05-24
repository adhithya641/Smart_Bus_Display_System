const mongoose = require("mongoose");

/**
 * Stop Model
 * Represents a physical bus stop with geo-coordinates
 * Supports geospatial queries for nearby stop searches
 */
const StopSchema = new mongoose.Schema({
  name: {
    type: String,
    required: [true, "Stop name is required"],
    trim: true,
  },
  code: {
    type: String,
    unique: true,
    sparse: true,
    trim: true,
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
  district: {
    type: String,
    trim: true,
  },
  address: {
    type: String,
    trim: true,
  },
  connectedRoutes: [{
    type: mongoose.Schema.Types.ObjectId,
    ref: "Route",
  }],
  hasDisplay: {
    type: Boolean,
    default: false,
  },
  displayId: {
    type: String,
    trim: true,
  },
  isActive: {
    type: Boolean,
    default: true,
  },
}, {
  timestamps: true,
});

// Geospatial index for nearby stop queries
StopSchema.index({ location: "2dsphere" });

// Text index for name search
StopSchema.index({ name: "text", district: "text" });

// Virtual getters for lat/lng convenience
StopSchema.virtual("lat").get(function () {
  return this.location?.coordinates?.[1];
});

StopSchema.virtual("lng").get(function () {
  return this.location?.coordinates?.[0];
});

StopSchema.set("toJSON", { virtuals: true });
StopSchema.set("toObject", { virtuals: true });

module.exports = mongoose.model("Stop", StopSchema);
