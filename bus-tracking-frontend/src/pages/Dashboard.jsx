import { useParams, useNavigate } from "react-router-dom";
import { useEffect, useState } from "react";
import io from "socket.io-client";
import { API_URL } from "../config";
import axios from "axios";
import SmartMap from "../components/SmartMap";
import "leaflet/dist/leaflet.css";

const socket = io(API_URL.replace("/api", ""));

export default function Dashboard() {
  const { busId } = useParams();
  const navigate = useNavigate();
  const [bus, setBus] = useState(null);
  const [position, setPosition] = useState(null);
  const [speed, setSpeed] = useState(0);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    const fetchBus = async () => {
      try {
        const res = await axios.get(`${API_URL}/buses/${busId}`);
        setBus(res.data);
        if (res.data.latitude && res.data.longitude) {
          setPosition([res.data.latitude, res.data.longitude]);
        } else if (res.data.stops && res.data.stops.length > 0) {
          setPosition([res.data.stops[0].lat, res.data.stops[0].lng]);
        }
      } catch (err) {
        console.error("Failed to fetch bus:", err);
      } finally {
        setLoading(false);
      }
    };
    fetchBus();
  }, [busId]);

  useEffect(() => {
    const handler = (data) => {
      if (data.busId === bus?.busId || data._id === busId) {
        setPosition([data.latitude, data.longitude]);
        if (data.speed) setSpeed(data.speed);
      }
    };
    socket.on("busLocationUpdate", handler);
    return () => socket.off("busLocationUpdate", handler);
  }, [bus, busId]);

  if (loading) {
    return (
      <div className="dashboard-page">
        <div style={{ display: "flex", alignItems: "center", justifyContent: "center", width: "100%" }}>
          <p style={{ color: "#94a3b8" }}>Loading bus data...</p>
        </div>
      </div>
    );
  }

  if (!bus) {
    return (
      <div className="dashboard-page">
        <div style={{ display: "flex", alignItems: "center", justifyContent: "center", width: "100%" }}>
          <p style={{ color: "#94a3b8" }}>Bus not found.</p>
        </div>
      </div>
    );
  }

  const stops = bus.stops || [];
  const mapCenter = position || [10.85, 78.65];

  return (
    <div className="dashboard-page">
      <div className="dashboard-sidebar">
        <div className="bus-info">
          <button className="back-btn" onClick={() => navigate(-1)} style={{
            background: "none", border: "none", color: "#94a3b8", fontSize: "14px",
            cursor: "pointer", marginBottom: "12px", fontFamily: "'Inter', sans-serif"
          }}>
            ← Back
          </button>
          <h2>🚌 {bus.name || bus.busId}</h2>
          <div className="info-row">📍 {bus.routeName || "No route"}</div>
          {bus.district && <div className="info-row">🗺️ {bus.district}</div>}
          {bus.timing && <div className="info-row">🕐 {bus.timing}</div>}
          <div className="speed-badge">⚡ {speed} km/h</div>
        </div>

        {stops.length > 0 && (
          <div className="stops-section">
            <h3>Stops</h3>
            {stops.map((stop, index) => (
              <div key={index} className="stop-item">
                <div className={`stop-dot ${index === 0 ? "first" : ""} ${index === stops.length - 1 ? "last" : ""}`} />
                <div>
                  <div className="stop-name">{stop.name}</div>
                  {stop.expectedArrival && <div className="stop-time">{stop.expectedArrival}</div>}
                </div>
              </div>
            ))}
          </div>
        )}
      </div>

      <div className="dashboard-map">
        <SmartMap
          busPosition={position || mapCenter}
          center={mapCenter}
          zoom={13}
          stops={stops}
        />
      </div>
    </div>
  );
}
