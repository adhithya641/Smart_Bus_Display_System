import { useEffect, useRef, useCallback } from "react";
import io from "socket.io-client";

import { API_URL } from "../config";

const SOCKET_URL = API_URL.replace("/api", "");

/**
 * useSocket — Reusable Socket.IO hook with auto-reconnect
 * @param {string} event - Event name to listen to
 * @param {Function} callback - Handler for incoming data
 */
export function useSocket(event, callback) {
  const socketRef = useRef(null);
  const callbackRef = useRef(callback);
  callbackRef.current = callback;

  useEffect(() => {
    if (!socketRef.current) {
      socketRef.current = io(SOCKET_URL, {
        transports: ["websocket", "polling"],
        reconnection: true,
        reconnectionDelay: 1000,
        reconnectionAttempts: 10,
      });
    }

    const socket = socketRef.current;
    const handler = (data) => callbackRef.current(data);
    socket.on(event, handler);

    return () => {
      socket.off(event, handler);
    };
  }, [event]);

  // Emit function
  const emit = useCallback((eventName, data) => {
    if (socketRef.current) {
      socketRef.current.emit(eventName, data);
    }
  }, []);

  return { emit };
}

/**
 * useAllBusUpdates — Listen to all bus location updates
 * @param {Function} onUpdate - Called with bus data on each update
 */
export function useAllBusUpdates(onUpdate) {
  return useSocket("busLocationUpdate", onUpdate);
}

export default useSocket;
