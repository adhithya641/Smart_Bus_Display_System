import React, { createContext, useContext, useState, useEffect, useCallback } from "react";
import axios from "axios";

import { API_URL as API } from "../config";
const AuthContext = createContext(null);

/**
 * AuthProvider — JWT-based authentication context
 * Manages login, registration, token storage, and role-based access
 */
export function AuthProvider({ children }) {
  const [user, setUser] = useState(null);
  const [token, setToken] = useState(localStorage.getItem("token"));
  const [loading, setLoading] = useState(true);
  const [driverProfile, setDriverProfile] = useState(null);

  // Setup axios interceptors for auth headers and automatic 401 logout
  useEffect(() => {
    const requestInterceptor = axios.interceptors.request.use((config) => {
      const t = localStorage.getItem("token");
      if (t) config.headers.Authorization = `Bearer ${t}`;
      return config;
    });

    const responseInterceptor = axios.interceptors.response.use(
      (response) => response,
      (error) => {
        if (error.response && error.response.status === 401) {
          console.warn("🔒 Session expired or unauthorized. Logging out...");
          localStorage.removeItem("token");
          localStorage.removeItem("refreshToken");
          setToken(null);
          setUser(null);
          setDriverProfile(null);
          // Redirect to login page
          window.location.href = "/";
        }
        return Promise.reject(error);
      }
    );

    return () => {
      axios.interceptors.request.eject(requestInterceptor);
      axios.interceptors.response.eject(responseInterceptor);
    };
  }, []);

  // Auto-login on app load if token exists
  const loadUser = useCallback(async () => {
    const storedToken = localStorage.getItem("token");
    if (!storedToken) {
      setLoading(false);
      return;
    }
    try {
      const res = await axios.get(`${API}/auth/me`, {
        headers: { Authorization: `Bearer ${storedToken}` },
      });
      setUser(res.data.user);
      setDriverProfile(res.data.driver);
      setToken(storedToken);
    } catch (err) {
      // Token expired or invalid
      localStorage.removeItem("token");
      localStorage.removeItem("refreshToken");
      setToken(null);
      setUser(null);
    }
    setLoading(false);
  }, []);

  useEffect(() => { loadUser(); }, [loadUser]);

  // Login with email and password
  const login = async (email, password) => {
    const res = await axios.post(`${API}/auth/login`, { email, password });
    localStorage.setItem("token", res.data.token);
    localStorage.setItem("refreshToken", res.data.refreshToken);
    setToken(res.data.token);
    setUser(res.data.user);
    setDriverProfile(res.data.driver);
    return res.data;
  };

  // Register new passenger account
  const register = async (name, email, password, phone) => {
    const res = await axios.post(`${API}/auth/register`, { name, email, password, phone });
    localStorage.setItem("token", res.data.token);
    localStorage.setItem("refreshToken", res.data.refreshToken);
    setToken(res.data.token);
    setUser(res.data.user);
    return res.data;
  };

  // Logout
  const logout = () => {
    localStorage.removeItem("token");
    localStorage.removeItem("refreshToken");
    setToken(null);
    setUser(null);
    setDriverProfile(null);
  };

  // Check roles
  const isAdmin = user?.role === "admin";
  const isDriver = user?.role === "driver";
  const isPassenger = user?.role === "passenger";
  const isLoggedIn = !!user;
  const role = user?.role || null;

  return (
    <AuthContext.Provider value={{
      user, token, loading, driverProfile,
      login, register, logout,
      isAdmin, isDriver, isPassenger, isLoggedIn, role,
    }}>
      {children}
    </AuthContext.Provider>
  );
}

export function useAuth() {
  return useContext(AuthContext);
}
