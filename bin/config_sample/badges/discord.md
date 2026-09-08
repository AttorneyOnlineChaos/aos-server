# discord.json

## client_id

Example: `1234567890123456789`

From your Discord app. See [#where-to-find-them](#where-to-find-them).

## client_secret

Example: `GENERATED_SECRET`

From your Discord app. See [#where-to-find-them](#where-to-find-them).

## minimum_account_age (optional)

Default: `0`

Account must be this many days old. 0 = off.

## multi_factor (optional)

Default: `false`

Account must have 2FA on.

## email_verified (optional)

Default: `false`

Account must have a verified email.

## guild (optional)

Example: `1234567890123456789`

Discord server id. Player must be in it. Blank = off. Needed for the four below. See [#where-to-find-them](#where-to-find-them).

## roles (optional)

Example: `["1234567890123456789", "9876543210987654321"]`

Role ids. Player needs one of them. Empty = any. See [#where-to-find-them](#where-to-find-them).

## minimum_membership (optional)

Default: `0`

Player must be in the server this many days. 0 = off.

## require_rules_accepted (optional)

Default: `false`

Player must have accepted the server rules.

## refuse_timed_out (optional)

Default: `false`

No timed-out players.

## Where to find them

- id, secret, redirects: https://discord.com/developers/applications > your app > OAuth2. The redirect is `redirect_origin` from config.ini with `/auth` on the end, like `https://mydomain.org:8080/auth`.
- server and role ids: Discord > User Settings > Advanced > Developer Mode on, then right-click > Copy ID.
