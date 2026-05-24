require("dotenv").config();
const mongoose = require("mongoose");
const Stop = require("./models/Stop");
const Bus = require("./models/Bus");
const connectDB = require("./config/db");

async function seedKSR() {
  await connectDB();

  // Create KSR Stop
  let ksrStop = await Stop.findOne({ name: "KSR Stop" });
  if (!ksrStop) {
    ksrStop = await Stop.create({
      name: "KSR Stop",
      code: "KSR-STP",
      district: "Namakkal",
      location: { type: "Point", coordinates: [77.8264, 11.3597] } // driver's simulated location
    });
    console.log("✅ Created KSR Stop: ID =", ksrStop._id);
  } else {
    console.log("ℹ️ KSR Stop already exists: ID =", ksrStop._id);
  }

  // Create KSR Express Bus106
  let ksrBus = await Bus.findOne({ busId: "BUS106" });
  if (!ksrBus) {
    ksrBus = await Bus.create({
      busId: "BUS106",
      name: "KSR Express",
      routeName: "KSR Stop → Erode",
      district: "Namakkal",
      timing: "8:00 AM - 8:00 PM",
      type: "express",
      isOnTrip: true,
      latitude: 11.3597,
      longitude: 77.8264,
      currentSpeed: 30,
      heading: 90,
      status: "active"
    });
    console.log("✅ Created KSR Express Bus (BUS106): ID =", ksrBus._id);
  } else {
    // If it exists, update it to make sure it is on trip with proper coordinates
    ksrBus.isOnTrip = true;
    ksrBus.latitude = 11.3597;
    ksrBus.longitude = 77.8264;
    ksrBus.name = "KSR Express";
    ksrBus.routeName = "KSR Stop → Erode";
    await ksrBus.save();
    console.log("✅ Updated KSR Express Bus (BUS106) trip status and location.");
  }

  console.log("\n🎉 Seeding KSR complete! Stop ID is:", ksrStop._id.toString());
  process.exit(0);
}

seedKSR().catch((err) => { console.error(err); process.exit(1); });
