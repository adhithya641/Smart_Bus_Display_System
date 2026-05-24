import { useState } from "react";
import { useNavigate } from "react-router-dom";

export default function SearchPage() {
  const navigate = useNavigate();

  const [search, setSearch] = useState("");

  const buses = [
    { id: "BUS101", route: "City Center" },
    { id: "BUS205", route: "College Route" },
    { id: "BUS300", route: "Airport Express" },
  ];

  const filtered = buses.filter(
    (bus) =>
      bus.id.toLowerCase().includes(search.toLowerCase()) ||
      bus.route.toLowerCase().includes(search.toLowerCase())
  );

  return (
    <div style={{ padding: 30 }}>
      <h1>🚌 Find Your Bus</h1>

      <input
        placeholder="Search by Bus No or Route"
        value={search}
        onChange={(e) => setSearch(e.target.value)}
        style={{ padding: 10, width: 300 }}
      />

      <div style={{ marginTop: 20 }}>
        {filtered.map((bus) => (
          <div
            key={bus.id}
            style={{
              border: "1px solid gray",
              padding: 15,
              margin: 10,
              cursor: "pointer",
            }}
            onClick={() => navigate(`/dashboard/${bus.id}`)}
          >
            <h3>{bus.id}</h3>
            <p>{bus.route}</p>
          </div>
        ))}
      </div>
    </div>
  );
}
