/**
 * Central configuration for the application
 */
export const API_URL = process.env.REACT_APP_API_URL || "http://localhost:5000/api";

// Helper to construct absolute URLs if needed
export const getApiUrl = (path) => `${API_URL}${path.startsWith("/") ? path : `/${path}`}`;
