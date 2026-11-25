// Typy tabel z supabase/schema.sql (utrzymywane ręcznie; przy większej bazie: supabase gen types)
type Json = string | number | boolean | null | { [k: string]: Json | undefined } | Json[];

type Table<Row, Required extends keyof Row = never> = {
  Row: Row;
  Insert: Partial<Row> & Pick<Row, Required>;
  Update: Partial<Row>;
  Relationships: [];
};

export interface Database {
  public: {
    Tables: {
      profiles: Table<{ user_id: string; display_name: string; form: string; created_at: string }, "user_id" | "display_name">;
      devices: Table<
        {
          id: string;
          secret_hash: string;
          pair_hash: string | null;
          name: string | null;
          settings: Json | null;
          config: Json | null;
          config_version: number;
          applied_version: number;
          status: Json | null;
          variant: string | null;
          fw: string | null;
          fw_build: number;
          offline_alerted: boolean;
          last_seen: string | null;
          created_at: string;
        },
        "id" | "secret_hash"
      >;
      members: Table<{ device_id: string; user_id: string; role: string; created_at: string }, "device_id" | "user_id" | "role">;
      invites: Table<
        { id: string; device_id: string; email: string; role: string; created_by: string | null; created_at: string },
        "device_id" | "email" | "role"
      >;
      changes: Table<
        { id: number; device_id: string; user_id: string | null; author: string; summary: string; version: number; created_at: string },
        "device_id" | "author" | "summary" | "version"
      >;
      push_subscriptions: Table<
        { endpoint: string; user_id: string; p256dh: string; auth: string; created_at: string },
        "endpoint" | "user_id" | "p256dh" | "auth"
      >;
      firmware_releases: Table<
        {
          id: number;
          variant: string;
          build: number;
          version: string;
          url: string;
          size: number;
          sha256: string;
          signature: string;
          notes: string;
          active: boolean;
          created_at: string;
        },
        "variant" | "build" | "version" | "url" | "size" | "sha256" | "signature"
      >;
    };
    Views: Record<string, never>;
    Functions: Record<string, never>;
    Enums: Record<string, never>;
    CompositeTypes: Record<string, never>;
  };
}
