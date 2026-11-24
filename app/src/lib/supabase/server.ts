import { createServerClient } from "@supabase/ssr";
import { cookies } from "next/headers";

// Klient z sesją zalogowanego użytkownika (tylko do logowania i odczytu tożsamości)
export async function supabaseServer() {
  const store = await cookies();
  return createServerClient(process.env.NEXT_PUBLIC_SUPABASE_URL!, process.env.NEXT_PUBLIC_SUPABASE_ANON_KEY!, {
    cookies: {
      getAll: () => store.getAll(),
      setAll: (list) => {
        try {
          list.forEach(({ name, value, options }) => store.set(name, value, options));
        } catch {
          // wywołane z komponentu serwerowego - sesję odświeża middleware
        }
      },
    },
  });
}
