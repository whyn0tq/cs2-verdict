# Verdict

Verdict is a CS2 administration plugin for Linux x86-64. It provides an in-game admin menu, bans, mutes, gags, and votes. It supports existing Material Admin databases and the [Verdict web panel](https://github.com/whyn0tq/cs2-verdict-web).

## Requirements

- [Metamod:Source 2.0 (build 1483+)](https://www.metamodsource.net/downloads.php?branch=dev)
- [cs2-menus (`utils.zip`)](https://github.com/whyn0tq/cs2-menus/releases)
- [cs2-DBMP 1.3.0+](https://github.com/whyn0tq/cs2-DBMP)
- MySQL 8 or MariaDB 10.6+

## Setup

1. Extract the [Verdict release](https://github.com/whyn0tq/cs2-verdict-v2/releases) into `game/csgo/`. Disable `cs2-admin_system` if it is installed.
2. Create a database or reuse an existing Material Admin database. Copy the `verdict` entry from [`databases.cfg.example`](configs/databases.cfg.example) into `addons/configs/databases.cfg` and set your credentials. Verdict creates its tables at startup.
3. Restart the server and check `meta list`. Add your first admin in the server console, replacing the example SteamID64 and name.

   ```text
   vd_add_admin 76561198000000000 "Name" z 100 0
   ```

4. Open `!admin` in chat. Plugin settings are in `addons/configs/verdict/`.

Modules that use the legacy `IAdminApi::GetMySQLConnection()` must open their own connection through DBMP. The legacy method returns `nullptr`.
