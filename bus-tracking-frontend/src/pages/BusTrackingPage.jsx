import { useParams, useNavigate } from "react-router-dom";
import { useEffect, useState, useRef } from "react";
import { MapContainer, TileLayer, Marker, Popup, Polyline, CircleMarker, useMap } from "react-leaflet";
import axios from "axios";
import L from "leaflet";
import StopTimeline from "../components/StopTimeline";
import { useSocket } from "../hooks/useSocket";
import "leaflet/dist/leaflet.css";
import { API_URL as API } from "../config";

delete L.Icon.Default.prototype._getIconUrl;
L.Icon.Default.mergeOptions({
  iconRetinaUrl: "https://cdnjs.cloudflare.com/ajax/libs/leaflet/1.9.4/images/marker-icon-2x.png",
  iconUrl: "https://cdnjs.cloudflare.com/ajax/libs/leaflet/1.9.4/images/marker-icon.png",
  shadowUrl: "https://cdnjs.cloudflare.com/ajax/libs/leaflet/1.9.4/images/marker-shadow.png",
});

const busIcon = new L.DivIcon({
  className: "",
  html: '<div style="font-size:36px;filter:drop-shadow(0 3px 6px rgba(0,0,0,0.5));transition:transform 0.5s ease">🚌</div>',
  iconSize: [40, 40], iconAnchor: [20, 20],
});


function SmoothFlyTo({ center, zoom }) {
  const map = useMap();
  const initial = useRef(true);
  useEffect(() => {
    if (initial.current) { map.setView(center, zoom); initial.current = false; }
    else map.flyTo(center, zoom, { duration: 1.5 });
  }, [center, zoom, map]);
  return null;
}

export default function BusTrackingPage() {
  const { busId } = useParams();
  const navigate = useNavigate();
  const [bus, setBus] = useState(null);
  const [position, setPosition] = useState(null);
  const [speed, setSpeed] = useState(0);
  const [loading, setLoading] = useState(true);
  const [autoFollow, setAutoFollow] = useState(true);

  // Fetch bus data
  useEffect(() => {
    const fetchBus = async () => {
      try {
        const res = await axios.get(`${API}/buses/${busId}`);
        setBus(res.data);
        if (res.data.latitude && res.data.longitude) {
          setPosition([res.data.latitude, res.data.longitude]);
        } else if (res.data.stops?.length > 0) {
          const s = res.data.stops[0];
          setPosition([s.lat, s.lng]);
        }
        setSpeed(res.data.currentSpeed || 0);
      } catch (err) { console.error(err); }
      setLoading(false);
    };
    fetchBus();
  }, [busId]);

  // Live updates via Socket.IO
  useSocket("busLocationUpdate", (data) => {
    if (data.busId === bus?.busId || data._id === busId) {
      setPosition([data.latitude, data.longitude]);
      setSpeed(data.speed || 0);
    }
  });

  if (loading) {
    return (
      <div className="tracking-page">
        <div className="tracking-loading">
          <div className="tracking-spinner" />
          <p>Loading bus data...</p>
        </div>
      </div>
    );
  }

  if (!bus) {
    return (
      <div className="tracking-page">
        <div className="tracking-loading">
          <p>Bus not found.</p>
          <button className="btn btn-outline" onClick={() => navigate(-1)}>← Go Back</button>
        </div>
      </div>
    );
  }

  const stops = bus.stops || [];
  const routeCoords = stops.filter(s => s.lat && s.lng).map(s => [s.lat, s.lng]);
  const mapCenter = autoFollow && position ? position : (routeCoords[0] || [10.85, 78.65]);
  const isLive = bus.isOnTrip && position;

  const statusText = isLive
    ? (speed > 30 ? "Moving" : speed > 0 ? "Slow" : "Stopped")
    : "Offline";
  const statusClass = isLive
    ? (speed > 30 ? "moving" : speed > 0 ? "slow" : "stopped")
    : "offline";

  return (
    <div className="tracking-page">
      {/* Info Sidebar */}
      <div className="tracking-sidebar">
        <button className="back-btn" onClick={() => navigate(-1)}>← Back</button>

        <div className="tracking-bus-info">
          <div className="tracking-bus-header">
            <h2>🚌 {bus.name || bus.busId}</h2>
            <div className={`tracking-status ${statusClass}`}>
              <span className="status-dot" />
              {statusText}
            </div>
          </div>

          <div className="tracking-details">
            {bus.routeName && <div className="info-row">📍 {bus.routeName}</div>}
            {bus.district && <div className="info-row">🗺️ {bus.district}</div>}
            {bus.timing && <div className="info-row">🕐 {bus.timing}</div>}
            {bus.type && <div className="info-row">🚌 {bus.type.toUpperCase()}</div>}
          </div>

          {isLive && (
            <div className="tracking-live-stats">
              <div className="live-stat">
                <span className="live-stat-value">{Math.round(speed)}</span>
                <span className="live-stat-label">km/h</span>
              </div>
              <div className="live-stat">
                <span className="live-stat-value">{position[0].toFixed(4)}</span>
                <span className="live-stat-label">Lat</span>
              </div>
              <div className="live-stat">
                <span className="live-stat-value">{position[1].toFixed(4)}</span>
                <span className="live-stat-label">Lng</span>
              </div>
            </div>
          )}

          <button className={`btn ${autoFollow ? "btn-primary" : "btn-outline"} btn-block`}
            onClick={() => setAutoFollow(!autoFollow)} id="auto-follow-toggle">
            {autoFollow ? "📍 Auto-following bus" : "📍 Enable auto-follow"}
          </button>
        </div>

        <StopTimeline stops={stops} />
      </div>

      {/* Full-screen Map */}
      <div className="tracking-map">
        {isLive && (
          <div className="map-district-label">
            📡 Live Tracking: {bus.name || bus.busId} — ⚡ {Math.round(speed)} km/h
          </div>
        )}
        <MapContainer center={mapCenter} zoom={14} style={{ height: "100%", width: "100%" }}>
          <TileLayer url="https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png"
            attribution='&copy; OpenStreetMap contributors' />
          {autoFollow && <SmoothFlyTo center={mapCenter} zoom={14} />}

          {/* Route polyline */}
          {routeCoords.length > 1 && (
            <Polyline positions={routeCoords} color="#3b82f6" weight={4} opacity={0.7} dashArray="10 6" />
          )}

          {/* Stop markers */}
          {stops.filter(s => s.lat && s.lng).map((stop, i) => (
            <CircleMarker key={i} center={[stop.lat, stop.lng]} radius={8}
              fillColor={i === 0 ? "#10b981" : i === stops.length - 1 ? "#ef4444" : "#3b82f6"}
              fillOpacity={0.9} color="white" weight={2}>
              <Popup>{stop.name}</Popup>
            </CircleMarker>
          ))}

          {/* Bus marker */}
          {position && (
            <Marker position={position} icon={busIcon}>
              <Popup>
                <strong>🚌 {bus.name || bus.busId}</strong><br />
                ⚡ {Math.round(speed)} km/h
              </Popup>
            </Marker>
          )}
        </MapContainer>
      </div>
    </div>
  );
}
