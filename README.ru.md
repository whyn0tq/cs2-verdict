<div align="center">

<img src="assets/banner.ru.svg" alt="Verdict" width="100%">

![CS2](https://img.shields.io/badge/CS2-Linux%20x64-e5383b?style=for-the-badge&labelColor=16161a) ![Metamod](https://img.shields.io/badge/Metamod:Source-2.0-e5383b?style=for-the-badge&labelColor=16161a) ![C++](https://img.shields.io/badge/plugin-C%2B%2B-e5383b?style=for-the-badge&labelColor=16161a) ![DB](https://img.shields.io/badge/MySQL%20%2F%20MariaDB-ready-e5383b?style=for-the-badge&labelColor=16161a)

[English](README.md) · **Русский**

[Возможности](#возможности) · [Установка](#установка) · [Команды](#команды) · [Флаги](#флаги-админов) · [Веб-панель](#веб-панель) · [Сборка](#сборка)

</div>

Заменяет **cs2-admin_system** и серверную часть **Material Admin**. Веб-панель [**Verdict Web**](https://github.com/whyn0tq/cs2-verdict-web) — отдельный репозиторий: панель и плагин работают с одной базой и общаются только через неё.

- **Своя база не нужна.** Работает на схеме Material Admin (`sb_bans`, `sb_comms`, `sb_admins`, `sb_srvgroups`, `sb_servers`). Старую базу MA можно подключить как есть.
- **Модули работают.** Отдаёт тот же `IAdminApi`, что и cs2-admin_system, поэтому модули admin_system, VIP, LR и другие не нужно пересобирать.
- **Лёгкий переход.** Конфиги в формате admin_system; данные переносятся командой `vd_import`.

## Возможности

- Баны по SteamID и IP, в том числе оффлайн
- Муты, гаги и сайленс
- Админы с флагами, иммунитетом, группами, сроком действия и привязкой к серверам
- Ограничения группы на срок бана и мута
- Меню `!admin` для управления игроками и сервером
- Голосования: кик, бан, мут, карта
- Управление игроками: slay, slap, поджог, бомбы, заморозка, команда, noclip, god, HP, броня, гравитация, скорость
- Смена карты (и из мастерской), рестарт, смена сторон, выполнение конфигов, RCON
- Несколько серверов на одной базе: общие или раздельные админы и баны
- Языки: русский, английский, украинский

## Что понадобится

| Что | Где взять |
|---|---|
| Сервер CS2 на **Linux** x86_64 | — |
| Metamod:Source **2.0** (dev-сборка) | [metamodsource.net](https://www.metamodsource.net/downloads.php?branch=dev) |
| cs2-menus (`utils.zip`) | [Pisex/cs2-menus](https://github.com/Pisex/cs2-menus/releases) |
| sql_mm (`sql_mm-linux-*.tar.gz`) | [zer0k-z/sql_mm](https://github.com/zer0k-z/sql_mm/releases) |
| MySQL 8 или MariaDB 10.6+ | панель хостинга |

Все пути — от папки сервера, в которой лежит `game/csgo/`.

## Установка

**1. Metamod.** Распакуйте Metamod 2.0 в `game/csgo/` и добавьте строку в `game/csgo/gameinfo.gi` внутри `SearchPaths` **перед** `Game csgo`:

```
Game    csgo/addons/metamod
```

Обновления CS2 перезаписывают `gameinfo.gi` — после каждого обновления строку нужно вернуть. Проверка: `meta version` в консоли сервера.

**2. Зависимости.** Распакуйте `utils.zip` (cs2-menus) и архив sql_mm в `game/csgo/`.

**3. Verdict.** Если стоит cs2-admin_system, удалите его (`addons/admin_system/` и `addons/metamod/admin_system.vdf`; файл `admin_system.phrases.txt` оставьте). Скачайте `verdict-linux.zip` из раздела *Releases* и скопируйте папку `addons` в `game/csgo/`.

**4. База данных.** Создайте базу и впишите её данные в `addons/configs/databases.cfg` (если файла нет — переименуйте `databases.cfg.example`). Таблицы создаются сами.

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

Если секции `verdict` нет, плагин возьмёт `admin_system` или `materialadmin`.

**5. Проверка.** Перезапустите сервер. В консоли появится `[Verdict] Database ready`, а `meta list` покажет Verdict.

**6. Первый админ.** В консоли сервера или через RCON:

```
vd_add_admin <steamid64> "Ник" z 100 0
```

`z` — полный доступ, `100` — иммунитет, `0` — бессрочно. Затем напишите `!admin` в чат.

## Команды

`!admin` открывает меню. В консоли сервера и через RCON команды начинаются с `vd_` (`vd_ban`), в чате — с `!` (`!ban`). **Доступ** — право, которое нужно игроку.

`<цель>` — ник, `#слот` или SteamID. `<срок>` — число с необязательной единицей: `s`, `m` (по умолчанию), `h`, `d`, `w`, например `30`, `2h`, `7d`; `0` — навсегда. Команды обслуживания вводятся с префиксом `vd_`.

### Наказания

<table width="100%">
<tr><th width="20%" align="left">Команда</th><th width="55%" align="left">Использование</th><th width="25%" align="left">Доступ</th></tr>
<tr><td><code>ban</code></td><td><code>&lt;цель&gt; &lt;срок&gt; [причина]</code></td><td><code>@admin/ban</code></td></tr>
<tr><td><code>banip</code></td><td><code>&lt;ip&gt; &lt;срок&gt; [причина]</code></td><td><code>@admin/ban</code></td></tr>
<tr><td><code>mute</code></td><td><code>&lt;цель&gt; &lt;срок&gt; [причина]</code></td><td><code>@admin/mute</code></td></tr>
<tr><td><code>gag</code></td><td><code>&lt;цель&gt; &lt;срок&gt; [причина]</code></td><td><code>@admin/gag</code></td></tr>
<tr><td><code>silence</code></td><td><code>&lt;цель&gt; &lt;срок&gt; [причина]</code></td><td><code>@admin/silence</code></td></tr>
<tr><td><code>unban</code></td><td><code>&lt;цель|ip&gt; [причина]</code></td><td><code>@admin/unban</code></td></tr>
<tr><td><code>unmute</code></td><td><code>&lt;цель&gt; [причина]</code></td><td><code>@admin/unmute</code></td></tr>
<tr><td><code>ungag</code></td><td><code>&lt;цель&gt; [причина]</code></td><td><code>@admin/ungag</code></td></tr>
<tr><td><code>unsilence</code></td><td><code>&lt;цель&gt; [причина]</code></td><td><code>@admin/unsilence</code></td></tr>
<tr><td><code>kick</code></td><td><code>&lt;цель&gt; [причина]</code></td><td><code>@admin/kick</code></td></tr>
</table>

### Игроки

<table width="100%">
<tr><th width="20%" align="left">Команда</th><th width="55%" align="left">Использование</th><th width="25%" align="left">Доступ</th></tr>
<tr><td><code>slay</code></td><td><code>&lt;цель&gt;</code></td><td><code>@admin/kill</code></td></tr>
<tr><td><code>slap</code></td><td><code>&lt;цель&gt; [урон]</code></td><td><code>@admin/slap</code></td></tr>
<tr><td><code>burn</code></td><td><code>&lt;цель&gt; [секунды]</code></td><td><code>@admin/burn</code></td></tr>
<tr><td><code>freeze</code></td><td><code>&lt;цель&gt; [секунды]</code> — 0 — разморозить</td><td><code>@admin/freeze</code></td></tr>
<tr><td><code>timebomb</code></td><td><code>&lt;цель&gt;</code> — вкл/выкл</td><td><code>@admin/timebomb</code></td></tr>
<tr><td><code>firebomb</code></td><td><code>&lt;цель&gt;</code> — вкл/выкл</td><td><code>@admin/firebomb</code></td></tr>
<tr><td><code>freezebomb</code></td><td><code>&lt;цель&gt;</code> — вкл/выкл</td><td><code>@admin/freezebomb</code></td></tr>
<tr><td><code>respawn</code></td><td><code>&lt;цель&gt;</code></td><td><code>@admin/respawn</code></td></tr>
<tr><td><code>rename</code></td><td><code>&lt;цель&gt; &lt;новый ник&gt;</code></td><td><code>@admin/rename</code></td></tr>
<tr><td><code>team</code></td><td><code>&lt;цель&gt; &lt;t|ct|spec&gt;</code></td><td><code>@admin/changeteam</code></td></tr>
<tr><td><code>noclip</code></td><td><code>&lt;цель&gt;</code> — вкл/выкл</td><td><code>@admin/noclip</code></td></tr>
<tr><td><code>god</code></td><td><code>&lt;цель&gt;</code> — вкл/выкл</td><td><code>@admin/god</code></td></tr>
<tr><td><code>hp</code></td><td><code>&lt;цель&gt; &lt;значение&gt;</code></td><td><code>@admin/hp</code></td></tr>
<tr><td><code>armor</code></td><td><code>&lt;цель&gt; &lt;значение&gt;</code></td><td><code>@admin/armor</code></td></tr>
<tr><td><code>speed</code></td><td><code>&lt;цель&gt; &lt;значение&gt;</code></td><td><code>@admin/speed</code></td></tr>
<tr><td><code>gravity</code></td><td><code>&lt;цель&gt; &lt;значение&gt;</code></td><td><code>@admin/gravity</code></td></tr>
<tr><td><code>who</code></td><td><code>[цель]</code> — информация об игроке, о себе если не указано</td><td><code>@admin/who</code></td></tr>
</table>

### Сервер

<table width="100%">
<tr><th width="20%" align="left">Команда</th><th width="55%" align="left">Использование</th><th width="25%" align="left">Доступ</th></tr>
<tr><td><code>map</code></td><td><code>&lt;карта|workshop id&gt;</code></td><td><code>@admin/maps</code></td></tr>
<tr><td><code>restart</code></td><td>—</td><td><code>@admin/restart</code></td></tr>
<tr><td><code>swapteams</code></td><td>—</td><td><code>@admin/swapteams</code></td></tr>
<tr><td><code>rcon</code></td><td><code>&lt;команда сервера&gt;</code></td><td><code>@admin/rcon</code></td></tr>
</table>

### Голосования

<table width="100%">
<tr><th width="20%" align="left">Команда</th><th width="55%" align="left">Использование</th><th width="25%" align="left">Доступ</th></tr>
<tr><td><code>votekick</code></td><td><code>&lt;цель&gt;</code></td><td><code>@admin/vote</code></td></tr>
<tr><td><code>voteban</code></td><td><code>&lt;цель&gt;</code></td><td><code>@admin/vote</code></td></tr>
<tr><td><code>votemute</code></td><td><code>&lt;цель&gt;</code></td><td><code>@admin/vote</code></td></tr>
<tr><td><code>votemap</code></td><td><code>&lt;карта|workshop id&gt;</code></td><td><code>@admin/vote</code></td></tr>
</table>

### Админы и группы

<table width="100%">
<tr><th width="20%" align="left">Команда</th><th width="55%" align="left">Использование</th><th width="25%" align="left">Доступ</th></tr>
<tr><td><code>add_admin</code></td><td><code>&lt;цель&gt; &lt;имя&gt; &lt;флаги&gt; &lt;иммунитет&gt; &lt;срок&gt; [group id] [comment]</code></td><td><code>@admin/add_admin</code></td></tr>
<tr><td><code>remove_admin</code></td><td><code>&lt;цель&gt;</code></td><td><code>@admin/remove_admin</code></td></tr>
<tr><td><code>add_group</code></td><td><code>&lt;имя&gt; &lt;флаги&gt; &lt;иммунитет&gt;</code></td><td><code>@admin/add_group</code></td></tr>
<tr><td><code>remove_group</code></td><td><code>&lt;id|имя&gt;</code></td><td><code>@admin/remove_group</code></td></tr>
</table>

### Обслуживание

<table width="100%">
<tr><th width="20%" align="left">Команда</th><th width="55%" align="left">Использование</th><th width="25%" align="left">Доступ</th></tr>
<tr><td><code>vd_reload</code></td><td>перечитать конфиг и фразы</td><td><code>@admin/reload_config</code></td></tr>
<tr><td><code>vd_reload_admin</code></td><td><code>&lt;цель&gt;</code> — перечитать права админа игрока на сервере</td><td><code>@admin/reload_admin</code></td></tr>
<tr><td><code>vd_reload_punish</code></td><td><code>&lt;цель&gt;</code> — перечитать наказания игрока на сервере</td><td><code>@admin/reload_punish</code></td></tr>
<tr><td><code>vd_sync</code></td><td><code>[steamid]</code> — перечитать админов и наказания у всех или у одного игрока</td><td><code>@admin/reload_admin</code></td></tr>
<tr><td><code>vd_import</code></td><td><code>as_</code> — импорт данных из cs2-admin_system</td><td>консоль</td></tr>
</table>

## Флаги админов

Значения по умолчанию из `core.ini`, меняются свободно.

| Флаг | Название | Флаг | Название |
|---|---|---|---|
| `a` | Резервный слот | `j` | Чат и микрофон (mute, gag, silence) |
| `b` | Базовый доступ | `k` | Голосования |
| `c` | Кик | `m` | RCON |
| `d` | Бан | `n` | Читерские команды (noclip, god, hp, armor, speed, gravity) |
| `e` | Разбан | `z` | ROOT |
| `f` | Управление игроками | | |
| `g` | Карты, рестарт, смена сторон | | |
| `i` | Конфиги | | |

## Веб-панель

[**Verdict Web**](https://github.com/whyn0tq/cs2-verdict-web) — отдельная панель на PHP и MySQL. Скачать её можно в [репозитории](https://github.com/whyn0tq/cs2-verdict-web) (*Code* → *Download ZIP* или *Releases*), ставится на обычный веб-хостинг. Укажите `server_id` в `core.ini`, и плагин будет писать в общую базу онлайн, чат и события, а также выполнять действия из панели (кик, перевод в команду, убийство, сообщения в чат). Баны, муты и админы из панели применяются без перезапуска сервера.

## Частые проблемы

| Проблема | Решение |
|---|---|
| Verdict нет в `meta list` | Верните `Game csgo/addons/metamod` в `gameinfo.gi` |
| Конфликт с admin_system | Удалите admin_system — два плагина с `IAdminApi` вместе не работают |
| Нет подключения к базе | Проверьте `databases.cfg`; разрешите удалённый доступ для IP игрового сервера |
| Не открывается меню | Установите cs2-menus и проверьте, что он загружен |
| `verdict.so` не загружается | Используйте релизный архив или соберите в контейнере Steam Runtime sniper |

## Сборка

Нужны Linux x86_64, clang, Python 3 и [AMBuild](https://github.com/alliedmodders/ambuild).

```bash
git clone https://github.com/alliedmodders/ambuild.git
python3 -m pip install ./ambuild
git clone --recursive --branch master https://github.com/alliedmodders/metamod-source.git
git clone --recursive --branch cs2 https://github.com/alliedmodders/hl2sdk.git hl2sdk-cs2

mkdir build && cd build
CC=clang CXX=clang++ python3 ../configure.py -s cs2 --targets x86_64 --enable-optimize \
  --hl2sdk-manifests=<metamod-source>/hl2sdk-manifests \
  --mms_path=<metamod-source> \
  --hl2sdk-root=<папка, в которой лежит hl2sdk-cs2>
ambuild
```

Папка `addons` появится в `build/package/`. Релизная сборка идёт в GitHub Actions ([`plugin.yml`](.github/workflows/plugin.yml)) в контейнере Steam Runtime sniper — так `verdict.so` не требует более новой glibc, чем есть на сервере.

```
src/            код плагина
include/        admin.h (IAdminApi), verdict.h (IVerdictApi)
configs/        core.ini, sorting.ini, maps.ini, databases.cfg.example
translations/   verdict.phrases.txt
third_party/    заголовки cs2-menus и sql_mm без изменений
```

---

<div align="center">

Автор: **teesa** ([whyn0tq](https://github.com/whyn0tq))

</div>
