# gmail.json

## client_id

Example: `1234567890-foobar.apps.googleusercontent.com`

From your Google OAuth client. See [#where-to-find-them](#where-to-find-them).

## client_secret

Example: `GENERATED_SECRET`

From your Google OAuth client. See [#where-to-find-them](#where-to-find-them).

## personal_only (optional)

Default: `true`

true = gmail.com accounts only. false = Workspace accounts too.

## hosted_domain (optional)

Example: `mydomain.org`

Only with personal_only off. Workspace domain the account must be from. Blank = any.

## Where to find them

- id, secret, redirects: https://console.cloud.google.com/apis/credentials > Create credentials > OAuth client ID > Web application. The redirect URI is `redirect_origin` from config.ini with `/auth` on the end, like `https://mydomain.org:8080/auth`.
