import { useState, useEffect, useCallback, useRef } from "react";
import { useNavigate } from "react-router-dom";
import { MapContainer, TileLayer, Marker, Popup, useMap } from "react-leaflet";
import { districts } from "../data/districtData";
import axios from "axios";
import L from "leaflet";
import StatsCard from "../components/StatsCard";
import { useAllBusUpdates } from "../hooks/useSocket";
import { showToast } from "../components/Toast";
import { useAuth } from "../context/AuthContext";
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
  html: '<div style="font-size:26px;filter:drop-shadow(0 2px 4px rgba(0,0,0,0.5))">🚌</div>',
  iconSize: [30, 30], iconAnchor: [15, 15],
});

function FlyTo({ center, zoom }) {
  const map = useMap();
  map.flyTo(center, zoom, { duration: 1 });
  return null;
}

export default function OfficialDashboard() {
  const navigate = useNavigate();
  const { isAdmin, isLoggedIn, loading } = useAuth();
  const [tab, setTab] = useState("overview");

  // Enforce administrative role protection
  useEffect(() => {
    if (!loading) {
      if (!isLoggedIn) {
        showToast("Authentication required. Please log in.", "error");
        navigate("/");
      } else if (!isAdmin) {
        showToast("Access denied. Admin role required.", "error");
        navigate("/home");
      }
    }
  }, [isLoggedIn, isAdmin, loading, navigate]);
  const [buses, setBuses] = useState([]);
  const [stats, setStats] = useState({ buses: {}, routes: {}, drivers: {}, alerts: {} });
  const [showModal, setShowModal] = useState(false);
  const [editingBus, setEditingBus] = useState(null);
  const [form, setForm] = useState({ busId: "", name: "", routeName: "", district: "", timing: "", type: "ordinary", capacity: 50 });
  const [alerts, setAlerts] = useState([]);
  const [alertForm, setAlertForm] = useState({ title: "", message: "", type: "info", severity: "info" });
  const [showAlertModal, setShowAlertModal] = useState(false);

  // Driver mode
  const [driverActive, setDriverActive] = useState(false);
  const [selectedBusForDriver, setSelectedBusForDriver] = useState("");
  const [driverPosition, setDriverPosition] = useState(null);
  const watchIdRef = useRef(null);

  const fetchBuses = useCallback(async () => {
    try { setBuses((await axios.get(`${API}/buses`)).data); } catch (e) { console.error(e); }
  }, []);

  const fetchStats = useCallback(async () => {
    try { setStats((await axios.get(`${API}/buses/stats`)).data); } catch (e) {}
  }, []);

  const fetchAlerts = useCallback(async () => {
    try { setAlerts((await axios.get(`${API}/alerts?active=true`)).data); } catch (e) {}
  }, []);

  useEffect(() => { fetchBuses(); fetchStats(); fetchAlerts(); }, [fetchBuses, fetchStats, fetchAlerts]);

  // Live bus updates
  useAllBusUpdates((data) => {
    setBuses(prev => {
      const idx = prev.findIndex(b => b.busId === data.busId);
      if (idx >= 0) { const u = [...prev]; u[idx] = { ...u[idx], ...data }; return u; }
      return prev;
    });
  });

  // Driver geolocation simulation
  useEffect(() => {
    if (driverActive && selectedBusForDriver) {
      if (!navigator.geolocation) { alert("Geolocation not supported."); setDriverActive(false); return; }
      watchIdRef.current = navigator.geolocation.watchPosition(
        (pos) => {
          const { latitude, longitude } = pos.coords;
          setDriverPosition([latitude, longitude]);
          axios.post(`${API}/gps/update`, { busId: selectedBusForDriver, latitude, longitude })
            .catch(e => console.error(e));
        },
        (e) => { alert("Location error: " + e.message); setDriverActive(false); },
        { enableHighAccuracy: true, maximumAge: 3000, timeout: 10000 }
      );
    }
    return () => { 
      if (watchIdRef.current !== null) { 
        navigator.geolocation.clearWatch(watchIdRef.current); 
        watchIdRef.current = null; 
        
        // Ensure simulation stop sets the bus offline in the database
        const busObj = buses.find(b => b.busId === selectedBusForDriver);
        if (busObj) {
          axios.put(`${API}/buses/${busObj._id}`, { isOnTrip: false, currentSpeed: 0 })
            .then(() => {
              fetchBuses();
              fetchStats();
            })
            .catch(e => console.error(e));
        }
      } 
    };
  }, [driverActive, selectedBusForDriver, buses, fetchBuses, fetchStats]);

  const handleSaveBus = async () => {
    try {
      if (editingBus) await axios.put(`${API}/buses/${editingBus._id}`, form);
      else await axios.post(`${API}/buses`, form);
      setShowModal(false); fetchBuses(); fetchStats();
      showToast(editingBus ? "Bus updated!" : "Bus created!", "success");
    } catch (e) {
      const msg = e.response?.data?.error || e.message;
      alert("Failed to save bus: " + msg);
    }
  };

  const handleDeleteBus = async (id) => {
    if (!window.confirm("Delete this bus?")) return;
    try { await axios.delete(`${API}/buses/${id}`); fetchBuses(); fetchStats(); showToast("Bus deleted.", "info"); }
    catch (e) { console.error(e); }
  };

  const handleForceOffline = async (busObj) => {
    if (!window.confirm(`Force stop tracking for bus ${busObj.name || busObj.busId}?`)) return;
    try {
      await axios.put(`${API}/buses/${busObj._id}`, { isOnTrip: false, currentSpeed: 0 });
      showToast("Bus tracking stopped successfully.", "info");
      fetchBuses();
      fetchStats();
    } catch (e) {
      alert("Failed to stop tracking: " + e.message);
    }
  };

  const handleSaveAlert = async () => {
    try {
      await axios.post(`${API}/alerts`, { ...alertForm, isActive: true, expiresAt: new Date(Date.now() + 24 * 60 * 60 * 1000) });
      setShowAlertModal(false); fetchAlerts(); showToast("Alert created!", "success");
      setAlertForm({ title: "", message: "", type: "info", severity: "info" });
    } catch (e) { alert("Failed to create alert."); }
  };

  const openAddBus = () => {
    setEditingBus(null);
    setForm({ busId: "", name: "", routeName: "", district: "", timing: "", type: "ordinary", capacity: 50 });
    setShowModal(true);
  };

  const openEditBus = (bus) => {
    setEditingBus(bus);
    setForm({ busId: bus.busId || "", name: bus.name || "", routeName: bus.routeName || "", district: bus.district || "", timing: bus.timing || "", type: bus.type || "ordinary", capacity: bus.capacity || 50 });
    setShowModal(true);
  };

  const mapCenter = driverPosition || [10.85, 78.65];
  const mapZoom = driverPosition ? 15 : 7;
  const liveBuses = buses.filter(b => b.latitude && b.longitude);

  const tabs = [
    { id: "overview", label: "📊 Overview" },
    { id: "buses", label: "🚌 Buses" },
    { id: "alerts", label: "🔔 Alerts" },
    { id: "map", label: "🗺️ Live Map" },
  ];

  if (loading) {
    return (
      <div className="official-page-loading" style={{ display: "flex", flexDirection: "column", justifyContent: "center", alignItems: "center", height: "100vh", background: "#0f172a", color: "#f8fafc" }}>
        <div className="tracking-spinner" style={{ border: "4px solid #1e293b", borderTop: "4px solid #3b82f6", borderRadius: "50%", width: "40px", height: "40px", animation: "spin 1s linear infinite" }} />
        <p style={{ marginTop: "16px", fontSize: "16px", fontWeight: "500" }}>Verifying credentials...</p>
      </div>
    );
  }

  if (!isLoggedIn || !isAdmin) {
    return null;
  }

  return (
    <div className="official-page">
      {/* Top Bar */}
      <div className="official-topbar">
        <h1>🛠️ Admin Dashboard</h1>
        <div className="topbar-actions">
          <div className="driver-select-group">
            <select value={selectedBusForDriver} onChange={(e) => setSelectedBusForDriver(e.target.value)} id="driver-bus-select">
              <option value="">Driver mode bus</option>
              {buses.map(b => (<option key={b._id} value={b.busId}>{b.name || b.busId}</option>))}
            </select>
          </div>
          <button className={`driver-toggle ${driverActive ? "active" : ""}`}
            onClick={() => { if (!driverActive && !selectedBusForDriver) { alert("Select a bus first."); return; } setDriverActive(!driverActive); }}
            id="driver-mode-toggle">
            <span className="toggle-dot" />
            {driverActive ? "📡 Sharing" : "🔌 Driver"}
          </button>
          <button className="btn btn-outline" onClick={() => navigate("/")} id="logout-btn">← Logout</button>
        </div>
      </div>

      {/* Tabs */}
      <div className="admin-tabs">
        {tabs.map(t => (
          <button key={t.id} className={`admin-tab ${tab === t.id ? "active" : ""}`}
            onClick={() => setTab(t.id)}>{t.label}</button>
        ))}
      </div>

      {/* Tab Content */}
      <div className="official-content">
        {tab === "overview" && (
          <div className="admin-overview">
            <div className="stats-grid">
              <StatsCard icon="🚌" label="Total Buses" value={stats.total || buses.length} color="blue" />
              <StatsCard icon="✅" label="Active" value={stats.active || 0} color="green" />
              <StatsCard icon="📡" label="On Trip" value={stats.onTrip || liveBuses.length} color="cyan" />
              <StatsCard icon="🔧" label="Maintenance" value={stats.maintenance || 0} color="orange" />
              <StatsCard icon="🔔" label="Active Alerts" value={alerts.length} color="red" />
              <StatsCard icon="🗺️" label="Districts" value={districts.length} color="purple" />
            </div>
            <div className="admin-overview-map">
              <h3>🗺️ Live Fleet Overview</h3>
              <div className="overview-map-container">
                <MapContainer center={[10.85, 78.65]} zoom={7} style={{ height: "100%", width: "100%" }}>
                  <TileLayer url="https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png" attribution='&copy; OSM' />
                  {liveBuses.map(bus => (
                    <Marker key={bus._id} position={[bus.latitude, bus.longitude]} icon={busIcon}>
                      <Popup><strong>{bus.name || bus.busId}</strong><br />{bus.routeName}</Popup>
                    </Marker>
                  ))}
                </MapContainer>
              </div>
            </div>
          </div>
        )}

        {tab === "buses" && (
          <div className="admin-buses">
            <div className="admin-section-header">
              <h2>All Buses ({buses.length})</h2>
              <button className="btn btn-primary" onClick={openAddBus} id="add-bus-btn">+ Add Bus</button>
            </div>
            <div className="admin-table-wrapper">
              <table className="bus-table">
                <thead>
                  <tr><th>Bus ID</th><th>Name</th><th>Route</th><th>District</th><th>Type</th><th>Status</th><th>Actions</th></tr>
                </thead>
                <tbody>
                  {buses.map(bus => (
                    <tr key={bus._id}>
                      <td><strong>{bus.busId}</strong></td>
                      <td>{bus.name}</td>
                      <td>{bus.routeName}</td>
                      <td>{bus.district}</td>
                      <td><span className={`bus-type-badge ${bus.type}`}>{bus.type || "ordinary"}</span></td>
                      <td>
                        <div style={{ display: "flex", alignItems: "center", gap: "8px" }}>
                          <span className={`status-pill ${bus.isOnTrip ? "live" : bus.status || "active"}`}>
                            {bus.isOnTrip ? "🟢 Live" : bus.status || "active"}
                          </span>
                          {bus.isOnTrip && (
                            <button className="btn btn-sm btn-outline" style={{ borderColor: "#ef4444", color: "#ef4444", padding: "2px 6px", fontSize: "11px", cursor: "pointer" }}
                              onClick={() => handleForceOffline(bus)}>
                              Stop
                            </button>
                          )}
                        </div>
                      </td>
                      <td>
                        <div className="table-actions">
                          <button className="btn btn-sm btn-outline" onClick={() => openEditBus(bus)}>✏️</button>
                          <button className="btn btn-sm btn-danger" onClick={() => handleDeleteBus(bus._id)}>🗑️</button>
                        </div>
                      </td>
                    </tr>
                  ))}
                  {buses.length === 0 && (
                    <tr><td colSpan={7} style={{ textAlign: "center", padding: "40px", color: "#64748b" }}>No buses yet.</td></tr>
                  )}
                </tbody>
              </table>
            </div>
          </div>
        )}

        {tab === "alerts" && (
          <div className="admin-alerts">
            <div className="admin-section-header">
              <h2>🔔 Active Alerts ({alerts.length})</h2>
              <button className="btn btn-primary" onClick={() => setShowAlertModal(true)}>+ Create Alert</button>
            </div>
            <div className="alerts-grid">
              {alerts.length === 0 ? (
                <div className="empty-state"><div className="empty-icon">🔔</div><p>No active alerts.</p></div>
              ) : alerts.map(a => (
                <div key={a._id} className={`alert-card alert-${a.severity}`}>
                  <div className="alert-card-header">
                    <span className="alert-type-badge">{a.type}</span>
                    <span className={`alert-severity-badge ${a.severity}`}>{a.severity}</span>
                  </div>
                  <h3>{a.title}</h3>
                  <p>{a.message}</p>
                  <div className="alert-card-footer">
                    <span>{new Date(a.createdAt).toLocaleString()}</span>
                    <button className="btn btn-sm btn-danger" onClick={async () => {
                      await axios.delete(`${API}/alerts/${a._id}`).catch(() => {});
                      fetchAlerts();
                    }}>Dismiss</button>
                  </div>
                </div>
              ))}
            </div>
          </div>
        )}

        {tab === "map" && (
          <div className="admin-map-full">
            <MapContainer center={mapCenter} zoom={mapZoom} style={{ height: "100%", width: "100%" }}>
              <TileLayer url="https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png" attribution='&copy; OSM' />
              {driverActive && driverPosition && (
                <><FlyTo center={driverPosition} zoom={15} />
                  <Marker position={driverPosition}><Popup>📡 Your location ({selectedBusForDriver})</Popup></Marker></>
              )}
              {liveBuses.map(bus => (
                <Marker key={bus._id} position={[bus.latitude, bus.longitude]} icon={busIcon}>
                  <Popup>{bus.name || bus.busId}<br />{bus.routeName}</Popup>
                </Marker>
              ))}
            </MapContainer>
          </div>
        )}
      </div>

      {/* Bus Modal */}
      {showModal && (
        <div className="modal-overlay" onClick={() => setShowModal(false)}>
          <div className="modal" onClick={e => e.stopPropagation()}>
            <h3>{editingBus ? "Edit Bus" : "Add New Bus"}</h3>
            <div className="form-group"><label>Bus ID</label>
              <input value={form.busId} onChange={e => setForm({ ...form, busId: e.target.value })} placeholder="e.g. BUS101" id="form-busId" /></div>
            <div className="form-group"><label>Bus Name</label>
              <input value={form.name} onChange={e => setForm({ ...form, name: e.target.value })} placeholder="e.g. Coimbatore Express" id="form-name" /></div>
            <div className="form-group"><label>Route Name</label>
              <input value={form.routeName} onChange={e => setForm({ ...form, routeName: e.target.value })} placeholder="e.g. Gandhipuram → Ukkadam" id="form-route" /></div>
            <div className="form-row">
              <div className="form-group"><label>District</label>
                <select value={form.district} onChange={e => setForm({ ...form, district: e.target.value })} id="form-district">
                  <option value="">Select</option>
                  {districts.map(d => (<option key={d.name} value={d.name}>{d.name}</option>))}
                </select></div>
              <div className="form-group"><label>Type</label>
                <select value={form.type} onChange={e => setForm({ ...form, type: e.target.value })} id="form-type">
                  <option value="ordinary">Ordinary</option><option value="express">Express</option>
                  <option value="deluxe">Deluxe</option><option value="ac">A/C</option>
                </select></div>
            </div>
            <div className="form-row">
              <div className="form-group"><label>Timing</label>
                <input value={form.timing} onChange={e => setForm({ ...form, timing: e.target.value })} placeholder="6:00 AM - 10:00 PM" id="form-timing" /></div>
              <div className="form-group"><label>Capacity</label>
                <input type="number" value={form.capacity} onChange={e => setForm({ ...form, capacity: parseInt(e.target.value) || 50 })} id="form-capacity" /></div>
            </div>
            <div className="modal-actions">
              <button className="btn btn-outline" onClick={() => setShowModal(false)}>Cancel</button>
              <button className="btn btn-primary" onClick={handleSaveBus} id="save-bus-btn">{editingBus ? "Update" : "Create"}</button>
            </div>
          </div>
        </div>
      )}

      {/* Alert Modal */}
      {showAlertModal && (
        <div className="modal-overlay" onClick={() => setShowAlertModal(false)}>
          <div className="modal" onClick={e => e.stopPropagation()}>
            <h3>Create Alert</h3>
            <div className="form-group"><label>Title</label>
              <input value={alertForm.title} onChange={e => setAlertForm({ ...alertForm, title: e.target.value })} placeholder="Alert title" /></div>
            <div className="form-group"><label>Message</label>
              <textarea value={alertForm.message} onChange={e => setAlertForm({ ...alertForm, message: e.target.value })}
                placeholder="Alert details..." rows={3} className="form-textarea" /></div>
            <div className="form-row">
              <div className="form-group"><label>Type</label>
                <select value={alertForm.type} onChange={e => setAlertForm({ ...alertForm, type: e.target.value })}>
                  <option value="info">Info</option><option value="delay">Delay</option>
                  <option value="breakdown">Breakdown</option><option value="diversion">Diversion</option>
                  <option value="emergency">Emergency</option><option value="weather">Weather</option>
                </select></div>
              <div className="form-group"><label>Severity</label>
                <select value={alertForm.severity} onChange={e => setAlertForm({ ...alertForm, severity: e.target.value })}>
                  <option value="info">Info</option><option value="warning">Warning</option><option value="critical">Critical</option>
                </select></div>
            </div>
            <div className="modal-actions">
              <button className="btn btn-outline" onClick={() => setShowAlertModal(false)}>Cancel</button>
              <button className="btn btn-primary" onClick={handleSaveAlert}>Create Alert</button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
