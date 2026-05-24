const jwt = require("jsonwebtoken");
const User = require("../models/User");

/**
 * JWT Authentication Middleware
 * Verifies Bearer token, checks tokenVersion for active session tracking,
 * and checks for role updates to force re-login if altered in Atlas.
 */
const authenticate = async (req, res, next) => {
  try {
    const authHeader = req.headers.authorization;

    if (!authHeader || !authHeader.startsWith("Bearer ")) {
      return res.status(401).json({ error: "Access denied. No token provided." });
    }

    const token = authHeader.split(" ")[1];
    const decoded = jwt.verify(token, process.env.JWT_SECRET);

    const user = await User.findById(decoded.userId);
    if (!user || !user.isActive) {
      return res.status(401).json({ error: "Invalid token. User not found or inactive." });
    }

    // 1. Single session tracking check
    if (user.tokenVersion !== undefined && decoded.tokenVersion !== user.tokenVersion) {
      return res.status(401).json({ error: "Session expired. Logged in from another device." });
    }

    // 2. Role update check (Forces re-login if changed directly in Atlas)
    if (decoded.role !== user.role) {
      return res.status(401).json({ error: "Your role has been updated. Please log in again." });
    }

    req.user = user;
    req.userId = user._id;
    next();
  } catch (err) {
    if (err.name === "TokenExpiredError") {
      return res.status(401).json({ error: "Token expired. Please login again." });
    }
    if (err.name === "JsonWebTokenError") {
      return res.status(401).json({ error: "Invalid token." });
    }
    return res.status(500).json({ error: "Authentication error." });
  }
};

/**
 * Optional authentication middleware
 * Does not block if no token is provided, but attaches user if valid.
 */
const optionalAuth = async (req, res, next) => {
  try {
    const authHeader = req.headers.authorization;
    if (authHeader && authHeader.startsWith("Bearer ")) {
      const token = authHeader.split(" ")[1];
      const decoded = jwt.verify(token, process.env.JWT_SECRET);
      
      const user = await User.findById(decoded.userId);
      if (user && user.isActive && decoded.tokenVersion === user.tokenVersion) {
        req.user = user;
        req.userId = user._id;
      }
    }
  } catch (err) {
    // Silently continue
  }
  next();
};

module.exports = { authenticate, optionalAuth };
