/**
 * Database Seed Script
 * Creates default admin user and sample data
 * Run: node seed.js
 */
require("dotenv").config();
const mongoose = require("mongoose");
const User = require("./models/User");
const Stop = require("./models/Stop");
const Bus = require("./models/Bus");
const connectDB = require("./config/db");

async function seed() {
  await connectDB();

  // 1. Create default admin
  const adminExists = await User.findOne({ email: "admin@tnbus.com" });
  if (!adminExists) {
    await User.create({
      name: "Admin",
      email: "admin@tnbus.com",
      password: "admin123",
      role: "admin",
      phone: "9999999999",
    });
    console.log("✅ Default admin created: admin@tnbus.com / admin123");
  } else {
    console.log("ℹ️ Admin already exists");
  }

  // 2. Create sample driver user
  const driverExists = await User.findOne({ email: "driver@tnbus.com" });
  if (!driverExists) {
    await User.create({
      name: "Driver Demo",
      email: "driver@tnbus.com",
      password: "driver123",
      role: "driver",
      phone: "8888888888",
    });
    console.log("✅ Default driver created: driver@tnbus.com / driver123");
  }

  // 3. Create sample passenger
  const passengerExists = await User.findOne({ email: "user@tnbus.com" });
  if (!passengerExists) {
    await User.create({
      name: "Demo User",
      email: "user@tnbus.com",
      password: "user123",
      role: "passenger",
      phone: "7777777777",
    });
    console.log("✅ Default passenger created: user@tnbus.com / user123");
  }

  // 4. Create sample stops (Coimbatore)
  const stopCount = await Stop.countDocuments();
  if (stopCount === 0) {
    const sampleStops = [
      { name: "Gandhipuram Bus Stand", code: "CBE-GAN", district: "Coimbatore", location: { type: "Point", coordinates: [76.9558, 11.0168] } },
      { name: "Ukkadam Bus Stand", code: "CBE-UKK", district: "Coimbatore", location: { type: "Point", coordinates: [76.9614, 10.9930] } },
      { name: "Singanallur", code: "CBE-SIN", district: "Coimbatore", location: { type: "Point", coordinates: [77.0060, 11.0040] } },
      { name: "Town Hall", code: "CBE-TWN", district: "Coimbatore", location: { type: "Point", coordinates: [76.9620, 11.0012] } },
      { name: "RS Puram", code: "CBE-RSP", district: "Coimbatore", location: { type: "Point", coordinates: [76.9440, 11.0090] } },
      { name: "Periyar Bus Stand", code: "MDU-PER", district: "Madurai", location: { type: "Point", coordinates: [78.1198, 9.9252] } },
      { name: "Mattuthavani", code: "MDU-MAT", district: "Madurai", location: { type: "Point", coordinates: [78.1540, 9.9670] } },
    ];
    await Stop.insertMany(sampleStops);
    console.log(`✅ ${sampleStops.length} sample stops created`);
  }

  // 5. Create sample buses
  const busCount = await Bus.countDocuments();
  if (busCount === 0) {
    const sampleBuses = [
      { busId: "CBE-101", name: "Gandhipuram Express", routeName: "Gandhipuram → Ukkadam", district: "Coimbatore", timing: "6:00 AM - 9:00 PM", type: "express" },
      { busId: "CBE-102", name: "RS Puram Local", routeName: "RS Puram → Town Hall", district: "Coimbatore", timing: "7:00 AM - 10:00 PM", type: "ordinary" },
      { busId: "MDU-201", name: "Madurai City Bus", routeName: "Periyar → Mattuthavani", district: "Madurai", timing: "5:30 AM - 11:00 PM", type: "ordinary" },
    ];
    await Bus.insertMany(sampleBuses);
    console.log(`✅ ${sampleBuses.length} sample buses created`);
  }

  console.log("\n🎉 Seed complete!");
  process.exit(0);
}

seed().catch((err) => { console.error(err); process.exit(1); });
