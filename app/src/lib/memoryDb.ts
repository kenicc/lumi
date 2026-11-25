// Baza w pamięci udająca klienta Supabase - tylko metody używane przez aplikację.
// Używana w testach i w trybie demo (LUMI_DEMO=1).
type Row = Record<string, unknown>;

export function memoryDb() {
  const tables: Record<string, Row[]> = { devices: [], members: [], changes: [], profiles: [], invites: [], firmware_releases: [], push_subscriptions: [], reminder_events: [], messages: [] };

  let seq = 1000;
  function query(table: string) {
    const filters: ((r: Row) => boolean)[] = [];
    let op: "select" | "update" | "insert" | "delete" | "upsert" = "select";
    let conflict: string[] = [];
    let payload: Row | Row[] = {};
    let head = false;
    let sortBy: { k: string; asc: boolean } | null = null;
    let lim = Infinity;
    const rows = () => tables[table].filter((r) => filters.every((f) => f(r)));
    const q = {
      select(_cols?: string, opts?: { count?: string; head?: boolean }) {
        head = !!opts?.head;
        return q;
      },
      eq(k: string, v: unknown) {
        filters.push((r) => r[k] === v);
        return q;
      },
      gt(k: string, v: number | string) {
        filters.push((r) => r[k] != null && (r[k] as number | string) > v);
        return q;
      },
      lt(k: string, v: number | string) {
        filters.push((r) => r[k] != null && (r[k] as number | string) < v);
        return q;
      },
      order(k: string, opts?: { ascending?: boolean }) {
        sortBy = { k, asc: opts?.ascending !== false };
        return q;
      },
      limit(n: number) {
        lim = n;
        return q;
      },
      in(k: string, v: unknown[]) {
        filters.push((r) => v.includes(r[k]));
        return q;
      },
      update(p: Row) {
        op = "update";
        payload = p;
        return q;
      },
      insert(p: Row | Row[]) {
        op = "insert";
        payload = p;
        return q;
      },
      upsert(p: Row, opts?: { onConflict?: string; ignoreDuplicates?: boolean }) {
        op = "upsert";
        payload = p;
        conflict = opts?.onConflict?.split(",") ?? [Object.keys(p)[0]];
        return q;
      },
      is(k: string, v: null) {
        filters.push((r) => (r[k] ?? null) === v);
        return q;
      },
      ilike(k: string, v: string) {
        filters.push((r) => String(r[k] ?? "").toLowerCase() === v.toLowerCase());
        return q;
      },
      delete() {
        op = "delete";
        return q;
      },
      async maybeSingle() {
        return { data: run()[0] ?? null, error: null };
      },
      async single() {
        const d = run()[0];
        return { data: d ?? null, error: d ? null : { message: "not found" } };
      },
      then(resolve: (v: unknown) => void) {
        const data = run();
        resolve({ data, error: null, count: data.length });
      },
    };
    function run(): Row[] {
      if (op === "insert") {
        const list = Array.isArray(payload) ? payload : [payload];
        tables[table].push(...list.map((r) => ({ id: r.id ?? (seq += 1), ...r })));
        return list;
      }
      if (op === "upsert") {
        const p = payload as Row;
        const hit = tables[table].find((r) => conflict.every((c) => r[c] === p[c]));
        if (hit) Object.assign(hit, p);
        else tables[table].push({ ...p });
        return [p];
      }
      const matched = rows();
      if (op === "update") matched.forEach((r) => Object.assign(r, payload));
      if (op === "delete") tables[table] = tables[table].filter((r) => !matched.includes(r));
      if (sortBy) {
        const { k, asc } = sortBy;
        matched.sort((a, b) => ((a[k] as number) - (b[k] as number)) * (asc ? 1 : -1));
      }
      // jak prawdziwa baza: kopie, nie żywe wiersze
      return head ? [] : matched.slice(0, lim).map((r) => structuredClone(r));
    }
    // count dla select(..., {count, head}) - zwracamy liczbę dopasowanych
    const origThen = q.then;
    q.then = (resolve) => {
      if (head) return resolve({ data: null, error: null, count: rows().length });
      return origThen(resolve);
    };
    return q;
  }
  // magazyn plików w pamięci (kosze Supabase Storage)
  const files = new Map<string, Buffer>();
  const storage = {
    from(bucket: string) {
      const key = (p: string) => `${bucket}/${p}`;
      return {
        async upload(p: string, data: Buffer | Uint8Array) {
          files.set(key(p), Buffer.from(data));
          return { data: { path: p }, error: null };
        },
        async download(p: string) {
          const b = files.get(key(p));
          return b ? { data: new Blob([new Uint8Array(b)]), error: null } : { data: null, error: { message: "not found" } };
        },
        async remove(paths: string[]) {
          paths.forEach((p) => files.delete(key(p)));
          return { data: null, error: null };
        },
        async createSignedUploadUrl(p: string) {
          return { data: { path: p, token: "demo" }, error: null };
        },
        getPublicUrl(p: string) {
          return { data: { publicUrl: `https://demo.invalid/${key(p)}` } };
        },
      };
    },
  };
  return { tables, files, client: { from: query, storage } };
}
