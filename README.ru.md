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

<p align="center"><a href="README.md">English</a> · <b>Русский</b></p>
</div>

Плагин Metamod:Source для CS2: баны, муты и админы. Заменяет **cs2-admin_system** и серверную часть **Material Admin**.

- **Схема Material Admin.** Таблицы `sb_bans`, `sb_comms`, `sb_admins`, `sb_srvgroups`, `sb_servers`. Старую базу MA из CS:GO можно подключить как есть.
- **Совместимость с admin_system.** Отдаёт тот же `IAdminApi`: модули VIP, LR и прочие работают без пересборки. Конфиги в формате admin_system, `core.ini` переносится почти без правок.
- **[Веб-панель](https://github.com/whyn0tq/cs2-verdict-web)** — PHP 5.2+, работает на обычном хостинге с той же базой.

## Возможности

| | |
|---|---|
| Наказания | бан по SteamID и IP (в том числе оффлайн), мут, гаг, кик |
| Админы | флаги, иммунитет, группы, срок действия, привязка к серверам, лимиты группы на срок бана и мута |
| Управление | slay, slap, burn, timebomb, firebomb, freezebomb, freeze, respawn, team, rename, noclip, god, hp, armor, speed, gravity |
| Сервер | смена карты (в том числе из мастерской), рестарт, смена сторон, exec конфигов, RCON |
| Голосования | кик, бан, мут, смена карты |
| Прочее | меню `!admin`, несколько серверов на одной базе, миграция из admin_system (`vd_import`), языки ru / en / uk |

## Требования

| Что | Где взять |
|---|---|
| Metamod:Source 2.0 (build 1483+) | [metamodsource.net](https://www.metamodsource.net/downloads.php?branch=dev) |
| cs2-menus (`utils.zip`) | [Pisex/cs2-menus](https://github.com/Pisex/cs2-menus/releases) |
| sql_mm (`sql_mm-linux-*.tar.gz`) | [zer0k-z/sql_mm](https://github.com/zer0k-z/sql_mm/releases) |
| MySQL 8 / MariaDB 10.6+ | панель хостинга |

## Установка

1. Удалите cs2-admin_system, если он стоит: папку `addons/admin_system/` и `addons/metamod/admin_system.vdf` (два `IAdminApi` вместе не работают; `addons/translations/admin_system.phrases.txt` оставьте — Verdict читает его для модулей).
2. Создайте пустую базу — таблицы Verdict создаст сам. В `addons/configs/databases.cfg` (если нет — скопируйте из `databases.cfg.example`) добавьте секцию:

   ```
   "Databases"
   {
       "verdict"
       {
           "host"      "127.0.0.1"
           "user"      "verdict"
           "pass"      "пароль"
           "database"  "verdict"
           "port"      "3306"
       }
   }
   ```

   Чужие секции (`vip`, `lr`…) не трогайте. Без секции `verdict` плагин ищет `admin_system`, затем `materialadmin`.
3. Перезапустите сервер. В консоли должно появиться `[Verdict] Database ready, server id 0`, `meta list` — показать Verdict.
4. Выдайте админа в консоли сервера:

   ```
   vd_add_admin 76561198000000000 "Ник" z 100 0
   ```

   `z` — полный доступ, `100` — иммунитет, `0` — бессрочно.

## Команды

В консоли и RCON — с префиксом `vd_`, в чате — с `!`. Доступ задаётся флагами админа.

| Команда | Аргументы |
|---|---|
| `ban` `mute` `gag` `silence` | `<ник\|#слот\|steamid> <срок> [причина]` |
| `unban` `unmute` `ungag` `unsilence` | `<ник\|#слот\|steamid\|ip> [причина]` |
| `banip` | `<ip> <срок> [причина]` |
| `kick` | `<ник\|#слот\|steamid> [причина]` |
| `add_admin` | `<цель> <имя> <флаги> <иммунитет> <срок> [id группы] [комментарий]` |
| `remove_admin` | `<цель>` |
| `add_group` / `remove_group` | `<название> <флаги> <иммунитет>` / `<id\|название>` |
| `slay` `noclip` `respawn` `god` `timebomb` `firebomb` `freezebomb` | `<цель>` |
| `slap` | `<цель> [урон]` |
| `burn` | `<цель> [секунды]` |
| `freeze` | `<цель> [секунды]`, `0` — разморозить |
| `hp` `armor` `speed` `gravity` | `<цель> <значение>` |
| `team` | `<цель> <t\|ct\|spec>` |
| `rename` | `<цель> <новый ник>` |
| `map` / `votemap` | `<карта\|id из мастерской>` |
| `votekick` `voteban` `votemute` | `<цель>` |
| `restart` `swapteams` `who` | — |
| `rcon` | `<команда>` |
| `reload` `reload_admin` `reload_punish` `sync` | перечитать конфиг / админов / наказания, синхронизировать с базой |
| `import` | `[префикс таблиц]`, по умолчанию `as_`. Только из консоли: переносит админов, группы и наказания из таблиц cs2-admin_system в той же базе |

**Срок** — в минутах, `0` — навсегда.

## Конфиги

`addons/configs/verdict/`:

- `core.ini` — префикс таблиц (`database_prefix`, по умолчанию `sb`), `server_id`, префикс чата, режим наказаний (`punish_scope`, `punish_ip`), параметры голосований, эффекты, список конфигов для `exec`, расшифровка флагов, причины и сроки банов и мутов.
- `sorting.ini` — порядок пунктов меню.
- `maps.ini` — список карт для смены и голосований.

Флаги админа (значения по умолчанию из `core.ini`):

| Флаг | Доступ |
|---|---|
| `a` | резервный слот |
| `b` | `status`, `who`, список админов, админ-чат |
| `c` | `kick` |
| `d` | `ban`, свой текст причины |
| `e` | `unban` |
| `f` | `slay`, `slap`, `burn`, `timebomb`, `firebomb`, `freezebomb`, `freeze`, `rename`, `team`, `respawn` |
| `g` | `map`, `restart`, `swapteams` |
| `i` | `reload`, exec конфигов |
| `j` | `mute`, `gag`, `silence` и снятие |
| `k` | голосования: `votekick`, `voteban`, `votemute`, `votemap` |
| `m` | `rcon` |
| `n` | `noclip`, `god`, `hp`, `armor`, `speed`, `gravity` |
| `z` | ROOT — полный доступ |

Несколько серверов на одной базе: у каждого свой `server_id`; баны и админы либо общие, либо раздельные — по `punish_scope`.

## Автор

**teesa (whyn0tq)** · [GitHub](https://github.com/whyn0tq)
