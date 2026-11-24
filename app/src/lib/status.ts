import type { DeviceRow } from "./types";

export const ONLINE_MS = 2 * 60 * 1000;

export function isOnline(d: Pick<DeviceRow, "last_seen">) {
  return !!d.last_seen && Date.now() - new Date(d.last_seen).getTime() < ONLINE_MS;
}

export function batteryText(d: DeviceRow) {
  const b = d.status?.battery;
  if (b === undefined) return "-";
  return `${Math.round(b)}%${d.status?.charging ? " ⚡" : ""}`;
}

export function signalText(d: DeviceRow) {
  const s = d.status;
  if (!s) return "-";
  if (s.sim === false) return "brak SIM";
  if (!s.registered) return "brak";
  return `${"▮".repeat(s.signal ?? 0)}${"▯".repeat(4 - (s.signal ?? 0))}`;
}
