// Testy klikania aplikacji (E2E) na trybie demo - bez Supabase, dane w pamięci serwera.
// Uruchom: npm run build && npm run e2e
import { defineConfig, devices } from "@playwright/test";

const PORT = 3210;

export default defineConfig({
  testDir: "e2e",
  workers: 1, // wspólna baza demo w pamięci - testy po kolei
  timeout: 30_000,
  use: { baseURL: `http://localhost:${PORT}`, ...devices["Pixel 7"], locale: "pl-PL", trace: "off" },
  webServer: {
    command: `npx next start -p ${PORT}`,
    url: `http://localhost:${PORT}/login`,
    reuseExistingServer: false,
    timeout: 60_000,
    env: {
      LUMI_DEMO: "1",
      NEXT_PUBLIC_SUPABASE_URL: "https://demo.invalid",
      NEXT_PUBLIC_SUPABASE_ANON_KEY: "e2e",
      SUPABASE_SERVICE_ROLE_KEY: "e2e",
    },
  },
});
