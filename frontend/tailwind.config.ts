import type { Config } from "tailwindcss";

const config: Config = {
  content: ["./index.html", "./src/**/*.{ts,tsx}"],
  theme: {
    extend: {
      colors: {
        primary: "#2DD4BF",
        "background-light": "#F0F9FF",
        "background-sidebar": "rgba(236, 253, 255, 0.85)",
        "text-dark": "#0F172A",
        "text-light": "#64748B",
        "accent-coral": "#F97316",
      },
      fontFamily: {
        sans: ["Inter", "system-ui", "-apple-system", "BlinkMacSystemFont", "Segoe UI", "sans-serif"],
      },
      boxShadow: {
        card: "0 20px 45px -30px rgba(15, 23, 42, 0.6)",
      },
    },
  },
  plugins: [],
};

export default config;
