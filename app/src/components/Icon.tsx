// Ikony liniowe w jednym stylu (24x24, kreska 1,75, kolor dziedziczony z tekstu).
// Zamiast emoji: wyglądają tak samo na każdym telefonie i nie krzyczą kolorami.
const PATHS = {
  phone:
    "M5 4h3.2l1.6 4-2 1.3a10.5 10.5 0 0 0 5 5l1.3-2 4 1.6V17a2 2 0 0 1-2 2A15 15 0 0 1 3 6a2 2 0 0 1 2-2z",
  plus: "M12 5v14M5 12h14",
  home: "M4 11 12 4l8 7M6 9.5V20h4.5v-5h3v5H18V9.5",
  message: "M5 5h14a1 1 0 0 1 1 1v10a1 1 0 0 1-1 1H10l-4 3v-3H5a1 1 0 0 1-1-1V6a1 1 0 0 1 1-1z",
  alarm: "M12 21a8 8 0 1 0 0-16 8 8 0 0 0 0 16zM12 9v4l2.5 1.5M4 4 2 6M20 4l2 2",
  radio: "M4 9h16v11H4zM7 9l9-5M8 16.5a2 2 0 1 0 0-.01M13 13h4M13 16h4",
  sun: "M12 16a4 4 0 1 0 0-8 4 4 0 0 0 0 8zM12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4",
  cloud: "M7 18h10.5a4 4 0 0 0 .6-7.96A6 6 0 0 0 6.6 9.1 4.5 4.5 0 0 0 7 18z",
  access: "M12 6.5a1.75 1.75 0 1 0 0-3.5 1.75 1.75 0 0 0 0 3.5zM5 9l7 1.5L19 9M12 10.5V15l-3 6M12 15l3 6",
  wifi: "M2.5 9a14 14 0 0 1 19 0M5.5 12.5a9.5 9.5 0 0 1 13 0M8.5 16a5 5 0 0 1 7 0M12 19.5h.01",
  users:
    "M9 11a3.5 3.5 0 1 0 0-7 3.5 3.5 0 0 0 0 7zM2.5 20a6.5 6.5 0 0 1 13 0M16 4.3a3.5 3.5 0 0 1 0 6.4M18 14a6.5 6.5 0 0 1 3.5 6",
} as const;

export type IconName = keyof typeof PATHS;

export function Icon({ name, size = 22 }: { name: IconName; size?: number }) {
  return (
    <svg
      width={size}
      height={size}
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth={1.75}
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden
    >
      <path d={PATHS[name]} />
    </svg>
  );
}
