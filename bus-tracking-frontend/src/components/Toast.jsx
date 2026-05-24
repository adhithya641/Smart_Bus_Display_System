/**
 * Toast — Notification toast component
 */
import { useState, useEffect } from "react";

let toastHandler = null;

export function showToast(message, type = "info", duration = 3000) {
  if (toastHandler) toastHandler({ message, type, duration });
}

export default function Toast() {
  const [toasts, setToasts] = useState([]);

  useEffect(() => {
    toastHandler = ({ message, type, duration }) => {
      const id = Date.now();
      setToasts(prev => [...prev, { id, message, type }]);
      setTimeout(() => {
        setToasts(prev => prev.filter(t => t.id !== id));
      }, duration);
    };
    return () => { toastHandler = null; };
  }, []);

  return (
    <div className="toast-container">
      {toasts.map(toast => (
        <div key={toast.id} className={`toast toast-${toast.type}`}>
          <span className="toast-icon">
            {toast.type === "success" ? "✅" : toast.type === "error" ? "❌" : toast.type === "warning" ? "⚠️" : "ℹ️"}
          </span>
          <span className="toast-message">{toast.message}</span>
        </div>
      ))}
    </div>
  );
}
