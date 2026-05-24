import { useState } from "react";

/**
 * SearchBar — Uber/Google Maps style dual search input
 * @param {Function} onSearch - Called with { from, to }
 * @param {boolean} loading - Show loading state
 */
export default function SearchBar({ onSearch, loading = false }) {
  const [from, setFrom] = useState("");
  const [to, setTo] = useState("");

  const handleSearch = (e) => {
    e.preventDefault();
    if (from.trim() || to.trim()) {
      onSearch({ from: from.trim(), to: to.trim() });
    }
  };

  const handleSwap = () => {
    setFrom(to);
    setTo(from);
  };

  return (
    <form className="search-bar-container" onSubmit={handleSearch} id="route-search-form">
      <div className="search-inputs">
        <div className="search-input-group">
          <div className="search-dot origin" />
          <input
            type="text"
            placeholder="Where are you? (e.g., Gandhipuram)"
            value={from}
            onChange={(e) => setFrom(e.target.value)}
            className="search-input"
            id="search-from"
          />
        </div>

        <button type="button" className="search-swap-btn" onClick={handleSwap} title="Swap">
          ⇅
        </button>

        <div className="search-input-group">
          <div className="search-dot destination" />
          <input
            type="text"
            placeholder="Where to? (e.g., Ukkadam)"
            value={to}
            onChange={(e) => setTo(e.target.value)}
            className="search-input"
            id="search-to"
          />
        </div>
      </div>

      <button
        type="submit"
        className="search-submit-btn"
        disabled={loading || (!from.trim() && !to.trim())}
        id="search-submit"
      >
        {loading ? (
          <span className="search-spinner" />
        ) : (
          <>🔍 Find Buses</>
        )}
      </button>
    </form>
  );
}
