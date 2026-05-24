const mongoose = require("mongoose");

/**
 * Schedule Model
 * Defines when buses run on specific routes
 */
const ScheduleSchema = new mongoose.Schema({
  route: {
    type: mongoose.Schema.Types.ObjectId,
    ref: "Route",
    required: true,
  },
  bus: {
    type: mongoose.Schema.Types.ObjectId,
    ref: "Bus",
    required: true,
  },
  driver: {
    type: mongoose.Schema.Types.ObjectId,
    ref: "Driver",
  },
  departureTime: {
    type: String, // "HH:MM" format, e.g., "06:30"
    required: [true, "Departure time is required"],
  },
  arrivalTime: {
    type: String, // "HH:MM" format
    required: [true, "Arrival time is required"],
  },
  daysOfOperation: [{
    type: String,
    enum: ["Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"],
  }],
  frequency: {
    type: String,
    enum: ["daily", "weekdays", "weekends", "custom"],
    default: "daily",
  },
  isActive: {
    type: Boolean,
    default: true,
  },
}, {
  timestamps: true,
});

// Compound index for route + day queries
ScheduleSchema.index({ route: 1, isActive: 1 });
ScheduleSchema.index({ bus: 1, isActive: 1 });

module.exports = mongoose.model("Schedule", ScheduleSchema);
