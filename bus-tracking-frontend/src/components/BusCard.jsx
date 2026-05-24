/**
 * BusCard — Premium bus result card showing route info and live status
 */
export default function BusCard({ bus, onClick, showETA = true }) {
  const isLive = bus.isOnTrip && bus.latitude && bus.longitude;
  const typeLabels = { ordinary: "ORD", express: "EXP", deluxe: "DLX", ac: "A/C" };
  const lastUpdate = bus.lastUpdated ? getTimeAgo(new Date(bus.lastUpdated)) : null;

  return (
    <div className={`bus-result-card ${isLive ? "live" : ""}`} onClick={onClick} id={`bus-card-${bus.busId}`}>
      <div className="bus-card-header">
        <div className="bus-card-id">
          <span className="bus-icon-sm">🚌</span>
          <span className="bus-card-name">{bus.name || bus.busId}</span>
          {bus.type && (
            <span className={`bus-type-badge ${bus.type}`}>{typeLabels[bus.type] || bus.type}</span>
          )}
        </div>
        {isLive && (
          <div className="live-indicator">
            <span className="live-dot" />
            LIVE
          </div>
        )}
      </div>

      <div className="bus-card-route">
        📍 {bus.routeName || bus.assignedRoute?.name || "Route info unavailable"}
      </div>

      {bus.timing && <div className="bus-card-timing">🕐 {bus.timing}</div>}

      <div className="bus-card-footer">
        {showETA && bus.eta && (
          <div className={`bus-eta-badge ${bus.eta.status}`}>
            <span className="eta-time">{bus.eta.etaMinutes} min</span>
            <span className="eta-dist">{bus.eta.distanceKm} km</span>
          </div>
        )}
        {bus.currentSpeed > 0 && (
          <span className="bus-speed-sm">⚡ {Math.round(bus.currentSpeed)} km/h</span>
        )}
        {lastUpdate && isLive && (
          <span className="bus-last-update">{lastUpdate}</span>
        )}
        <span className="bus-card-arrow">→</span>
      </div>
    </div>
  );
}

function getTimeAgo(date) {
  const seconds = Math.floor((new Date() - date) / 1000);
  if (seconds < 60) return `${seconds}s ago`;
  if (seconds < 3600) return `${Math.floor(seconds / 60)}m ago`;
  return `${Math.floor(seconds / 3600)}h ago`;
}
