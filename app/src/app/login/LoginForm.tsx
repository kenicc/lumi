"use client";
import { useRouter } from "next/navigation";
import { useState } from "react";
import { supabaseBrowser } from "@/lib/supabase/browser";

// Logowanie kodem z e-maila (nie linkiem): link otwiera Safari zamiast zainstalowanej aplikacji na iPhonie
export function LoginForm({ next }: { next: string }) {
  const router = useRouter();
  const [email, setEmail] = useState("");
  const [code, setCode] = useState("");
  const [step, setStep] = useState<"email" | "code">("email");
  const [busy, setBusy] = useState(false);
  const [err, setErr] = useState("");

  async function sendCode(e: React.FormEvent) {
    e.preventDefault();
    setBusy(true);
    setErr("");
    const { error } = await supabaseBrowser().auth.signInWithOtp({ email: email.trim(), options: { shouldCreateUser: true } });
    setBusy(false);
    if (error) setErr("Nie udało się wysłać kodu. Sprawdź adres i spróbuj ponownie.");
    else setStep("code");
  }

  async function verify(e: React.FormEvent) {
    e.preventDefault();
    setBusy(true);
    setErr("");
    const { error } = await supabaseBrowser().auth.verifyOtp({ email: email.trim(), token: code.trim(), type: "email" });
    setBusy(false);
    if (error) setErr("Kod jest nieprawidłowy albo wygasł.");
    else {
      router.replace(next);
      router.refresh();
    }
  }

  if (step === "email")
    return (
      <form onSubmit={sendCode} className="card">
        <label htmlFor="email">Twój e-mail</label>
        <input id="email" type="email" required autoComplete="email" value={email} onChange={(e) => setEmail(e.target.value)} />
        <p className="dim small">Wyślemy kod do logowania. Bez hasła.</p>
        {err && <div className="flash err">{err}</div>}
        <button className="btn primary block" disabled={busy}>{busy ? "Wysyłam..." : "Wyślij kod"}</button>
      </form>
    );
  return (
    <form onSubmit={verify} className="card">
      <p>Kod wysłaliśmy na <b>{email}</b>.</p>
      <label htmlFor="code">Kod z e-maila</label>
      <input id="code" type="text" inputMode="numeric" autoComplete="one-time-code" required maxLength={10} value={code} onChange={(e) => setCode(e.target.value)} />
      {err && <div className="flash err">{err}</div>}
      <button className="btn primary block" disabled={busy}>{busy ? "Sprawdzam..." : "Zaloguj"}</button>
      <button type="button" className="btn block" style={{ marginTop: 8 }} onClick={() => setStep("email")}>Zmień e-mail</button>
    </form>
  );
}
