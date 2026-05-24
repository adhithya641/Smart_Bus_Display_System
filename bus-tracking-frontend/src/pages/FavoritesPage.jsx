import { useState, useEffect } from "react";
import { useNavigate } from "react-router-dom";
import axios from "axios";
import { useAuth } from "../context/AuthContext";
import { API_URL } from "../config";

const API = API_URL;

/**
 * FavoritesPage — Saved routes and quick access to tracking
 */
export default function FavoritesPage() {
  const navigate = useNavigate();
  const { user, token } = useAuth();
  const [favorites, setFavorites] = useState([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    const loadFavorites = async () => {
      if (!token || !user?.favoriteRoutes?.length) {
        setLoading(false);
        return;
      }
      try {
        const res = await axios.get(`${API}/auth/me`);
        setFavorites(res.data.user.favoriteRoutes || []);
      } catch (err) { console.error(err); }
      setLoading(false);
    };
    loadFavorites();
  }, [token, user]);

  return (
    <div className="favorites-page">
      <div className="favorites-container">
        <div className="sidebar-header">
          <button className="back-btn" onClick={() => navigate("/home")}>← Back</button>
          <h2>⭐ Favorite Routes</h2>
          <p>Your saved routes for quick access</p>
        </div>

        <div className="favorites-list">
          {loading ? (
            <div className="empty-state"><p>Loading...</p></div>
          ) : !token ? (
            <div className="empty-state">
              <div className="empty-icon">🔐</div>
              <p>Sign in to save favorite routes</p>
              <button className="btn btn-primary" onClick={() => navigate("/")}>Sign In</button>
            </div>
          ) : favorites.length === 0 ? (
            <div className="empty-state">
              <div className="empty-icon">⭐</div>
              <p>No favorite routes yet.<br />Search for a route and tap the star to save it.</p>
              <button className="btn btn-primary" onClick={() => navigate("/home")}>Search Routes</button>
            </div>
          ) : (
            favorites.map(route => (
              <div key={route._id} className="route-result-card"
                onClick={() => navigate(`/districts`)}>
                <div className="route-result-header">
                  <span className="route-number">{route.routeNumber}</span>
                  <span className="route-name">{route.name}</span>
                </div>
                <div className="route-result-path">
                  <span className="route-origin">{route.origin}</span>
                  <span className="route-arrow">→</span>
                  <span className="route-dest">{route.destination}</span>
                </div>
              </div>
            ))
          )}
        </div>
      </div>
    </div>
  );
}
