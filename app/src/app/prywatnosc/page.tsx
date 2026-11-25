import Link from "next/link";
import { RETENTION_DAYS } from "@/lib/account";

// Informacja o danych (RODO, art. 13) - dostępna bez logowania
export default function Privacy() {
  return (
    <main>
      <h1>Prywatność</h1>
      <p>LUMI zbiera tylko to, co jest potrzebne, żeby rodzina mogła ustawić urządzenie i wiedzieć, że działa.</p>

      <h2>Co przechowujemy</h2>
      <ul>
        <li><b>Twoje konto:</b> e-mail (do logowania kodem) i imię, które widzi użytkownik LUMI.</li>
        <li><b>Ustawienia LUMI:</b> kontakty (imię, numer, zdjęcie), stacje, przypomnienia, wygląd, sieci Wi-Fi.</li>
        <li><b>Wiadomości</b> od rodziny i informację, kiedy zostały przeczytane.</li>
        <li><b>Potwierdzenia przypomnień</b> (zrobione, odłożone, bez potwierdzenia).</li>
        <li><b>Stan urządzenia:</b> bateria, zasięg, wersja oprogramowania, ostatnie błędy techniczne.</li>
      </ul>

      <h2>Czego nie zbieramy</h2>
      <ul>
        <li>lokalizacji,</li>
        <li>treści rozmów ani listy połączeń,</li>
        <li>historii słuchania radia i muzyki,</li>
        <li>numerów telefonów w dziennikach błędów.</li>
      </ul>
      <p>Mikrofon LUMI działa tylko w trakcie rozmowy telefonicznej - nie da się go włączyć zdalnie.</p>

      <h2>Kto widzi dane</h2>
      <p>
        Tylko osoby dodane do danego LUMI (główny opiekun, rodzina, znajomi - znajomi nie widzą kontaktów). Zdjęcia kontaktów
        leżą w prywatnym magazynie: pobiera je tylko to urządzenie i osoby z dostępem.
      </p>

      <h2>Jak długo</h2>
      <ul>
        <li>potwierdzenia przypomnień: {RETENTION_DAYS.reminderEvents} dni,</li>
        <li>wiadomości: {RETENTION_DAYS.messages} dni,</li>
        <li>historia zmian ustawień: {RETENTION_DAYS.changes} dni,</li>
        <li>ustawienia i konto: do usunięcia LUMI z aplikacji albo usunięcia konta.</li>
      </ul>

      <h2>Twoje prawa</h2>
      <p>
        Konto usuniesz sam w <Link href="/profil">profilu</Link>. Jeśli jesteś głównym opiekunem, rola przejdzie na osobę z rodziny;
        gdy nikogo innego nie ma, LUMI i jego dane zostaną usunięte. LUMI usuwa z aplikacji główny opiekun (Osoby → Usuń LUMI).
      </p>
      <p className="dim small">Administratorem danych jest osoba lub firma, która uruchomiła tę instancję aplikacji LUMI.</p>
    </main>
  );
}
