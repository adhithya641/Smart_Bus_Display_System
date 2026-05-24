import { useState, useEffect } from "react";
import { useNavigate } from "react-router-dom";
import { MapContainer, TileLayer, Marker, Popup, useMap } from "react-leaflet";
import axios from "axios";
import L from "leaflet";
import SearchBar from "../components/SearchBar";
import BusCard from "../components/BusCard";
import { SkeletonList } from "../components/LoadingSkeleton";
import { useAllBusUpdates } from "../hooks/useSocket";
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
  html: '<div style="font-size:28px;filter:drop-shadow(0 2px 4px rgba(0,0,0,0.5))">🚌</div>',
  iconSize: [32, 32], iconAnchor: [16, 16],
});


function FlyTo({ center, zoom }) {
  const map = useMap();
  map.flyTo(center, zoom, { duration: 1.2 });
  return null;
}

export default function PassengerDashboard() {
  const navigate = useNavigate();
  const [searchResults, setSearchResults] = useState(null);
  const [liveBuses, setLiveBuses] = useState([]);
  const [alerts, setAlerts] = useState([]);
  const [loading, setLoading] = useState(false);
  const [searched, setSearched] = useState(false);
  const [mapCenter, setMapCenter] = useState([10.85, 78.65]);
  const [mapZoom, setMapZoom] = useState(7);

  // Load live buses and alerts on mount
  useEffect(() => {
    const loadData = async () => {
      try {
        const [busRes, alertRes] = await Promise.all([
          axios.get(`${API}/buses/live`).catch(() => ({ data: [] })),
          axios.get(`${API}/alerts/active`).catch(() => ({ data: [] })),
        ]);
        setLiveBuses(busRes.data);
        setAlerts(alertRes.data);
      } catch (err) { console.error(err); }
    };
    loadData();
  }, []);

  // Live bus updates via Socket.IO
  useAllBusUpdates((data) => {
    setLiveBuses(prev => {
      const idx = prev.findIndex(b => b.busId === data.busId);
      if (idx >= 0) {
        const updated = [...prev];
        updated[idx] = { ...updated[idx], ...data };
        return updated;
      }
      return [...prev, data];
    });
  });

  const handleSearch = async ({ from, to }) => {
    setLoading(true);
    setSearched(true);
    try {
      const params = new URLSearchParams();
      if (from) params.append("from", from);
      if (to) params.append("to", to);
      const res = await axios.get(`${API}/routes/search?${params.toString()}`);
      setSearchResults(res.data);
      if (res.data.length > 0 && res.data[0].stops?.length > 0) {
        const firstStop = res.data[0].stops[0]?.stop;
        if (firstStop?.location) {
          setMapCenter([firstStop.location.coordinates[1], firstStop.location.coordinates[0]]);
          setMapZoom(12);
        }
      }
    } catch (err) {
      console.error(err);
      setSearchResults([]);
    }
    setLoading(false);
  };

  return (
    <div className="passenger-dashboard">
      {/* Sidebar */}
      <div className="passenger-sidebar">
        <div className="sidebar-header">
          <h2>🚌 Find Your Bus</h2>
          <p>Search routes across Tamil Nadu</p>
        </div>

        <SearchBar onSearch={handleSearch} loading={loading} />

        {/* Active Alerts */}
        {alerts.length > 0 && (
          <div className="alerts-section">
            {alerts.slice(0, 3).map(alert => (
              <div key={alert._id} className={`alert-badge alert-${alert.severity}`}>
                {alert.severity === "critical" ? "🚨" : alert.severity === "warning" ? "⚠️" : "ℹ️"}
                {alert.title}
              </div>
            ))}
          </div>
        )}

        {/* Search Results */}
        <div className="passenger-results">
          {loading ? (
            <SkeletonList count={4} />
          ) : searched && searchResults !== null ? (
            searchResults.length === 0 ? (
              <div className="empty-state">
                <div className="empty-icon">🔍</div>
                <p>No routes found. Try different search terms.</p>
              </div>
            ) : (
              <>
                <div className="results-header">
                  <span>{searchResults.length} route{searchResults.length !== 1 ? "s" : ""} found</span>
                </div>
                {searchResults.map(route => (
                  <div key={route._id} className="route-result-card">
                    <div className="route-result-header">
                      <span className="route-number">{route.routeNumber}</span>
                      <span className="route-name">{route.name}</span>
                      {route.type && <span className={`bus-type-badge ${route.type}`}>{route.type}</span>}
                    </div>
                    <div className="route-result-path">
                      <span className="route-origin">{route.origin}</span>
                      <span className="route-arrow">→</span>
                      <span className="route-dest">{route.destination}</span>
                    </div>
                    <div className="route-result-info">
                      {route.totalDistance > 0 && <span>📏 {route.totalDistance} km</span>}
                      {route.estimatedDuration > 0 && <span>⏱️ {route.estimatedDuration} min</span>}
                      {route.fare && <span>💰 ₹{route.fare.base}</span>}
                    </div>
                    {route.activeBuses?.length > 0 && (
                      <div className="route-buses">
                        {route.activeBuses.map(bus => (
                          <BusCard key={bus._id} bus={bus}
                            onClick={() => navigate(`/track/${bus._id}`)} />
                        ))}
                      </div>
                    )}
                    {(!route.activeBuses || route.activeBuses.length === 0) && (
                      <div className="no-active-buses">No active buses on this route right now</div>
                    )}
                  </div>
                ))}
              </>
            )
          ) : (
            /* Default: show live buses */
            <>
              <div className="results-header">
                <span>🟢 {liveBuses.length} bus{liveBuses.length !== 1 ? "es" : ""} live</span>
              </div>
              {liveBuses.length === 0 ? (
                <div className="empty-state">
                  <div className="empty-icon">📡</div>
                  <p>No buses are currently live. Use the search bar to find routes.</p>
                </div>
              ) : (
                liveBuses.map(bus => (
                  <BusCard key={bus._id || bus.busId} bus={bus}
                    onClick={() => navigate(`/track/${bus._id}`)} showETA={false} />
                ))
              )}
            </>
          )}
        </div>

        {/* Quick Actions */}
        <div className="quick-actions">
          <button className="quick-action-btn" onClick={() => navigate("/districts")} id="quick-districts">
            📍 Browse Districts
          </button>
          <button className="quick-action-btn" onClick={() => navigate("/favorites")} id="quick-favorites">
            ⭐ Favorites
          </button>
        </div>
      </div>

      {/* Map */}
      <div className="passenger-map">
        <MapContainer center={mapCenter} zoom={mapZoom} style={{ height: "100%", width: "100%" }}>
          <TileLayer url="https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png"
            attribution='&copy; OpenStreetMap contributors' />
          <FlyTo center={mapCenter} zoom={mapZoom} />
          {liveBuses.filter(b => b.latitude && b.longitude).map(bus => (
            <Marker key={bus._id || bus.busId} position={[bus.latitude, bus.longitude]} icon={busIcon}>
              <Popup>
                <strong>{bus.name || bus.busId}</strong><br />
                {bus.routeName && <>{bus.routeName}<br /></>}
                {bus.currentSpeed > 0 && <>⚡ {Math.round(bus.currentSpeed)} km/h</>}
              </Popup>
            </Marker>
          ))}
        </MapContainer>
      </div>
    </div>
  );
}
