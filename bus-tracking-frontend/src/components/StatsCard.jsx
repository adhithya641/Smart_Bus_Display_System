/**
 * StatsCard — Glassmorphism stats card for admin dashboard
 */
export default function StatsCard({ icon, label, value, trend, color = "blue" }) {
  return (
    <div className={`stats-card stats-${color}`}>
      <div className="stats-icon">{icon}</div>
      <div className="stats-info">
        <div className="stats-value">{value}</div>
        <div className="stats-label">{label}</div>
      </div>
      {trend !== undefined && (
        <div className={`stats-trend ${trend >= 0 ? "up" : "down"}`}>
          {trend >= 0 ? "↑" : "↓"} {Math.abs(trend)}%
        </div>
      )}
    </div>
  );
}
