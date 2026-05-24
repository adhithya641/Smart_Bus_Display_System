import { useState, useMemo } from "react";
import { useNavigate } from "react-router-dom";
import { MapContainer, TileLayer, useMap } from "react-leaflet";
import { districts } from "../data/districtData";
import "leaflet/dist/leaflet.css";

function ChangeView({ center, zoom }) {
  const map = useMap();
  map.flyTo(center, zoom, { duration: 1.2 });
  return null;
}

export default function DistrictSelectPage() {
  const navigate = useNavigate();
  const [search, setSearch] = useState("");
  const [selected, setSelected] = useState(null);

  const filtered = useMemo(() => {
    return districts.filter((d) =>
      d.name.toLowerCase().includes(search.toLowerCase())
    );
  }, [search]);

  const mapCenter = selected ? selected.center : [10.85, 78.65];
  const mapZoom = selected ? selected.zoom : 7;

  const handleDistrictClick = (district) => {
    navigate(`/buses/${encodeURIComponent(district.name)}`);
  };

  return (
    <div className="district-page">
      <div className="district-sidebar">
        <div className="sidebar-header">
          <button className="back-btn" onClick={() => navigate("/")}>
            ← Back to Login
          </button>
          <h2>📍 Select District</h2>
          <p>Choose a Tamil Nadu district to find buses</p>
        </div>

        <div className="search-box">
          <input
            placeholder="Search districts..."
            value={search}
            onChange={(e) => setSearch(e.target.value)}
            id="district-search"
          />
        </div>

        <div className="district-list">
          {filtered.map((district) => (
            <div
              key={district.name}
              className={`district-item ${selected?.name === district.name ? "active" : ""}`}
              onMouseEnter={() => setSelected(district)}
              onClick={() => handleDistrictClick(district)}
              id={`district-${district.name.toLowerCase().replace(/\s/g, "-")}`}
            >
              <div className="district-icon">📍</div>
              <span className="district-name">{district.name}</span>
              <span className="district-arrow">→</span>
            </div>
          ))}
        </div>
      </div>

      <div className="district-map">
        {selected && (
          <div className="map-district-label">
            📍 {selected.name}
          </div>
        )}
        <MapContainer
          center={mapCenter}
          zoom={mapZoom}
          style={{ height: "100%", width: "100%" }}
          zoomControl={true}
        >
          <TileLayer
            url="https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png"
            attribution='&copy; OpenStreetMap contributors'
          />
          <ChangeView center={mapCenter} zoom={mapZoom} />
        </MapContainer>
      </div>
    </div>
  );
}
