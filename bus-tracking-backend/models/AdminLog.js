const mongoose = require("mongoose");

/**
 * AdminLog Model
 * Audit trail for all admin actions
 */
const AdminLogSchema = new mongoose.Schema({
  admin: {
    type: mongoose.Schema.Types.ObjectId,
    ref: "User",
    required: true,
  },
  action: {
    type: String,
    required: true,
    enum: [
      "CREATE_BUS", "UPDATE_BUS", "DELETE_BUS",
      "CREATE_ROUTE", "UPDATE_ROUTE", "DELETE_ROUTE",
      "CREATE_STOP", "UPDATE_STOP", "DELETE_STOP",
      "CREATE_SCHEDULE", "UPDATE_SCHEDULE", "DELETE_SCHEDULE",
      "ASSIGN_DRIVER", "UNASSIGN_DRIVER",
      "CREATE_ALERT", "UPDATE_ALERT", "DELETE_ALERT",
      "CREATE_USER", "UPDATE_USER", "DELETE_USER",
      "LOGIN", "LOGOUT",
    ],
  },
  entity: {
    type: String, // "Bus", "Route", "Stop", etc.
    required: true,
  },
  entityId: {
    type: mongoose.Schema.Types.ObjectId,
  },
  details: {
    type: mongoose.Schema.Types.Mixed, // Flexible JSON for change details
  },
  ipAddress: {
    type: String,
  },
}, {
  timestamps: true, // createdAt serves as timestamp
});

// Index for admin activity queries
AdminLogSchema.index({ admin: 1, createdAt: -1 });
AdminLogSchema.index({ entity: 1, entityId: 1 });

module.exports = mongoose.model("AdminLog", AdminLogSchema);
