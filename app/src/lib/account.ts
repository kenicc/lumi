// Usunięcie konta (RODO, prawo do usunięcia danych) - plan bez dostępu do bazy, testowany.
// Główny opiekun nie może zostawić seniora z urządzeniem, którym nikt nie zarządza:
// rola przechodzi na najdawniej dodaną osobę z rodziny (potem znajomych); LUMI bez nikogo jest usuwane.
import type { Role } from "./types";

export interface Membership {
  device_id: string;
  user_id: string;
  role: Role;
  created_at: string;
}

export interface DeletionPlan {
  promote: { device_id: string; user_id: string }[];
  deleteDevices: string[];
}

export function planAccountDeletion(userId: string, all: Membership[]): DeletionPlan {
  const plan: DeletionPlan = { promote: [], deleteDevices: [] };
  const mine = all.filter((m) => m.user_id === userId && m.role === "guardian");
  for (const g of mine) {
    const others = all
      .filter((m) => m.device_id === g.device_id && m.user_id !== userId)
      .sort((a, b) => (a.role === b.role ? a.created_at.localeCompare(b.created_at) : a.role === "family" ? -1 : 1));
    if (others.some((o) => o.role === "guardian")) continue; // jest już inny opiekun
    if (others.length) plan.promote.push({ device_id: g.device_id, user_id: others[0].user_id });
    else plan.deleteDevices.push(g.device_id);
  }
  return plan;
}

// Jak długo trzymamy dane (dni). Krócej = mniej danych o seniorze na serwerze.
export const RETENTION_DAYS = { reminderEvents: 90, messages: 180, changes: 365 } as const;
