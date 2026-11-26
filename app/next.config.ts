import type { NextConfig } from "next";

const config: NextConfig = {
  poweredByHeader: false,
  devIndicators: false,
  experimental: { serverActions: { bodySizeLimit: "1mb" } },
};

export default config;
