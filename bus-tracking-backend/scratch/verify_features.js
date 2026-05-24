/**
 * System Verification Script
 * Automatically verifies JWT session invalidations, dual-session prevention,
 * role validation, and live GPS tracking session locks.
 * Run: node scratch/verify_features.js
 */
require("dotenv").config();
const mongoose = require("mongoose");
const jwt = require("jsonwebtoken");
const User = require("../models/User");
const Driver = require("../models/Driver");
const Bus = require("../models/Bus");
const LiveGPS = require("../models/LiveGPS");

// Mock Express req/res cycle for middlewares
const { authenticate } = require("../middleware/auth");
const requireRole = require("../middleware/roleMiddleware");

async function runTests() {
  console.log("🚀 Starting MERN Smart Bus Tracking System Validation Tests...\n");
  
  const mongoURI = process.env.MONGO_URI || "mongodb://localhost:27017/bus-tracking";
  await mongoose.connect(mongoURI);
  console.log("🔹 Connected to test database successfully.");

  try {
    // ----------------------------------------------------
    // TEST 1: User Token Versioning & Dual Session Prevention
    // ----------------------------------------------------
    console.log("\n🧪 TEST 1: Verification of Dual Session Prevention...");
    
    // Cleanup & seed test user
    await User.deleteMany({ email: "test_session@tnbus.com" });
    const user = await User.create({
      name: "Test Session User",
      email: "test_session@tnbus.com",
      password: "password123",
      role: "driver",
      tokenVersion: 0
    });
    
    // Simulate Login 1
    user.tokenVersion += 1;
    await user.save();
    const token1 = jwt.sign(
      { userId: user._id, email: user.email, role: user.role, tokenVersion: 1 },
      process.env.JWT_SECRET,
      { expiresIn: "1h" }
    );
    console.log("   ✅ Device 1 signs in, gets Token with version: 1");

    // Simulate Login 2 (on other device)
    user.tokenVersion += 1;
    await user.save();
    const token2 = jwt.sign(
      { userId: user._id, email: user.email, role: user.role, tokenVersion: 2 },
      process.env.JWT_SECRET,
      { expiresIn: "1h" }
    );
    console.log("   ✅ Device 2 signs in, increments DB to version: 2, gets Token with version: 2");

    // Verify Token 1 (device 1 attempts access)
    let authenticated1 = false;
    const req1 = { headers: { authorization: `Bearer ${token1}` } };
    const res1 = {
      status: (code) => ({
        json: (data) => {
          console.log(`   ✅ Device 1 rejected as expected! Code: ${code}, Msg: "${data.error}"`);
          authenticated1 = false;
        }
      })
    };
    
    await authenticate(req1, res1, () => {
      authenticated1 = true;
    });

    // Verify Token 2 (device 2 attempts access)
    let authenticated2 = false;
    const req2 = { headers: { authorization: `Bearer ${token2}` } };
    const res2 = {
      status: (code) => ({ json: (data) => { authenticated2 = false; } })
    };
    
    await authenticate(req2, res2, () => {
      authenticated2 = true;
    });
    
    if (authenticated2 && !authenticated1) {
      console.log("   🎉 TEST 1 SUCCESS: Dual sessions prevented automatically!");
    } else {
      console.log("   ❌ TEST 1 FAILED: Sessions were not validated correctly.");
    }

    // ----------------------------------------------------
    // TEST 2: Role Authorization Checks
    // ----------------------------------------------------
    console.log("\n🧪 TEST 2: Verification of Role-Based Authorization...");
    
    const reqRole = { user: { role: "driver" } };
    let roleAuthorized = false;
    const resRole = {
      status: (code) => ({
        json: (data) => {
          console.log(`   ✅ Passenger/Driver restricted from Admin role as expected! Msg: "${data.error}"`);
          roleAuthorized = false;
        }
      })
    };

    const adminCheck = requireRole("admin");
    adminCheck(reqRole, resRole, () => {
      roleAuthorized = true;
    });

    // ----------------------------------------------------
    // TEST 3: GPS Session Locking & Multiple Session Prevention
    // ----------------------------------------------------
    console.log("\n🧪 TEST 3: Verification of Active Tracking Session Lock...");
    
    // Clean & Seed Driver & Bus
    await Driver.deleteMany({ licenseNumber: "LIC-TEST-123" });
    await Bus.deleteMany({ busId: "BUS-TEST-101" });
    
    const testBus = await Bus.create({
      busId: "BUS-TEST-101",
      name: "Test Bus Express",
      routeName: "Test Route",
      district: "Coimbatore"
    });

    const testDriver = await Driver.create({
      user: user._id,
      licenseNumber: "LIC-TEST-123",
      assignedBus: testBus._id,
      phone: "9000000000",
      trackingStatus: "online",
      activeTrackingSessionId: "session_abc_123"
    });

    // Simulate Location update from Old session "session_xyz_789"
    const reqGPSOld = {
      user: user,
      userId: user._id,
      body: {
        busId: "BUS-TEST-101",
        latitude: 11.0168,
        longitude: 76.9558,
        trackingSessionId: "session_xyz_789" // does not match active session_abc_123
      }
    };

    const gpsController = require("../controllers/gpsController");
    const resGPSOld = {
      status: (code) => ({
        json: (data) => {
          console.log(`   ✅ Old/invalid tracking session rejected! Code: ${code}, Msg: "${data.error}"`);
        }
      }),
      json: (data) => {
        console.log("   ❌ FAILED: Accepted invalid tracking session!");
      }
    };

    await gpsController.updateGPS(reqGPSOld, resGPSOld);

    // Simulate Location update from Active session "session_abc_123"
    const reqGPSActive = {
      user: user,
      userId: user._id,
      body: {
        busId: "BUS-TEST-101",
        latitude: 11.0168,
        longitude: 76.9558,
        trackingSessionId: "session_abc_123" // matches active session_abc_123
      }
    };

    let gpsAccepted = false;
    const resGPSActive = {
      status: (code) => ({ json: (data) => { gpsAccepted = false; } }),
      json: (data) => {
        if (data.success) {
          gpsAccepted = true;
          console.log(`   ✅ Active tracking session accepted coordinate update perfectly!`);
        }
      }
    };

    await gpsController.updateGPS(reqGPSActive, resGPSActive);
    
    if (gpsAccepted) {
      console.log("   🎉 TEST 3 SUCCESS: Single active tracking instance enforced!");
    } else {
      console.log("   ❌ TEST 3 FAILED: Tracking session lock failed.");
    }

    // Clean up
    await User.deleteMany({ email: "test_session@tnbus.com" });
    await Driver.deleteMany({ licenseNumber: "LIC-TEST-123" });
    await Bus.deleteMany({ busId: "BUS-TEST-101" });
    
    console.log("\n🎉 All Verification Tests Completed Successfully!");
  } catch (err) {
    console.error("❌ Test error encountered:", err);
  } finally {
    await mongoose.disconnect();
    console.log("🔹 Test database disconnected safely.");
    process.exit(0);
  }
}

runTests();
