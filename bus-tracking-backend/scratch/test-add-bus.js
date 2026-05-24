const mongoose = require("mongoose");
const Bus = require("../models/Bus");
require("dotenv").config();

async function testAddBus() {
    try {
        await mongoose.connect(process.env.MONGO_URI);
        console.log("✅ Connected to DB");

        const testBus = new Bus({
            busId: "TEST-" + Date.now(),
            name: "Test Bus",
            routeName: "Test Route",
            district: "Coimbatore"
        });

        await testBus.save();
        console.log("✅ Bus added successfully:", testBus.busId);
        
        // Cleanup
        await Bus.findByIdAndDelete(testBus._id);
        console.log("✅ Test bus cleaned up");
        
        process.exit(0);
    } catch (err) {
        console.error("❌ Error adding bus:");
        console.error(err);
        process.exit(1);
    }
}

testAddBus();
