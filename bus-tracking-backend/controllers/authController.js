const jwt = require("jsonwebtoken");
const User = require("../models/User");
const Driver = require("../models/Driver");

// Helper: Generate JWT access token (24h) - stores user id, email, and role as requested
function generateToken(user) {
  return jwt.sign(
    { 
      userId: user._id, 
      email: user.email, 
      role: user.role,
      tokenVersion: user.tokenVersion 
    },
    process.env.JWT_SECRET,
    { expiresIn: "24h" }
  );
}

// Helper: Generate refresh token (7 days)
function generateRefreshToken(user) {
  return jwt.sign(
    { userId: user._id },
    process.env.JWT_REFRESH_SECRET,
    { expiresIn: "7d" }
  );
}

/**
 * Register a new passenger account
 */
exports.register = async (req, res) => {
  try {
    const { name, email, password, phone } = req.body;

    const existingUser = await User.findOne({ email });
    if (existingUser) {
      return res.status(400).json({ error: "Email already registered." });
    }

    const user = new User({
      name,
      email,
      password,
      phone,
      role: "passenger",
      tokenVersion: 0
    });

    await user.save();

    const token = generateToken(user);
    const refreshToken = generateRefreshToken(user);

    res.status(201).json({
      success: true,
      token,
      refreshToken,
      user: user.toJSON(),
    });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Login user (passenger, admin, or driver)
 * Increments tokenVersion to invalidate previous device sessions
 */
exports.login = async (req, res) => {
  try {
    const { email, password } = req.body;

    if (!email || !password) {
      return res.status(400).json({ error: "Email and password are required." });
    }

    const user = await User.findOne({ email }).select("+password");
    if (!user) {
      return res.status(401).json({ error: "Invalid email or password." });
    }

    if (!user.isActive) {
      return res.status(401).json({ error: "Account is deactivated." });
    }

    const isMatch = await user.comparePassword(password);
    if (!isMatch) {
      return res.status(401).json({ error: "Invalid email or password." });
    }

    // Increment tokenVersion to invalidate other active sessions
    user.tokenVersion = (user.tokenVersion || 0) + 1;
    user.lastLogin = new Date();
    await user.save();

    const token = generateToken(user);
    const refreshToken = generateRefreshToken(user);

    let driverProfile = null;
    if (user.role === "driver") {
      driverProfile = await Driver.findOne({ user: user._id }).populate("assignedBus");
    }

    res.json({
      success: true,
      token,
      refreshToken,
      user: user.toJSON(),
      driver: driverProfile,
    });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Refresh access token
 */
exports.refresh = async (req, res) => {
  try {
    const { refreshToken } = req.body;

    if (!refreshToken) {
      return res.status(400).json({ error: "Refresh token is required." });
    }

    const decoded = jwt.verify(refreshToken, process.env.JWT_REFRESH_SECRET);
    const user = await User.findById(decoded.userId);

    if (!user || !user.isActive) {
      return res.status(401).json({ error: "Invalid refresh token." });
    }

    const token = generateToken(user);
    res.json({ success: true, token });
  } catch (err) {
    res.status(401).json({ error: "Invalid or expired refresh token." });
  }
};

/**
 * Get current user profile
 */
exports.getMe = async (req, res) => {
  try {
    const user = await User.findById(req.userId).populate("favoriteRoutes");
    if (!user) {
      return res.status(404).json({ error: "User not found" });
    }

    let driverProfile = null;
    if (user.role === "driver") {
      driverProfile = await Driver.findOne({ user: user._id }).populate("assignedBus");
    }

    res.json({
      user: user.toJSON(),
      driver: driverProfile,
    });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};

/**
 * Toggle favorite route for passenger
 */
exports.toggleFavorite = async (req, res) => {
  try {
    const user = await User.findById(req.userId);
    if (!user) return res.status(404).json({ error: "User not found" });

    const routeId = req.params.routeId;
    const index = user.favoriteRoutes.indexOf(routeId);
    if (index > -1) {
      user.favoriteRoutes.splice(index, 1);
    } else {
      user.favoriteRoutes.push(routeId);
    }

    await user.save();
    res.json({ success: true, favorites: user.favoriteRoutes });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
};
