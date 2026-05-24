const mongoose = require("mongoose");

async function testLocalConnection() {
    const localUri = "mongodb://localhost:27017/tnbus";
    try {
        console.log("Connecting to:", localUri);
        await mongoose.connect(localUri);
        console.log("✅ Local MongoDB Connected Successfully");
        process.exit(0);
    } catch (err) {
        console.error("❌ Local Connection Error:");
        console.error(err);
        process.exit(1);
    }
}

testLocalConnection();
