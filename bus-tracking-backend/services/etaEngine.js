/**
 * AI ETA Prediction Engine
 * Calculates estimated time of arrival using multiple factors:
 * 1. Haversine distance between bus and target stop
 * 2. Current bus speed from GPS data
 * 3. Time-of-day traffic factor (peak hours = slower)
 * 4. Intermediate stop dwell time (~30s per stop)
 * 5. Historical average speed fallback
 */

/**
 * Calculate distance between two GPS coordinates using Haversine formula
 * @param {number} lat1 - Latitude of point 1
 * @param {number} lon1 - Longitude of point 1
 * @param {number} lat2 - Latitude of point 2
 * @param {number} lon2 - Longitude of point 2
 * @returns {number} Distance in kilometers
 */
function haversineDistance(lat1, lon1, lat2, lon2) {
  const R = 6371; // Earth's radius in km
  const dLat = toRad(lat2 - lat1);
  const dLon = toRad(lon2 - lon1);
  const a =
    Math.sin(dLat / 2) * Math.sin(dLat / 2) +
    Math.cos(toRad(lat1)) * Math.cos(toRad(lat2)) *
    Math.sin(dLon / 2) * Math.sin(dLon / 2);
  const c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
  return R * c;
}

function toRad(deg) {
  return deg * (Math.PI / 180);
}

/**
 * Get traffic multiplier based on time of day
 * Peak hours have slower speeds (higher multiplier)
 * @param {Date} now - Current time
 * @returns {number} Multiplier (1.0 = normal, >1 = slower)
 */
function getTrafficFactor(now = new Date()) {
  const hour = now.getHours();

  // Morning peak: 7-10 AM
  if (hour >= 7 && hour <= 9) return 1.4;
  if (hour === 10) return 1.2;

  // Evening peak: 5-8 PM
  if (hour >= 17 && hour <= 19) return 1.5;
  if (hour === 20) return 1.2;

  // Late night: minimal traffic
  if (hour >= 22 || hour <= 5) return 0.8;

  // Normal hours
  return 1.0;
}

/**
 * Calculate ETA from bus to a specific stop
 * @param {Object} params
 * @param {number} params.busLat - Bus current latitude
 * @param {number} params.busLng - Bus current longitude
 * @param {number} params.stopLat - Target stop latitude
 * @param {number} params.stopLng - Target stop longitude
 * @param {number} params.currentSpeed - Current speed in km/h (from GPS)
 * @param {number} params.intermediateStops - Number of stops before target
 * @param {number} params.routeDistance - Actual route distance in km (if known)
 * @returns {Object} { etaMinutes, distanceKm, confidence, status }
 */
function calculateETA({
  busLat,
  busLng,
  stopLat,
  stopLng,
  currentSpeed = 0,
  intermediateStops = 0,
  routeDistance = null,
}) {
  // Step 1: Calculate distance
  const straightLineDistance = haversineDistance(busLat, busLng, stopLat, stopLng);

  // Use route distance if provided, otherwise estimate from straight-line
  // Roads are typically 1.3-1.5x the straight-line distance
  const estimatedDistance = routeDistance || (straightLineDistance * 1.35);

  // Step 2: Determine effective speed
  const trafficFactor = getTrafficFactor();

  // Default city bus average speed: 25 km/h
  const DEFAULT_SPEED = 25;
  const MIN_SPEED = 5;
  const MAX_SPEED = 60;

  let effectiveSpeed;
  let confidence;

  if (currentSpeed > MIN_SPEED) {
    // Use actual speed with traffic adjustment
    effectiveSpeed = Math.min(currentSpeed / trafficFactor, MAX_SPEED);
    confidence = "high";
  } else if (currentSpeed > 0) {
    // Bus is moving slowly (traffic/stop), blend with average
    effectiveSpeed = ((currentSpeed + DEFAULT_SPEED) / 2) / trafficFactor;
    confidence = "medium";
  } else {
    // No speed data, use default with traffic factor
    effectiveSpeed = DEFAULT_SPEED / trafficFactor;
    confidence = "low";
  }

  // Step 3: Calculate base travel time
  let travelTimeMinutes = (estimatedDistance / effectiveSpeed) * 60;

  // Step 4: Add dwell time for intermediate stops (~30 seconds each)
  const dwellTime = intermediateStops * 0.5; // 30 seconds = 0.5 minutes
  travelTimeMinutes += dwellTime;

  // Step 5: Round and determine status
  const etaMinutes = Math.max(1, Math.round(travelTimeMinutes));

  // Determine status based on expected vs actual
  let status = "on_time";
  if (trafficFactor > 1.3) {
    status = "delayed";
  } else if (trafficFactor < 0.9) {
    status = "early";
  }

  return {
    etaMinutes,
    distanceKm: Math.round(estimatedDistance * 10) / 10,
    confidence,
    status,
    trafficFactor: Math.round(trafficFactor * 100) / 100,
    effectiveSpeedKmh: Math.round(effectiveSpeed * 10) / 10,
  };
}

/**
 * Calculate ETAs for all stops on a route from bus position
 * @param {Object} busPosition - { lat, lng, speed }
 * @param {Array} stops - Array of { lat, lng, name, order }
 * @returns {Array} Stops with ETA info added
 */
function calculateRouteETAs(busPosition, stops) {
  if (!busPosition || !stops || stops.length === 0) return [];

  // Sort stops by order
  const sortedStops = [...stops].sort((a, b) => (a.order || 0) - (b.order || 0));

  return sortedStops.map((stop, index) => {
    const stopLat = stop.lat || stop.location?.coordinates?.[1];
    const stopLng = stop.lng || stop.location?.coordinates?.[0];

    if (!stopLat || !stopLng) {
      return { ...stop, eta: null };
    }

    const eta = calculateETA({
      busLat: busPosition.lat,
      busLng: busPosition.lng,
      stopLat,
      stopLng,
      currentSpeed: busPosition.speed || 0,
      intermediateStops: index,
    });

    return {
      ...stop,
      eta,
    };
  });
}

module.exports = {
  calculateETA,
  calculateRouteETAs,
  haversineDistance,
  getTrafficFactor,
};
