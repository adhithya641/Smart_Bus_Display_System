import { useState } from "react";
import { useNavigate } from "react-router-dom";
import { useAuth } from "../context/AuthContext";
import { showToast } from "../components/Toast";

/**
 * RegisterPage — Passenger registration form
 */
export default function RegisterPage() {
  const navigate = useNavigate();
  const { register } = useAuth();
  const [form, setForm] = useState({ name: "", email: "", password: "", confirmPassword: "", phone: "" });
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState("");

  const handleChange = (e) => setForm({ ...form, [e.target.name]: e.target.value });

  const handleSubmit = async (e) => {
    e.preventDefault();
    setError("");

    if (form.password !== form.confirmPassword) {
      setError("Passwords do not match.");
      return;
    }
    if (form.password.length < 6) {
      setError("Password must be at least 6 characters.");
      return;
    }

    setLoading(true);
    try {
      await register(form.name, form.email, form.password, form.phone);
      showToast("Account created successfully!", "success");
      navigate("/home");
    } catch (err) {
      setError(err.response?.data?.error || "Registration failed.");
    }
    setLoading(false);
  };

  return (
    <div className="login-page">
      <div className="login-particles">
        {[...Array(6)].map((_, i) => <div key={i} className={`particle p${i + 1}`} />)}
      </div>

      <div className="login-header">
        <span className="bus-icon">🚌</span>
        <h1>Create Account</h1>
        <p>Join TN Bus Tracker to save routes and track buses</p>
      </div>

      <div className="login-form-card" style={{ animation: "fadeInUp 0.6s ease-out" }}>
        <form onSubmit={handleSubmit}>
          <div className="form-group">
            <label>Full Name</label>
            <input name="name" value={form.name} onChange={handleChange}
              placeholder="Your full name" required id="reg-name" />
          </div>
          <div className="form-group">
            <label>Email</label>
            <input type="email" name="email" value={form.email} onChange={handleChange}
              placeholder="your@email.com" required id="reg-email" />
          </div>
          <div className="form-group">
            <label>Phone</label>
            <input name="phone" value={form.phone} onChange={handleChange}
              placeholder="9876543210" id="reg-phone" />
          </div>
          <div className="form-row">
            <div className="form-group">
              <label>Password</label>
              <input type="password" name="password" value={form.password} onChange={handleChange}
                placeholder="••••••••" required id="reg-password" />
            </div>
            <div className="form-group">
              <label>Confirm Password</label>
              <input type="password" name="confirmPassword" value={form.confirmPassword}
                onChange={handleChange} placeholder="••••••••" required id="reg-confirm" />
            </div>
          </div>
          {error && <div className="form-error">{error}</div>}
          <button type="submit" className="btn btn-primary btn-block" disabled={loading} id="reg-submit">
            {loading ? "Creating account..." : "Create Account"}
          </button>
        </form>
        <div className="login-register-link">
          Already have an account?{" "}
          <span onClick={() => navigate("/")} className="link-text">Sign In</span>
        </div>
      </div>
    </div>
  );
}
