/**
 * LoadingSkeleton — Animated loading placeholder
 */
export function SkeletonLine({ width = "100%", height = "16px" }) {
  return <div className="skeleton-line" style={{ width, height }} />;
}

export function SkeletonCard() {
  return (
    <div className="skeleton-card">
      <SkeletonLine width="60%" height="20px" />
      <SkeletonLine width="80%" />
      <SkeletonLine width="40%" />
    </div>
  );
}

export function SkeletonList({ count = 5 }) {
  return (
    <div className="skeleton-list">
      {Array.from({ length: count }).map((_, i) => (
        <SkeletonCard key={i} />
      ))}
    </div>
  );
}

export default function LoadingSkeleton({ type = "card", count = 3 }) {
  if (type === "list") return <SkeletonList count={count} />;
  return <SkeletonCard />;
}
