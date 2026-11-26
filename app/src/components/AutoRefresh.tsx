"use client";
import { useRouter } from "next/navigation";
import { useEffect } from "react";

// Odświeża stan urządzenia (bateria, ostatni kontakt) bez przeładowania strony
export function AutoRefresh({ seconds = 20 }: { seconds?: number }) {
  const router = useRouter();
  useEffect(() => {
    const t = setInterval(() => {
      if (document.visibilityState === "visible") router.refresh();
    }, seconds * 1000);
    return () => clearInterval(t);
  }, [router, seconds]);
  return null;
}
