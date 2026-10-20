<div align="center">

<h1 align="center">Verdict</h1>

<p align="center">
  <img src="https://img.shields.io/badge/CS2-0d1117?style=for-the-badge" alt="CS2">
  <img src="https://img.shields.io/badge/LINUX_X64-e5383b?style=for-the-badge" alt="Linux x64">
  <img src="https://img.shields.io/badge/METAMOD:SOURCE-0d1117?style=for-the-badge" alt="Metamod:Source">
  <img src="https://img.shields.io/badge/BUILD_1483%2B-e5383b?style=for-the-badge" alt="Build 1483+">
  <img src="https://img.shields.io/badge/PLUGIN-0d1117?style=for-the-badge" alt="Plugin">
  <img src="https://img.shields.io/badge/C%2B%2B-e5383b?style=for-the-badge" alt="C++">
  <img src="https://img.shields.io/badge/MYSQL_%2F_MARIADB-0d1117?style=for-the-badge" alt="MySQL / MariaDB">
  <img src="https://img.shields.io/badge/READY-e5383b?style=for-the-badge" alt="Ready">
</p>

<p align="center"><b>English</b> · <a href="README.ru.md">Русский</a></p>
</div>

Metamod:Source plugin for CS2: bans, mutes and admins. Replaces **cs2-admin_system** and the server side of **Material Admin**.

- **Material Admin schema.** Tables `sb_bans`, `sb_comms`, `sb_admins`, `sb_srvgroups`, `sb_servers`. An old MA database from CS:GO can be connected as is.
- **admin_system compatible.** Exposes the same `IAdminApi`: VIP, LR and other modules work without a rebuild. Configs use the admin_system format, `core.ini` carries over almost unchanged.
- **[Web panel](https://github.com/whyn0tq/cs2-verdict-web)** — PHP 5.2+, runs on ordinary hosting and shares the same database.

## Features

| | |
|---|---|
| Punishments | ban by SteamID and IP (including offline), mute, gag, kick |
| Admins | flags, immunity, groups, expiry, per-server binding, group limits on ban and mute duration |
| Player control | slay, slap, burn, timebomb, firebomb, freezebomb, freeze, respawn, team, rename, noclip, god, hp, armor, speed, gravity |
| Server | map change (including workshop), restart, swap teams, exec configs, RCON |
| Votes | kick, ban, mute, map change |
| Other | `!admin` menu, several servers on one database, migration from admin_system (`vd_import`), ru / en / uk languages |

## Requirements

| What | Where to get it |
|---|---|
| Metamod:Source 2.0 (build 1483+) | [metamodsource.net](https://www.metamodsource.net/downloads.php?branch=dev) |
| cs2-menus (`utils.zip`) | [Pisex/cs2-menus](https://github.com/whyn0tq/cs2-menus/releases) |
| sql_mm (`sql_mm-linux-*.tar.gz`) | [zer0k-z/sql_mm](https://github.com/zer0k-z/sql_mm/releases) |
| MySQL 8 / MariaDB 10.6+ | hosting control panel |

## Installation

1. Remove cs2-admin_system if installed: the `addons/admin_system/` folder and `addons/metamod/admin_system.vdf` (two `IAdminApi` providers cannot run together; keep `addons/translations/admin_system.phrases.txt` — Verdict reads it for modules).
2. Create an empty database — Verdict creates its tables itself. In `addons/configs/databases.cfg` (copy it from `databases.cfg.example` if missing) add a section:

   ```
   "Databases"
   {
       "verdict"
       {
           "host"      "127.0.0.1"
           "user"      "verdict"
           "pass"      "password"
           "database"  "verdict"
           "port"      "3306"
       }
   }
   ```

   Leave other plugins' sections (`vip`, `lr`…) alone. Without a `verdict` section the plugin falls back to `admin_system`, then `materialadmin`.
3. Restart the server. The console should print `[Verdict] Database ready, server id 0`, and `meta list` should show Verdict.
4. Add an admin from the server console:

   ```
   vd_add_admin 76561198000000000 "Name" z 100 0
   ```

   `z` — full access, `100` — immunity, `0` — permanent.

## Commands

Use the `vd_` prefix in the console and RCON, `!` in chat. Access is controlled by admin flags.

| Command | Arguments |
|---|---|
| `ban` `mute` `gag` `silence` | `<name\|#slot\|steamid> <duration> [reason]` |
| `unban` `unmute` `ungag` `unsilence` | `<name\|#slot\|steamid\|ip> [reason]` |
| `banip` | `<ip> <duration> [reason]` |
| `kick` | `<name\|#slot\|steamid> [reason]` |
| `add_admin` | `<target> <name> <flags> <immunity> <duration> [group id] [comment]` |
| `remove_admin` | `<target>` |
| `add_group` / `remove_group` | `<name> <flags> <immunity>` / `<id\|name>` |
| `slay` `noclip` `respawn` `god` `timebomb` `firebomb` `freezebomb` | `<target>` |
| `slap` | `<target> [damage]` |
| `burn` | `<target> [seconds]` |
| `freeze` | `<target> [seconds]`, `0` — unfreeze |
| `hp` `armor` `speed` `gravity` | `<target> <value>` |
| `team` | `<target> <t\|ct\|spec>` |
| `rename` | `<target> <new name>` |
| `map` / `votemap` | `<map\|workshop id>` |
| `votekick` `voteban` `votemute` | `<target>` |
| `restart` `swapteams` `who` | — |
| `rcon` | `<command>` |
| `reload` `reload_admin` `reload_punish` `sync` | reload config / admins / punishments, sync with the database |
| `import` | `[table prefix]`, default `as_`. Console only: imports admins, groups and punishments from cs2-admin_system tables in the same database |

**Duration** is in minutes, `0` — permanent.

## Configs

`addons/configs/verdict/`:

- `core.ini` — table prefix (`database_prefix`, default `sb`), `server_id`, chat prefix, punishment mode (`punish_scope`, `punish_ip`), vote settings, effects, the list of configs for `exec`, flag definitions, ban and mute reasons and durations.
- `sorting.ini` — menu item order.
- `maps.ini` — map list for map change and votes.

Admin flags (defaults from `core.ini`):

| Flag | Access |
|---|---|
| `a` | reserved slot |
| `b` | `status`, `who`, admin list, admin chat |
| `c` | `kick` |
| `d` | `ban`, custom reason text |
| `e` | `unban` |
| `f` | `slay`, `slap`, `burn`, `timebomb`, `firebomb`, `freezebomb`, `freeze`, `rename`, `team`, `respawn` |
| `g` | `map`, `restart`, `swapteams` |
| `i` | `reload`, exec configs |
| `j` | `mute`, `gag`, `silence` and their removal |
| `k` | votes: `votekick`, `voteban`, `votemute`, `votemap` |
| `m` | `rcon` |
| `n` | `noclip`, `god`, `hp`, `armor`, `speed`, `gravity` |
| `z` | ROOT — full access |

Several servers on one database: each has its own `server_id`; bans and admins are either shared or separate, controlled by `punish_scope`.

## Author

**teesa (whyn0tq)** · [GitHub](https://github.com/whyn0tq)
