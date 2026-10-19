<div align="center">

<img src="assets/banner.svg" alt="Verdict" width="100%">

![CS2](https://img.shields.io/badge/CS2-Linux%20x64-e5383b?style=for-the-badge&labelColor=16161a) ![Metamod](https://img.shields.io/badge/Metamod:Source-2.0-e5383b?style=for-the-badge&labelColor=16161a) ![C++](https://img.shields.io/badge/plugin-C%2B%2B-e5383b?style=for-the-badge&labelColor=16161a) ![DB](https://img.shields.io/badge/MySQL%20%2F%20MariaDB-ready-e5383b?style=for-the-badge&labelColor=16161a)

**English** · [Русский](README.ru.md)

[Features](#features) · [Installation](#installation) · [Commands](#commands) · [Flags](#admin-flags) · [Web panel](#web-panel) · [Build](#build)

</div>

Replaces **cs2-admin_system** and the server side of **Material Admin**. The web panel, [**Verdict Web**](https://github.com/whyn0tq/cs2-verdict-web), is a separate repository: the panel and the plugin share one database and talk only through it.

- **No new database.** Runs on the Material Admin schema (`sb_bans`, `sb_comms`, `sb_admins`, `sb_srvgroups`, `sb_servers`). An old MA database plugs in as is.
- **Modules keep working.** Exposes the same `IAdminApi` as cs2-admin_system, so admin_system, VIP, LR and other modules need no rebuild.
- **Easy migration.** Configs use the admin_system format; data moves over with `vd_import`.

## Features

- Bans by SteamID and IP, including offline players
- Mutes, gags and silence
- Admins with flags, immunity, groups, expiry and per-server binding
- Per-group limits on ban and mute duration
- `!admin` menu for players and server management
- Votes: kick, ban, mute, map
- Player tools: slay, slap, burn, bombs, freeze, team, noclip, god, HP, armor, gravity, speed
- Map change (workshop too), restart, swap teams, config exec, RCON
- Many servers on one database, shared or separate admins and bans
- Languages: English, Russian, Ukrainian

## Requirements

| What | Where |
|---|---|
| CS2 server on **Linux** x86_64 | — |
| Metamod:Source **2.0** (dev build) | [metamodsource.net](https://www.metamodsource.net/downloads.php?branch=dev) |
| cs2-menus (`utils.zip`) | [Pisex/cs2-menus](https://github.com/Pisex/cs2-menus/releases) |
| sql_mm (`sql_mm-linux-*.tar.gz`) | [zer0k-z/sql_mm](https://github.com/zer0k-z/sql_mm/releases) |
| MySQL 8 or MariaDB 10.6+ | your hosting panel |

All paths are relative to the server folder that contains `game/csgo/`.

## Installation

**1. Metamod.** Extract Metamod 2.0 into `game/csgo/`, then add this line to `game/csgo/gameinfo.gi` inside `SearchPaths`, **before** `Game csgo`:

```
Game    csgo/addons/metamod
```

CS2 updates overwrite `gameinfo.gi` — re-add the line after each update. Check with `meta version` in the server console.

**2. Dependencies.** Extract `utils.zip` (cs2-menus) and the sql_mm archive into `game/csgo/`.

**3. Verdict.** Remove cs2-admin_system if installed (`addons/admin_system/` and `addons/metamod/admin_system.vdf`; keep `admin_system.phrases.txt`). Download `verdict-linux.zip` from *Releases* and copy its `addons` folder into `game/csgo/`.

**4. Database.** Create a database, then put its details into `addons/configs/databases.cfg` (rename `databases.cfg.example` if the file is missing). Tables are created automatically.

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

If there is no `verdict` section, the plugin falls back to `admin_system` or `materialadmin`.

**5. Check.** Restart the server. The console should print `[Verdict] Database ready`, and `meta list` should show Verdict.

**6. First admin.** Run in the server console or via RCON:

```
vd_add_admin <steamid64> "Nick" z 100 0
```

`z` — full access, `100` — immunity, `0` — permanent. Then type `!admin` in chat.

## Commands

`!admin` opens the menu. In the server console and RCON, commands start with `vd_` (`vd_ban`); in chat, with `!` (`!ban`). **Access** is the permission a player needs.

`<target>` is a name, `#slot` or SteamID. `<duration>` is a number with an optional unit: `s`, `m` (default), `h`, `d`, `w` — for example `30`, `2h`, `7d`; `0` is permanent.

### Punishments

<table width="100%">
<tr><th width="20%" align="left">Command</th><th width="55%" align="left">Usage</th><th width="25%" align="left">Access</th></tr>
<tr><td><code>ban</code></td><td><code>&lt;target&gt; &lt;duration&gt; [reason]</code></td><td><code>@admin/ban</code></td></tr>
<tr><td><code>banip</code></td><td><code>&lt;ip&gt; &lt;duration&gt; [reason]</code></td><td><code>@admin/ban</code></td></tr>
<tr><td><code>mute</code></td><td><code>&lt;target&gt; &lt;duration&gt; [reason]</code></td><td><code>@admin/mute</code></td></tr>
<tr><td><code>gag</code></td><td><code>&lt;target&gt; &lt;duration&gt; [reason]</code></td><td><code>@admin/gag</code></td></tr>
<tr><td><code>silence</code></td><td><code>&lt;target&gt; &lt;duration&gt; [reason]</code></td><td><code>@admin/silence</code></td></tr>
<tr><td><code>unban</code></td><td><code>&lt;target|ip&gt; [reason]</code></td><td><code>@admin/unban</code></td></tr>
<tr><td><code>unmute</code></td><td><code>&lt;target&gt; [reason]</code></td><td><code>@admin/unmute</code></td></tr>
<tr><td><code>ungag</code></td><td><code>&lt;target&gt; [reason]</code></td><td><code>@admin/ungag</code></td></tr>
<tr><td><code>unsilence</code></td><td><code>&lt;target&gt; [reason]</code></td><td><code>@admin/unsilence</code></td></tr>
<tr><td><code>kick</code></td><td><code>&lt;target&gt; [reason]</code></td><td><code>@admin/kick</code></td></tr>
</table>

### Players

<table width="100%">
<tr><th width="20%" align="left">Command</th><th width="55%" align="left">Usage</th><th width="25%" align="left">Access</th></tr>
<tr><td><code>slay</code></td><td><code>&lt;target&gt;</code></td><td><code>@admin/kill</code></td></tr>
<tr><td><code>slap</code></td><td><code>&lt;target&gt; [damage]</code></td><td><code>@admin/slap</code></td></tr>
<tr><td><code>burn</code></td><td><code>&lt;target&gt; [seconds]</code></td><td><code>@admin/burn</code></td></tr>
<tr><td><code>freeze</code></td><td><code>&lt;target&gt; [seconds]</code> — 0 unfreezes</td><td><code>@admin/freeze</code></td></tr>
<tr><td><code>timebomb</code></td><td><code>&lt;target&gt;</code> — toggle</td><td><code>@admin/timebomb</code></td></tr>
<tr><td><code>firebomb</code></td><td><code>&lt;target&gt;</code> — toggle</td><td><code>@admin/firebomb</code></td></tr>
<tr><td><code>freezebomb</code></td><td><code>&lt;target&gt;</code> — toggle</td><td><code>@admin/freezebomb</code></td></tr>
<tr><td><code>respawn</code></td><td><code>&lt;target&gt;</code></td><td><code>@admin/respawn</code></td></tr>
<tr><td><code>rename</code></td><td><code>&lt;target&gt; &lt;new name&gt;</code></td><td><code>@admin/rename</code></td></tr>
<tr><td><code>team</code></td><td><code>&lt;target&gt; &lt;t|ct|spec&gt;</code></td><td><code>@admin/changeteam</code></td></tr>
<tr><td><code>noclip</code></td><td><code>&lt;target&gt;</code> — toggle</td><td><code>@admin/noclip</code></td></tr>
<tr><td><code>god</code></td><td><code>&lt;target&gt;</code> — toggle</td><td><code>@admin/god</code></td></tr>
<tr><td><code>hp</code></td><td><code>&lt;target&gt; &lt;value&gt;</code></td><td><code>@admin/hp</code></td></tr>
<tr><td><code>armor</code></td><td><code>&lt;target&gt; &lt;value&gt;</code></td><td><code>@admin/armor</code></td></tr>
<tr><td><code>speed</code></td><td><code>&lt;target&gt; &lt;value&gt;</code></td><td><code>@admin/speed</code></td></tr>
<tr><td><code>gravity</code></td><td><code>&lt;target&gt; &lt;value&gt;</code></td><td><code>@admin/gravity</code></td></tr>
<tr><td><code>who</code></td><td><code>[target]</code> — player info, yourself if omitted</td><td><code>@admin/who</code></td></tr>
</table>

### Server

<table width="100%">
<tr><th width="20%" align="left">Command</th><th width="55%" align="left">Usage</th><th width="25%" align="left">Access</th></tr>
<tr><td><code>map</code></td><td><code>&lt;map|workshop id&gt;</code></td><td><code>@admin/maps</code></td></tr>
<tr><td><code>restart</code></td><td>—</td><td><code>@admin/restart</code></td></tr>
<tr><td><code>swapteams</code></td><td>—</td><td><code>@admin/swapteams</code></td></tr>
<tr><td><code>rcon</code></td><td><code>&lt;server command&gt;</code></td><td><code>@admin/rcon</code></td></tr>
</table>

### Votes

<table width="100%">
<tr><th width="20%" align="left">Command</th><th width="55%" align="left">Usage</th><th width="25%" align="left">Access</th></tr>
<tr><td><code>votekick</code></td><td><code>&lt;target&gt;</code></td><td><code>@admin/vote</code></td></tr>
<tr><td><code>voteban</code></td><td><code>&lt;target&gt;</code></td><td><code>@admin/vote</code></td></tr>
<tr><td><code>votemute</code></td><td><code>&lt;target&gt;</code></td><td><code>@admin/vote</code></td></tr>
<tr><td><code>votemap</code></td><td><code>&lt;map|workshop id&gt;</code></td><td><code>@admin/vote</code></td></tr>
</table>

### Admins and groups

<table width="100%">
<tr><th width="20%" align="left">Command</th><th width="55%" align="left">Usage</th><th width="25%" align="left">Access</th></tr>
<tr><td><code>add_admin</code></td><td><code>&lt;target&gt; &lt;name&gt; &lt;flags&gt; &lt;immunity&gt; &lt;duration&gt; [group id] [comment]</code></td><td><code>@admin/add_admin</code></td></tr>
<tr><td><code>remove_admin</code></td><td><code>&lt;target&gt;</code></td><td><code>@admin/remove_admin</code></td></tr>
<tr><td><code>add_group</code></td><td><code>&lt;name&gt; &lt;flags&gt; &lt;immunity&gt;</code></td><td><code>@admin/add_group</code></td></tr>
<tr><td><code>remove_group</code></td><td><code>&lt;id|name&gt;</code></td><td><code>@admin/remove_group</code></td></tr>
</table>

### Maintenance

<table width="100%">
<tr><th width="20%" align="left">Command</th><th width="55%" align="left">Usage</th><th width="25%" align="left">Access</th></tr>
<tr><td><code>vd_reload</code></td><td>reload config and phrases</td><td><code>@admin/reload_config</code></td></tr>
<tr><td><code>vd_reload_admin</code></td><td><code>&lt;target&gt;</code> — reload an online player's admin rights</td><td><code>@admin/reload_admin</code></td></tr>
<tr><td><code>vd_reload_punish</code></td><td><code>&lt;target&gt;</code> — reload an online player's punishments</td><td><code>@admin/reload_punish</code></td></tr>
<tr><td><code>vd_sync</code></td><td><code>[steamid]</code> — reload admins and punishments for everyone or one player</td><td><code>@admin/reload_admin</code></td></tr>
<tr><td><code>vd_import</code></td><td><code>as_</code> — import data from cs2-admin_system</td><td>console</td></tr>
</table>

## Admin flags

Defaults from `core.ini`, fully editable.

| Flag | Name | Flag | Name |
|---|---|---|---|
| `a` | Reserved slot | `j` | Chat and voice (mute, gag, silence) |
| `b` | Basic access | `k` | Votes |
| `c` | Kick | `m` | RCON |
| `d` | Ban | `n` | Cheat commands (noclip, god, hp, armor, speed, gravity) |
| `e` | Unban | `z` | ROOT |
| `f` | Player management | | |
| `g` | Maps, restart, swap teams | | |
| `i` | Configs | | |

## Web panel

[**Verdict Web**](https://github.com/whyn0tq/cs2-verdict-web) is a separate PHP + MySQL panel. Download it from the [repository](https://github.com/whyn0tq/cs2-verdict-web) (*Code* → *Download ZIP* or *Releases*) and upload it to any regular web hosting. Set `server_id` in `core.ini` and the plugin publishes online players, chat and events to the shared database and runs panel actions (kick, move, slay, chat messages). Bans, mutes and admins issued in the panel apply without a restart.

## Troubleshooting

| Problem | Fix |
|---|---|
| Verdict missing from `meta list` | Re-add `Game csgo/addons/metamod` to `gameinfo.gi` |
| Conflicts with admin_system | Remove admin_system — two `IAdminApi` plugins cannot run together |
| Cannot connect to the database | Check `databases.cfg`; allow remote access for the game server IP |
| Menu does not open | Install cs2-menus and make sure it loads |
| `verdict.so` fails to load | Use the release archive or build in the Steam Runtime sniper container |

## Build

Requires Linux x86_64, clang, Python 3 and [AMBuild](https://github.com/alliedmodders/ambuild).

```bash
git clone https://github.com/alliedmodders/ambuild.git
python3 -m pip install ./ambuild
git clone --recursive --branch master https://github.com/alliedmodders/metamod-source.git
git clone --recursive --branch cs2 https://github.com/alliedmodders/hl2sdk.git hl2sdk-cs2

mkdir build && cd build
CC=clang CXX=clang++ python3 ../configure.py -s cs2 --targets x86_64 --enable-optimize \
  --hl2sdk-manifests=<metamod-source>/hl2sdk-manifests \
  --mms_path=<metamod-source> \
  --hl2sdk-root=<folder containing hl2sdk-cs2>
ambuild
```

The `addons` folder appears in `build/package/`. Release builds run in GitHub Actions ([`plugin.yml`](.github/workflows/plugin.yml)) inside the Steam Runtime sniper container, so `verdict.so` does not require a newer glibc than your server has.

```
src/            plugin code
include/        admin.h (IAdminApi), verdict.h (IVerdictApi)
configs/        core.ini, sorting.ini, maps.ini, databases.cfg.example
translations/   verdict.phrases.txt
third_party/    cs2-menus and sql_mm headers, unmodified
```

---

<div align="center">

Author: **teesa** ([whyn0tq](https://github.com/whyn0tq))

</div>
