import React from "react";
import { MapContainer, TileLayer, Polyline, CircleMarker, Marker, Popup, useMap } from "react-leaflet";
import L from "leaflet";
import "leaflet/dist/leaflet.css";

// Fix default marker icon
delete L.Icon.Default.prototype._getIconUrl;
L.Icon.Default.mergeOptions({
  iconRetinaUrl: "https://cdnjs.cloudflare.com/ajax/libs/leaflet/1.9.4/images/marker-icon-2x.png",
  iconUrl: "https://cdnjs.cloudflare.com/ajax/libs/leaflet/1.9.4/images/marker-icon.png",
  shadowUrl: "https://cdnjs.cloudflare.com/ajax/libs/leaflet/1.9.4/images/marker-shadow.png",
});

// Custom bus icon
const busIcon = new L.DivIcon({
  className: "",
  html: '<div style="font-size:28px;filter:drop-shadow(0 2px 4px rgba(0,0,0,0.5))">🚌</div>',
  iconSize: [32, 32],
  iconAnchor: [16, 16],
});

function ChangeView({ center, zoom }) {
  const map = useMap();
  map.flyTo(center, zoom, { duration: 1.2 });
  return null;
}

export default function SmartMap({ busPosition, center, zoom = 13, stops = [] }) {
  const routeCoords = stops.filter(s => s.lat && s.lng).map(s => [s.lat, s.lng]);
  const mapCenter = center || (routeCoords.length > 0 ? routeCoords[0] : [10.85, 78.65]);

  return (
    <MapContainer center={mapCenter} zoom={zoom} style={{ height: "100%", width: "100%" }}>
      <TileLayer
        url="https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png"
        attribution='&copy; OpenStreetMap contributors'
      />
      <ChangeView center={mapCenter} zoom={zoom} />

      {/* Route Line */}
      {routeCoords.length > 1 && (
        <Polyline positions={routeCoords} color="#3b82f6" weight={3} opacity={0.7} />
      )}

      {/* Stops */}
      {stops.filter(s => s.lat && s.lng).map((stop, index) => (
        <CircleMarker
          key={index}
          center={[stop.lat, stop.lng]}
          radius={7}
          fillColor={index === 0 ? "#10b981" : index === stops.length - 1 ? "#ef4444" : "#3b82f6"}
          fillOpacity={0.9}
          color="white"
          weight={2}
        >
          <Popup>{stop.name}</Popup>
        </CircleMarker>
      ))}

      {/* Moving Bus Marker */}
      {busPosition && (
        <Marker position={busPosition} icon={busIcon}>
          <Popup>🚌 Bus is here</Popup>
        </Marker>
      )}
    </MapContainer>
  );
}
