# Static showcase deployment

## Boundary

Only `showcase/public/` is deployed. The C++ simulator, SQLite database, Python
worker, TypeScript diagnostic process, fixtures, tests, AI records, and local
configuration remain source artifacts. There is no public ingestion, command,
administration, or credential endpoint.

The hosting project is `source-trace-edge-lab-demo`. Project identifiers and
the custom hostname are public deployment metadata, not secrets.

## Preconditions

1. Use an authenticated Firebase CLI with access to the isolated Google Cloud
   project.
2. Confirm the checkout is the intended public commit and clean:

   ```sh
   git status --short
   git rev-parse HEAD
   ```

3. Run the publication and hosting checks:

   ```sh
   tools/check-publication-scope.sh
   node --test showcase/tests/*.test.mjs
   ```

## Deploy

From the repository root:

```sh
firebase deploy --only hosting --project source-trace-edge-lab-demo
```

Firebase must report that only `showcase/public` was uploaded. Verify the
default Firebase URL first, then `https://demo.marquesco.com`.

## Live checks

```sh
curl --fail --silent --show-error --location \
  https://source-trace-edge-lab-demo.web.app/
curl --fail --silent --show-error --location \
  https://demo.marquesco.com/
curl --silent --show-error --head \
  https://demo.marquesco.com/
```

Confirm the response includes the committed CSP, `nosniff`, no-referrer policy,
frame denial, and permissions restrictions. Use a browser to run the recorded
trace and the valid/corrupt frame checks.

## Custom domain

Configure `demo.marquesco.com` from Firebase Hosting and publish only the DNS
records Firebase returns. Do not infer or reuse records from another site.
Keep the Firebase ownership record in DNS after verification so renewals remain
automated.

## Rollback

Use the Firebase Hosting release history to roll back to the immediately
preceding verified version. Do not delete the site or DNS records as a rollback
mechanism. After rollback, repeat the live checks and record the served release
and reason in an AI session note.
