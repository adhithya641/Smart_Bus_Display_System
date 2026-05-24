import { BrowserRouter, Routes, Route } from "react-router-dom";
import { AuthProvider } from "./context/AuthContext";
import Toast from "./components/Toast";
import LoginPage from "./pages/LoginPage";
import RegisterPage from "./pages/RegisterPage";
import PassengerDashboard from "./pages/PassengerDashboard";
import DistrictSelectPage from "./pages/DistrictSelectPage";
import BusRoutePage from "./pages/BusRoutePage";
import Dashboard from "./pages/Dashboard";
import BusTrackingPage from "./pages/BusTrackingPage";
import FavoritesPage from "./pages/FavoritesPage";
import OfficialDashboard from "./pages/OfficialDashboard";
import DriverApp from "./pages/DriverApp";
import "./styles.css";

function App() {
  return (
    <AuthProvider>
      <BrowserRouter>
        <Toast />
        <Routes>
          {/* Auth */}
          <Route path="/" element={<LoginPage />} />
          <Route path="/register" element={<RegisterPage />} />

          {/* Passenger */}
          <Route path="/home" element={<PassengerDashboard />} />
          <Route path="/districts" element={<DistrictSelectPage />} />
          <Route path="/buses/:district" element={<BusRoutePage />} />
          <Route path="/dashboard/:busId" element={<Dashboard />} />
          <Route path="/track/:busId" element={<BusTrackingPage />} />
          <Route path="/search" element={<PassengerDashboard />} />
          <Route path="/favorites" element={<FavoritesPage />} />

          {/* Admin */}
          <Route path="/official" element={<OfficialDashboard />} />

          {/* Driver */}
          <Route path="/driver" element={<DriverApp />} />
        </Routes>
      </BrowserRouter>
    </AuthProvider>
  );
}

export default App;
