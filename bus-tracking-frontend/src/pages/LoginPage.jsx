import { useState } from "react";
import { useNavigate } from "react-router-dom";
import { useAuth } from "../context/AuthContext";
import { showToast } from "../components/Toast";

/**
 * LoginPage — Premium login with email/password auth and quick-access guest mode
 */
export default function LoginPage() {
  const navigate = useNavigate();
  const { login } = useAuth();
  const mode = "select"; // "select" | "login"
  const [email, setEmail] = useState("");
  const [password, setPassword] = useState("");
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState("");

  const handleLogin = async (e) => {
    e.preventDefault();
    setLoading(true);
    setError("");
    try {
      const data = await login(email, password);
      showToast(`Welcome back, ${data.user.name}!`, "success");
      if (data.user.role === "admin") navigate("/official");
      else if (data.user.role === "driver") navigate("/driver");
      else navigate("/home");
    } catch (err) {
      setError(err.response?.data?.error || "Login failed. Please try again.");
    }
    setLoading(false);
  };
  return (
    <div className="login-page">
      {/* Animated background particles */}
      <div className="login-particles">
        {[...Array(6)].map((_, i) => <div key={i} className={`particle p${i + 1}`} />)}
      </div>

      <div className="login-header">
        <span className="bus-icon">🚌</span>
        <h1>TN Bus Tracker</h1>
        <p>Real-time bus tracking across Tamil Nadu</p>
      </div>

      {mode === "select" ? (
        <div className="login-content" style={{ animation: "fadeInUp 0.6s ease-out" }}>
          {/* Login Form */}
          <div className="login-form-card">
            <h2>Sign In</h2>
            <form onSubmit={handleLogin}>
              <div className="form-group">
                <label>Email</label>
                <input type="email" value={email} onChange={(e) => setEmail(e.target.value)}
                  placeholder="your@email.com" required id="login-email" />
              </div>
              <div className="form-group">
                <label>Password</label>
                <input type="password" value={password} onChange={(e) => setPassword(e.target.value)}
                  placeholder="••••••••" required id="login-password" />
              </div>
              {error && <div className="form-error">{error}</div>}
              <button type="submit" className="btn btn-primary btn-block" disabled={loading} id="login-submit">
                {loading ? "Signing in..." : "Sign In"}
              </button>
            </form>
            <div className="login-register-link">
              Don't have an account?{" "}
              <span onClick={() => navigate("/register")} className="link-text">Register</span>
            </div>
          </div>
        </div>
      ) : null}
    </div>
  );
}
