/**
 * Admin User Verification & Reset Script
 * Verifies if admin@tnbus.com exists in the database and resets their password to admin123.
 * Run: node scratch/check_admin.js
 */
require("dotenv").config();
const mongoose = require("mongoose");
const User = require("../models/User");
const Driver = require("../models/Driver");
const Bus = require("../models/Bus");

async function checkAdmin() {
  const mongoURI = process.env.MONGO_URI;
  if (!mongoURI) {
    console.error("❌ MONGO_URI is not defined in .env!");
    process.exit(1);
  }

  console.log("🔹 Connecting to MongoDB...");
  await mongoose.connect(mongoURI);
  console.log("🔹 Connected successfully.");

  try {
    const adminEmail = "admin@tnbus.com";
    const defaultPassword = "admin123";

    console.log(`\n🔎 Searching for user with email: ${adminEmail}...`);
    let admin = await User.findOne({ email: adminEmail });

    if (admin) {
      console.log("✅ Admin user found!");
      console.log(`   ID: ${admin._id}`);
      console.log(`   Name: ${admin.name}`);
      console.log(`   Role: ${admin.role}`);
      console.log(`   Active: ${admin.isActive}`);
      
      // Force reset password and ensure active
      console.log(`\n🔧 Resetting password to "${defaultPassword}" and ensuring role is "admin"...`);
      admin.password = defaultPassword;
      admin.role = "admin";
      admin.isActive = true;
      await admin.save();
      console.log("🎉 Admin user updated successfully!");
    } else {
      console.log("⚠️ Admin user not found in the database. Creating one now...");
      
      admin = new User({
        name: "Admin User",
        email: adminEmail,
        password: defaultPassword,
        role: "admin",
        phone: "9999999999",
        isActive: true,
        tokenVersion: 0
      });

      await admin.save();
      console.log(`🎉 Admin user created successfully with password "${defaultPassword}"!`);
    }

    // Double check other roles if needed
    console.log("\n🔎 Verifying Driver User...");
    let driverUser = await User.findOne({ email: "driver@tnbus.com" });
    if (!driverUser) {
      console.log("⚠️ Driver user not found, creating driver@tnbus.com...");
      driverUser = await User.create({
        name: "Driver Demo",
        email: "driver@tnbus.com",
        password: "driver123",
        role: "driver",
        phone: "8888888888",
        isActive: true,
        tokenVersion: 0
      });
      console.log("🎉 Driver user created!");
    } else {
      console.log("✅ Driver user exists.");
    }

    // Now, verify/create the Driver profile collection document!
    let driverProfile = await Driver.findOne({ user: driverUser._id });
    if (!driverProfile) {
      console.log("⚠️ Driver profile document not found. Seeding driver profile...");
      let bus = await Bus.findOne({ busId: "CBE-101" });
      if (!bus) {
        // Create sample bus if missing
        bus = await Bus.create({
          busId: "CBE-101",
          name: "Gandhipuram Express",
          routeName: "Gandhipuram → Ukkadam",
          district: "Coimbatore",
          timing: "6:00 AM - 9:00 PM",
          type: "express",
          status: "active"
        });
        console.log("🎉 Seeded CBE-101 bus!");
      }

      driverProfile = new Driver({
        user: driverUser._id,
        licenseNumber: "DL-TN-2026-0099",
        assignedBus: bus._id,
        phone: "8888888888",
        isActive: true
      });
      await driverProfile.save();
      
      bus.assignedDriver = driverProfile._id;
      await bus.save();
      
      console.log("🎉 Driver profile created and linked to Bus CBE-101!");
    } else {
      console.log("✅ Driver profile document already exists in collection!");
      // Ensure it is assigned a bus
      if (!driverProfile.assignedBus) {
        let bus = await Bus.findOne({ busId: "CBE-101" });
        if (bus) {
          driverProfile.assignedBus = bus._id;
          await driverProfile.save();
          bus.assignedDriver = driverProfile._id;
          await bus.save();
          console.log("🎉 Re-assigned driver profile to Bus CBE-101.");
        }
      }
    }

    console.log("\n🔎 Verifying Passenger User...");
    const passenger = await User.findOne({ email: "user@tnbus.com" });
    if (!passenger) {
      console.log("⚠️ Passenger not found, creating user@tnbus.com...");
      await User.create({
        name: "Passenger Demo",
        email: "user@tnbus.com",
        password: "user123",
        role: "passenger",
        phone: "7777777777",
        isActive: true,
        tokenVersion: 0
      });
      console.log("🎉 Passenger created with password user123!");
    } else {
      console.log("✅ Passenger user exists.");
    }

  } catch (err) {
    console.error("❌ Error running verification:", err);
  } finally {
    await mongoose.disconnect();
    console.log("\n🔹 Disconnected from MongoDB.");
    process.exit(0);
  }
}

checkAdmin();
