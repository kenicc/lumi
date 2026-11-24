import "server-only";
import { createClient, type SupabaseClient } from "@supabase/supabase-js";
import type { Database } from "../database";

// Klucz serwisowy omija RLS - używać wyłącznie po sprawdzeniu uprawnień (src/lib/access.ts)
let client: SupabaseClient<Database> | null = null;

export function admin() {
  if (!client) {
    client = createClient<Database>(process.env.NEXT_PUBLIC_SUPABASE_URL!, process.env.SUPABASE_SERVICE_ROLE_KEY!, {
      auth: { persistSession: false, autoRefreshToken: false },
    });
  }
  return client;
}
