import { LoginForm } from "./LoginForm";

export default async function Login({ searchParams }: { searchParams: Promise<{ next?: string }> }) {
  const { next } = await searchParams;
  const safeNext = next && next.startsWith("/") && !next.startsWith("//") ? next : "/";
  return (
    <main>
      <h1>LUMI</h1>
      <p className="dim">Aplikacja dla rodziny i opiekunów. Tu ustawiasz kontakty, radio i wygląd LUMI, a urządzenie samo pobiera zmiany.</p>
      <LoginForm next={safeNext} />
    </main>
  );
}
