const mongoose = require("mongoose");
require("dotenv").config();

async function testConnection() {
    try {
        console.log("Connecting to:", process.env.MONGO_URI);
        await mongoose.connect(process.env.MONGO_URI);
        console.log("✅ MongoDB Connected Successfully");
        process.exit(0);
    } catch (err) {
        console.error("❌ Connection Error:");
        console.error(err);
        process.exit(1);
    }
}

testConnection();
