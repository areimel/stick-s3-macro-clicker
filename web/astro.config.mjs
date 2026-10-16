import { defineConfig } from "astro/config";

export default defineConfig({
  site: "https://bemarketing-hosting.github.io",
  base: process.env.BASE_PATH || "/stick-s3-macro-clicker/",
  output: "static",
});
