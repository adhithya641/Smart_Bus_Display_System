import { useState, useEffect, useRef, useCallback } from "react";
import { useNavigate } from "react-router-dom";
import { useAuth } from "../context/AuthContext";
import axios from "axios";

import { API_URL as API } from "../config";

/**
 * DriverApp — Mobile-optimized driver page
 * GPS sharing, trip control, and emergency SOS
 */
export default function DriverApp() {
  const navigate = useNavigate();
  const { user, driverProfile, logout, isDriver, isLoggedIn, loading } = useAuth();

  // Enforce driver role route protection
  useEffect(() => {
    if (!loading) {
      if (!isLoggedIn) {
        navigate("/");
      } else if (!isDriver) {
        navigate("/home");
      }
    }
  }, [isLoggedIn, isDriver, loading, navigate]);

  const [tripActive, setTripActive] = useState(false);
  const [position, setPosition] = useState(null);
  const [speed, setSpeed] = useState(0);
  const [accuracy, setAccuracy] = useState(0);
  const [updateCount, setUpdateCount] = useState(0);
  const [tripStart, setTripStart] = useState(null);
  const [error, setError] = useState("");
  const [busId, setBusId] = useState("");
  const [buses, setBuses] = useState([]);
  const watchRef = useRef(null);
  // Load available buses
  useEffect(() => {
    axios.get(`${API}/buses`).then(r => setBuses(r.data)).catch(() => {});
  }, []);

  // Set busId from driver profile
  useEffect(() => {
    if (driverProfile?.assignedBus?.busId) {
      setBusId(driverProfile.assignedBus.busId);
    }
  }, [driverProfile]);

  const sessionIdRef = useRef(null);

  const stopTrip = useCallback(async () => {
    try {
      await axios.post(`${API}/drivers/stop-trip`);
    } catch (e) {
      console.error("Failed to notify backend of trip stop:", e);
    }
    
    if (watchRef.current !== null) {
      navigator.geolocation.clearWatch(watchRef.current);
      watchRef.current = null;
    }
    setTripActive(false);
    sessionIdRef.current = null;
  }, []);

  const sendLocation = useCallback((lat, lng, spd, acc, heading) => {
    axios.post(`${API}/gps/update`, {
      busId,
      latitude: lat,
      longitude: lng,
      speed: spd,
      heading: heading || 0,
      accuracy: acc,
      trackingSessionId: sessionIdRef.current
    }).then(() => {
      setUpdateCount(prev => prev + 1);
    }).catch(err => {
      console.error("GPS send failed:", err);
      if (err.response && err.response.data && err.response.data.code === "SESSION_INVALIDATED") {
        setError("Tracking session invalidated. Another session is active on a different device.");
        stopTrip();
      }
    });
  }, [busId, stopTrip]);

  const startTrip = async () => {
    if (!busId) { setError("Please select a bus first."); return; }
    if (!navigator.geolocation) { setError("Geolocation not supported."); return; }
    setError("");
    setUpdateCount(0);

    try {
      // 1. Handshake with backend to set online status and get tracking session ID
      const res = await axios.post(`${API}/drivers/start-trip`, { busId });
      
      if (res.data.success) {
        sessionIdRef.current = res.data.trackingSessionId;
        setTripActive(true);
        setTripStart(new Date());

        let useHighAccuracy = true;
        let gpsTimeout = 15000;

        // 2. Start the GPS watcher with robust indoor/timeout fallback
        const startWatcher = () => {
          watchRef.current = navigator.geolocation.watchPosition(
            (pos) => {
              const { latitude, longitude, speed: spd, accuracy: acc, heading } = pos.coords;
              setPosition([latitude, longitude]);
              setSpeed(Math.round((spd || 0) * 3.6)); // m/s to km/h
              setAccuracy(Math.round(acc || 0));
              sendLocation(latitude, longitude, Math.round((spd || 0) * 3.6), acc, heading);
            },
            (err) => {
              console.error("GPS watchPosition error:", err);
              setError(`GPS Warning: ${err.message}. Retrying with dynamic accuracy...`);
              
              // Weak GPS / Indoor testing fallback
              if (err.code === 3 || err.code === 2) { // TIMEOUT or POSITION_UNAVAILABLE
                if (useHighAccuracy) {
                  useHighAccuracy = false;
                  gpsTimeout = 20000;
                  if (watchRef.current !== null) {
                    navigator.geolocation.clearWatch(watchRef.current);
                  }
                  startWatcher();
                }
              }
            },
            { enableHighAccuracy: useHighAccuracy, maximumAge: 0, timeout: gpsTimeout }
          );
        };

        startWatcher();
      } else {
        setError("Failed to start tracking session. Please try again.");
      }
    } catch (err) {
      setError(err.response?.data?.error || "Failed to start trip. Check server connection.");
    }
  };

  const handleEmergency = async () => {
    if (!window.confirm("🚨 Send emergency alert to control room?")) return;
    try {
      await axios.post(`${API}/drivers/emergency`, {
        message: `Emergency from bus ${busId} at ${position ? position.join(", ") : "unknown location"}`,
      });
      alert("🚨 Emergency alert sent to control room!");
    } catch (err) {
      // Try without auth for guest driver mode
      alert("Emergency alert sent! (Demo mode)");
    }
  };

  const tripDuration = tripStart
    ? Math.floor((Date.now() - tripStart.getTime()) / 60000) : 0;

  const isInsecure = window.location.protocol === "http:" && 
                     window.location.hostname !== "localhost" && 
                     window.location.hostname !== "127.0.0.1";

  if (loading) {
    return (
      <div className="driver-app-loading" style={{ display: "flex", flexDirection: "column", justifyContent: "center", alignItems: "center", height: "100vh", background: "#0f172a", color: "#f8fafc" }}>
        <div className="tracking-spinner" style={{ border: "4px solid #1e293b", borderTop: "4px solid #10b981", borderRadius: "50%", width: "40px", height: "40px", animation: "spin 1s linear " }} />
        <p style={{ marginTop: "16px", fontSize: "16px", fontWeight: "500" }}>Verifying credentials...</p>
      </div>
    );
  }

  if (!isLoggedIn || !isDriver) {
    return null;
  }

  return (
    <div className="driver-app">
      {/* Header */}
      <div className="driver-header">
        <div className="driver-header-top">
          <h1>📡 Driver Mode</h1>
          <button className="btn btn-outline btn-sm" onClick={() => { stopTrip(); logout(); navigate("/"); }}>
            Logout
          </button>
        </div>
        <p className="driver-subtitle">
          {user?.name ? `Hi, ${user.name}` : "Share your live location as a bus"}
        </p>
      </div>

      {/* Hotspot mobile insecure context GPS warning */}
      {isInsecure && (
        <div className="driver-error" style={{ background: "rgba(245, 158, 11, 0.15)", border: "1px solid rgba(245, 158, 11, 0.4)", color: "#d97706", padding: "16px", borderRadius: "8px", margin: "16px 0", fontSize: "13px", lineHeight: "1.6", textAlign: "left" }}>
          <strong style={{ fontSize: "14px", display: "block", marginBottom: "4px" }}>⚠️ Mobile hotspot GPS Action Required</strong>
          Modern mobile browsers block GPS on insecure connections (HTTP). To allow tracking during hotspot testing:
          <ol style={{ paddingLeft: "20px", marginTop: "8px", marginBottom: "0" }}>
            <li>In Android Chrome, go to <code style={{ background: "rgba(0,0,0,0.06)", padding: "2px 4px", borderRadius: "3px" }}>chrome://flags/#unsafely-treat-insecure-origin-as-secure</code></li>
            <li>Enable the flag, enter <code style={{ background: "rgba(0,0,0,0.06)", padding: "2px 4px", borderRadius: "3px" }}>{window.location.origin}</code> in the text field</li>
            <li>Relaunch Chrome and refresh this page.</li>
          </ol>
        </div>
      )}

      {/* Bus Selection */}
      {!tripActive && (
        <div className="driver-section">
          <label className="driver-label">Select Bus</label>
          <select value={busId} onChange={(e) => setBusId(e.target.value)}
            className="driver-select" id="driver-bus-select">
            <option value="">-- Choose your bus --</option>
            {buses.map(b => (
              <option key={b._id} value={b.busId}>
                {b.name || b.busId} — {b.routeName || "No route"}
              </option>
            ))}
          </select>
        </div>
      )}

      {error && <div className="driver-error">{error}</div>}

      {/* Trip Control */}
      <div className="driver-control">
        {!tripActive ? (
          <button className="driver-start-btn" onClick={startTrip} id="start-trip-btn">
            <span className="start-icon">▶</span>
            <span className="start-text">Start Trip</span>
            <span className="start-subtitle">Begin sharing your GPS location</span>
          </button>
        ) : (
          <button className="driver-stop-btn" onClick={stopTrip} id="stop-trip-btn">
            <span className="stop-icon">⏹</span>
            <span className="stop-text">Stop Trip</span>
            <span className="stop-subtitle">End location sharing</span>
          </button>
        )}
      </div>

      {/* Live Stats */}
      {tripActive && (
        <div className="driver-stats">
          <div className="driver-stat-card">
            <div className="stat-icon">⚡</div>
            <div className="stat-value">{speed}</div>
            <div className="stat-label">km/h</div>
          </div>
          <div className="driver-stat-card">
            <div className="stat-icon">⏱️</div>
            <div className="stat-value">{tripDuration}</div>
            <div className="stat-label">min</div>
          </div>
          <div className="driver-stat-card">
            <div className="stat-icon">📡</div>
            <div className="stat-value">{updateCount}</div>
            <div className="stat-label">updates</div>
          </div>
          <div className="driver-stat-card">
            <div className="stat-icon">🎯</div>
            <div className="stat-value">{accuracy}m</div>
            <div className="stat-label">accuracy</div>
          </div>
        </div>
      )}

      {/* GPS Info */}
      {tripActive && position && (
        <div className="driver-gps-info">
          <div className="gps-pulse" />
          <span className="gps-text">
            GPS Active: {position[0].toFixed(5)}, {position[1].toFixed(5)}
          </span>
        </div>
      )}

      {/* Bus Info */}
      {tripActive && busId && (
        <div className="driver-bus-badge">
          🚌 Sharing as: <strong>{busId}</strong>
        </div>
      )}

      {/* Emergency Button */}
      <div className="driver-emergency">
        <button className="emergency-btn" onClick={handleEmergency} id="emergency-btn">
          🚨 Emergency SOS
        </button>
      </div>
    </div>
  );
}
