import { useNavigate, useLocation } from "react-router-dom";
import { useAuth } from "../context/AuthContext";

/**
 * Navbar — Top navigation bar with role-aware links
 */
export default function Navbar() {
  const { user, logout, isAdmin, isDriver } = useAuth();
  const navigate = useNavigate();
  const location = useLocation();

  const handleLogout = () => {
    logout();
    navigate("/");
  };

  const isActive = (path) => location.pathname === path || location.pathname.startsWith(path + "/");

  return (
    <nav className="navbar" id="main-navbar">
      <div className="nav-brand" onClick={() => navigate(isAdmin ? "/official" : "/home")}>
        <span className="nav-logo">🚌</span>
        <span className="nav-title">TN Bus Tracker</span>
      </div>

      <div className="nav-links">
        {!isAdmin && !isDriver && (
          <>
            <button className={`nav-link ${isActive("/home") ? "active" : ""}`}
              onClick={() => navigate("/home")} id="nav-home">
              🏠 Home
            </button>
            <button className={`nav-link ${isActive("/districts") ? "active" : ""}`}
              onClick={() => navigate("/districts")} id="nav-districts">
              📍 Districts
            </button>
            <button className={`nav-link ${isActive("/search") ? "active" : ""}`}
              onClick={() => navigate("/search")} id="nav-search">
              🔍 Search
            </button>
            <button className={`nav-link ${isActive("/favorites") ? "active" : ""}`}
              onClick={() => navigate("/favorites")} id="nav-favorites">
              ⭐ Favorites
            </button>
          </>
        )}

        {isAdmin && (
          <button className={`nav-link ${isActive("/official") ? "active" : ""}`}
            onClick={() => navigate("/official")} id="nav-admin">
            🛠️ Dashboard
          </button>
        )}

        {isDriver && (
          <button className={`nav-link ${isActive("/driver") ? "active" : ""}`}
            onClick={() => navigate("/driver")} id="nav-driver">
            📡 Driver Mode
          </button>
        )}
      </div>

      <div className="nav-user">
        {user && (
          <>
            <div className="nav-user-info">
              <span className="nav-user-avatar">
                {isAdmin ? "🛡️" : isDriver ? "🚗" : "👤"}
              </span>
              <span className="nav-user-name">{user.name || "User"}</span>
              <span className={`nav-role-badge ${user.role}`}>{user.role}</span>
            </div>
            <button className="nav-logout" onClick={handleLogout} id="nav-logout">
              Logout
            </button>
          </>
        )}
      </div>
    </nav>
  );
}
