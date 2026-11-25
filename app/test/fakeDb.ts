// Atrapa klienta Supabase w pamięci - tylko metody używane przez trasy API urządzenia
type Row = Record<string, unknown>;

export function fakeDb() {
  const tables: Record<string, Row[]> = { devices: [], members: [], changes: [], profiles: [], invites: [], firmware_releases: [] };

  function query(table: string) {
    const filters: ((r: Row) => boolean)[] = [];
    let op: "select" | "update" | "insert" | "delete" = "select";
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
      gt(k: string, v: number) {
        filters.push((r) => (r[k] as number) > v);
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
        tables[table].push(...list.map((r) => ({ ...r })));
        return list;
      }
      const matched = rows();
      if (op === "update") matched.forEach((r) => Object.assign(r, payload));
      if (op === "delete") tables[table] = tables[table].filter((r) => !matched.includes(r));
      if (sortBy) {
        const { k, asc } = sortBy;
        matched.sort((a, b) => ((a[k] as number) - (b[k] as number)) * (asc ? 1 : -1));
      }
      return head ? [] : matched.slice(0, lim);
    }
    // count dla select(..., {count, head}) - zwracamy liczbę dopasowanych
    const origThen = q.then;
    q.then = (resolve) => {
      if (head) return resolve({ data: null, error: null, count: rows().length });
      return origThen(resolve);
    };
    return q;
  }
  return { tables, client: { from: query } };
}
