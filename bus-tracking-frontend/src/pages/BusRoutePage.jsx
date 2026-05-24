import { useState, useEffect } from "react";
import { useParams, useNavigate } from "react-router-dom";
import { MapContainer, TileLayer, Marker, Popup, useMap } from "react-leaflet";
import { districts } from "../data/districtData";
import { API_URL as API } from "../config";
import axios from "axios";
import L from "leaflet";
import "leaflet/dist/leaflet.css";

// Fix default marker icon
delete L.Icon.Default.prototype._getIconUrl;
L.Icon.Default.mergeOptions({
  iconRetinaUrl: "https://cdnjs.cloudflare.com/ajax/libs/leaflet/1.9.4/images/marker-icon-2x.png",
  iconUrl: "https://cdnjs.cloudflare.com/ajax/libs/leaflet/1.9.4/images/marker-icon.png",
  shadowUrl: "https://cdnjs.cloudflare.com/ajax/libs/leaflet/1.9.4/images/marker-shadow.png",
});

function ChangeView({ center, zoom }) {
  const map = useMap();
  map.flyTo(center, zoom, { duration: 1.2 });
  return null;
}

export default function BusRoutePage() {
  const { district } = useParams();
  const navigate = useNavigate();
  const [buses, setBuses] = useState([]);
  const [loading, setLoading] = useState(true);

  const districtInfo = districts.find(
    (d) => d.name.toLowerCase() === decodeURIComponent(district).toLowerCase()
  );

  const mapCenter = districtInfo ? districtInfo.center : [10.85, 78.65];
  const mapZoom = districtInfo ? districtInfo.zoom : 7;

  useEffect(() => {
    const fetchBuses = async () => {
      try {
        const res = await axios.get(
          `${API}/buses?district=${encodeURIComponent(decodeURIComponent(district))}`
        );
        setBuses(res.data);
      } catch (err) {
        console.error("Failed to fetch buses:", err);
      } finally {
        setLoading(false);
      }
    };
    fetchBuses();
  }, [district]);

  return (
    <div className="bus-route-page">
      <div className="bus-route-sidebar">
        <div className="sidebar-header">
          <button className="back-btn" onClick={() => navigate("/districts")}>
            ← Back to Districts
          </button>
          <h2>🚌 {decodeURIComponent(district)}</h2>
          <p>Available bus routes in this district</p>
        </div>

        <div className="bus-list">
          {loading ? (
            <div className="empty-state">
              <p>Loading buses...</p>
            </div>
          ) : buses.length === 0 ? (
            <div className="empty-state">
              <div className="empty-icon">🚌</div>
              <p>No buses registered for this district yet.<br/>Officials can add buses from the admin panel.</p>
            </div>
          ) : (
            buses.map((bus) => (
              <div
                key={bus._id}
                className="bus-card"
                onClick={() => navigate(`/dashboard/${bus._id}`)}
                id={`bus-${bus.busId}`}
              >
                <div className="bus-name">
                  🚌 {bus.name || bus.busId}
                </div>
                <div className="bus-route">{bus.routeName || "No route info"}</div>
                {bus.timing && <div className="bus-timing">🕐 {bus.timing}</div>}
              </div>
            ))
          )}
        </div>
      </div>

      <div className="bus-route-map">
        <div className="map-district-label">
          📍 {decodeURIComponent(district)}
        </div>
        <MapContainer
          center={mapCenter}
          zoom={mapZoom}
          style={{ height: "100%", width: "100%" }}
        >
          <TileLayer
            url="https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png"
            attribution='&copy; OpenStreetMap contributors'
          />
          <ChangeView center={mapCenter} zoom={mapZoom} />
          {buses.filter(b => b.latitude && b.longitude).map((bus) => (
            <Marker key={bus._id} position={[bus.latitude, bus.longitude]}>
              <Popup>
                <strong>{bus.name || bus.busId}</strong><br/>
                {bus.routeName}
              </Popup>
            </Marker>
          ))}
        </MapContainer>
      </div>
    </div>
  );
}
