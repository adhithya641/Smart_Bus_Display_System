let ioInstance = null;

/**
 * Initialize Socket.IO connection and hooks
 * @param {object} io - Socket.io server instance
 */
const initSocket = (io) => {
  ioInstance = io;

  io.on("connection", (socket) => {
    console.log(`🔌 Client connected: ${socket.id}`);

    // Join specific bus room for specialized updates if needed
    socket.on("joinBusRoom", (busId) => {
      socket.join(`bus_${busId}`);
      console.log(`📡 Client ${socket.id} joined tracking room for bus: ${busId}`);
    });

    socket.on("leaveBusRoom", (busId) => {
      socket.leave(`bus_${busId}`);
      console.log(`📡 Client ${socket.id} left tracking room for bus: ${busId}`);
    });

    socket.on("disconnect", () => {
      console.log(`🔌 Client disconnected: ${socket.id}`);
    });
  });
};

/**
 * Broadcast live bus location to all active clients and specific rooms
 * @param {object} data - Bus coordinates and speed information
 */
const emitBusLocation = (data) => {
  if (!ioInstance) return;

  // Broadcast to all (for general dashboards)
  ioInstance.emit("busLocationUpdate", data);

  // Broadcast specifically to the bus room (for dedicated passenger views)
  if (data.busId) {
    ioInstance.to(`bus_${data.busId}`).emit("busLocationUpdate", data);
  }
};

module.exports = { initSocket, emitBusLocation };
