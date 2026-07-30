import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";

const config = JSON.parse(
  await readFile(new URL("../../firebase.json", import.meta.url), "utf8"),
);

test("Firebase deploys only the static reviewer artifact", () => {
  assert.equal(config.hosting.public, "showcase/public");
  assert.equal("rewrites" in config.hosting, false);
  assert.equal("functions" in config, false);
});

test("hosting applies a restrictive browser policy", () => {
  const globalHeaders = config.hosting.headers.find(
    (entry) => entry.source === "**",
  ).headers;
  const headers = Object.fromEntries(
    globalHeaders.map(({ key, value }) => [key, value]),
  );

  assert.match(headers["Content-Security-Policy"], /connect-src 'none'/);
  assert.match(headers["Content-Security-Policy"], /frame-ancestors 'none'/);
  assert.equal(headers["X-Content-Type-Options"], "nosniff");
  assert.equal(headers["Referrer-Policy"], "no-referrer");
  assert.equal(headers["X-Frame-Options"], "DENY");
});
