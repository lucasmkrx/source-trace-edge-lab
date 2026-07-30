# AI session: live cache-header correction

- Date: 2026-07-30
- Trigger: first live inspection of the Firebase Hosting default URL.
- AI tool: OpenAI Codex.

## Observed evidence

- The deployed `/` response body matched the committed `index.html` SHA-256
  exactly.
- CSP, MIME-sniffing denial, frame denial, permissions policy, no-referrer
  policy, and cross-origin opener policy were present.
- The clean root URL returned Firebase's default `Cache-Control: max-age=3600`.
  The HTML-specific glob did not match the public clean URL as intended.

## Correction

Set the global default cache policy to `max-age=0, must-revalidate`, then retain
the later one-hour policy for CSS and JavaScript assets. Extend the hosting
contract test so a future configuration change cannot silently restore stale
HTML caching.

## Acceptance

The correction is complete only when the deployed clean root URL returns the
revalidation policy and the live body still matches the committed artifact.
