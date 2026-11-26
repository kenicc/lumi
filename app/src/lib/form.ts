// Odczyt pól formularza (FormData) bez powtarzania tych samych konwersji w każdej akcji
export const str = (fd: FormData, k: string) => String(fd.get(k) ?? "").trim();
export const num = (fd: FormData, k: string, def = 0) => {
  const v = Number(String(fd.get(k) ?? "").replace(",", "."));
  return Number.isFinite(v) ? v : def;
};
export const bool = (fd: FormData, k: string) => fd.get(k) === "on";
export const newId = () => crypto.randomUUID().replace(/-/g, "").slice(0, 10);

export function move<T>(arr: T[], i: number, dir: -1 | 1) {
  const j = i + dir;
  if (i < 0 || j < 0 || j >= arr.length) return;
  [arr[i], arr[j]] = [arr[j], arr[i]];
}
