import { LoginForm } from "./LoginForm";

export default async function Login({ searchParams }: { searchParams: Promise<{ next?: string; deleted?: string }> }) {
  const { next, deleted } = await searchParams;
  const safeNext = next && next.startsWith("/") && !next.startsWith("//") ? next : "/";
  return (
    <main>
      <h1>LUMI</h1>
      <p className="dim">Aplikacja dla rodziny i opiekunów. Tu ustawiasz kontakty, radio i wygląd LUMI, a urządzenie samo pobiera zmiany.</p>
      {deleted && <div className="flash ok" role="status">Konto zostało usunięte.</div>}
      <LoginForm next={safeNext} />
      <p className="small"><a href="/prywatnosc">Prywatność - jakie dane przechowujemy</a></p>
    </main>
  );
}
