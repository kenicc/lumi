import type { Metadata, Viewport } from "next";
// Czcionka z własnego serwera (paczka npm), nie z Google Fonts: szybciej i bez wysyłania adresu IP
// rodziny do Google przy każdym otwarciu aplikacji (RODO).
import "@fontsource/inter/400.css";
import "@fontsource/inter/500.css";
import "@fontsource/inter/600.css";
import "@fontsource/inter/700.css";
import "./globals.css";
import { SwRegister } from "@/components/SwRegister";

export const metadata: Metadata = {
  title: "LUMI - aplikacja rodziny",
  description: "Ustawienia LUMI Pocket dla rodziny i opiekunów",
  manifest: "/manifest.webmanifest",
  appleWebApp: { capable: true, title: "LUMI", statusBarStyle: "default" },
  icons: { icon: "/icons/icon-192.png", apple: "/icons/apple-touch-icon.png" },
};

export const viewport: Viewport = {
  width: "device-width",
  initialScale: 1,
  themeColor: [
    { media: "(prefers-color-scheme: light)", color: "#f7f6f2" },
    { media: "(prefers-color-scheme: dark)", color: "#0b0b0d" },
  ],
};

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="pl">
      <body>
        {children}
        <SwRegister />
      </body>
    </html>
  );
}
