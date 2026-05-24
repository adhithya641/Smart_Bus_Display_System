/**
 * StopTimeline — Vertical timeline of bus stops with ETA info
 */
export default function StopTimeline({ stops = [], currentStopIndex = -1 }) {
  if (stops.length === 0) return null;

  return (
    <div className="stop-timeline" id="stop-timeline">
      <h3 className="timeline-title">Route Stops</h3>
      {stops.map((stop, index) => {
        const isPassed = index < currentStopIndex;
        const isCurrent = index === currentStopIndex;
        const name = stop.name || stop.stop?.name || `Stop ${index + 1}`;
        const eta = stop.eta;

        return (
          <div key={index} className={`timeline-item ${isPassed ? "passed" : ""} ${isCurrent ? "current" : ""}`}>
            <div className="timeline-track">
              <div className={`timeline-dot ${index === 0 ? "first" : ""} ${index === stops.length - 1 ? "last" : ""} ${isCurrent ? "current" : ""} ${isPassed ? "passed" : ""}`} />
              {index < stops.length - 1 && <div className={`timeline-line ${isPassed ? "passed" : ""}`} />}
            </div>
            <div className="timeline-content">
              <div className="timeline-stop-name">{name}</div>
              {eta && (
                <div className="timeline-eta">
                  <span className={`timeline-eta-time ${eta.status}`}>
                    {eta.etaMinutes} min
                  </span>
                  <span className="timeline-eta-dist">{eta.distanceKm} km</span>
                </div>
              )}
              {stop.expectedArrival && !eta && (
                <div className="timeline-expected">{stop.expectedArrival}</div>
              )}
            </div>
          </div>
        );
      })}
    </div>
  );
}
