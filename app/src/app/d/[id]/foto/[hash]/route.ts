// Zdjęcie kontaktu w aplikacji - tylko dla osób z dostępem do tego LUMI
import { NextResponse } from "next/server";
import { requireMember } from "@/lib/access";
import { admin } from "@/lib/supabase/admin";

export async function GET(_req: Request, { params }: { params: Promise<{ id: string; hash: string }> }) {
  const { id, hash } = await params;
  await requireMember(id);
  if (!/^[0-9a-f]{16}$/.test(hash)) return new NextResponse(null, { status: 404 });
  const { data } = await admin().storage.from("photos").download(`${id}/${hash}.jpg`);
  if (!data) return new NextResponse(null, { status: 404 });
  return new NextResponse(Buffer.from(await data.arrayBuffer()), {
    headers: { "Content-Type": "image/jpeg", "Cache-Control": "private, max-age=31536000, immutable" },
  });
}
