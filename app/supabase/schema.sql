-- LUMI - schemat bazy (Supabase / Postgres). Uruchom w SQL Editor nowego projektu.
--
-- Zasada bezpieczeństwa: RLS włączone, a zasad (policies) brak. Przeglądarka nie czyta ani nie
-- zapisuje tabel bezpośrednio - wszystko idzie przez serwer aplikacji (klucz serwisowy), który
-- sprawdza rolę użytkownika przy każdym zapytaniu (src/lib/access.ts).

create table if not exists profiles (
  user_id uuid primary key references auth.users on delete cascade,
  display_name text not null check (char_length(display_name) between 1 and 24),
  -- forma czasownika w komunikacie na urządzeniu: f "Ania dodała", m "Marek dodał", n "Ania dodaje"
  form text not null default 'n' check (form in ('f', 'm', 'n')),
  created_at timestamptz not null default now()
);

create table if not exists devices (
  id text primary key check (id ~ '^LUMI-[0-9A-F]{6}$'),
  secret_hash text not null,          -- sha256 sekretu urządzenia
  pair_hash text,                     -- sha256 kodu parowania z ekranu LUMI
  name text,                          -- "Mama", "Dziadek Józef"
  settings jsonb,                     -- ustawienia edytowane w aplikacji (src/lib/schema.ts)
  config jsonb,                       -- dokument wysyłany do urządzenia
  config_version int not null default 0,
  applied_version int not null default 0,  -- wersja potwierdzona przez urządzenie
  status jsonb,                       -- bateria, zasięg, wersja oprogramowania
  variant text,
  fw text,
  last_seen timestamptz,
  created_at timestamptz not null default now()
);

create table if not exists members (
  device_id text not null references devices on delete cascade,
  user_id uuid not null references auth.users on delete cascade,
  role text not null check (role in ('guardian', 'family', 'friend')),
  created_at timestamptz not null default now(),
  primary key (device_id, user_id)
);
create index if not exists members_user on members (user_id);

create table if not exists invites (
  id uuid primary key default gen_random_uuid(),
  device_id text not null references devices on delete cascade,
  email text not null,
  role text not null check (role in ('family', 'friend')),
  created_by uuid references auth.users on delete set null,
  created_at timestamptz not null default now(),
  unique (device_id, email)
);
create index if not exists invites_email on invites (lower(email));

create table if not exists changes (
  id bigserial primary key,
  device_id text not null references devices on delete cascade,
  user_id uuid references auth.users on delete set null,
  author text not null,
  summary text not null,
  version int not null,
  created_at timestamptz not null default now()
);
create index if not exists changes_device on changes (device_id, created_at desc);

alter table profiles enable row level security;
alter table devices enable row level security;
alter table members enable row level security;
alter table invites enable row level security;
alter table changes enable row level security;

-- Muzyka od rodziny: publiczny kosz z losowymi nazwami plików (urządzenie pobiera MP3 bez logowania).
-- Wgrywanie tylko przez podpisany adres wydawany przez serwer po sprawdzeniu roli.
insert into storage.buckets (id, name, public, file_size_limit, allowed_mime_types)
values ('music', 'music', true, 20971520, array['audio/mpeg'])
on conflict (id) do nothing;
