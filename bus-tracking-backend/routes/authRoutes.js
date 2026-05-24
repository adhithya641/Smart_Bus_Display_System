const express = require("express");
const router = express.Router();
const authController = require("../controllers/authController");
const { authenticate } = require("../middleware/auth");

/**
 * Auth Routes
 * Handles user login, registration, refresh, profile loading, and favorites toggling
 */

// POST /api/auth/register - Register new passenger account
router.post("/register", authController.register);

// POST /api/auth/login - Login for passenger, admin, and driver
router.post("/login", authController.login);

// POST /api/auth/refresh - Refresh access token
router.post("/refresh", authController.refresh);

// GET /api/auth/me - Get current authenticated user profile
router.get("/me", authenticate, authController.getMe);

// PUT /api/auth/favorites/:routeId - Toggle favorite route
router.put("/favorites/:routeId", authenticate, authController.toggleFavorite);

module.exports = router;
