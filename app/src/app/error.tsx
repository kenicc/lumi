"use client";

// Zamiast pustego ekranu: zrozumiały komunikat i powrót
export default function Error({ error, reset }: { error: Error; reset: () => void }) {
  return (
    <main>
      <h1>Coś poszło nie tak</h1>
      <p>LUMI działa dalej na ostatnich ustawieniach. Spróbuj ponownie za chwilę.</p>
      <p className="dim small">{error.message}</p>
      <div className="row">
        <button className="btn primary" onClick={reset}>Spróbuj ponownie</button>
        <a className="btn" href="/">Strona główna</a>
      </div>
    </main>
  );
}
