import type { Settings } from "./schema";

export type Role = "guardian" | "family" | "friend";

export interface DeviceStatus {
  alertLevel?: number;  // serwer: najniższy zgłoszony próg baterii (lib/alerts.ts), nie od urządzenia
  version?: number;
  battery?: number;
  batteryMv?: number;
  charging?: boolean;
  signal?: number;
  registered?: boolean;
  lte?: boolean;
  operator?: string;
  modem?: string;
  sim?: boolean;
  wifiRssi?: number;
  uptime?: number;
  variant?: string;
  build?: number;
  reset?: string;
  boots?: number;
  errors?: string[];
}

export interface DeviceRow {
  id: string;
  secret_hash: string;
  pair_hash: string | null;
  name: string | null;
  settings: Settings | null;
  config: unknown;
  config_version: number;
  applied_version: number;
  status: DeviceStatus | null;
  variant: string | null;
  fw: string | null;
  fw_build: number;
  offline_alerted: boolean;
  last_seen: string | null;
}

export const ROLE_NAMES: Record<Role, string> = { guardian: "Główny opiekun", family: "Rodzina", friend: "Znajomi" };
