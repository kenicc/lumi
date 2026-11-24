"use client";
import { useEffect } from "react";

// Service worker: aplikacja instaluje się na ekranie telefonu i otwiera bez paska przeglądarki
export function SwRegister() {
  useEffect(() => {
    if ("serviceWorker" in navigator) navigator.serviceWorker.register("/sw.js").catch(() => {});
  }, []);
  return null;
}
