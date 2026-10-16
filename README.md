# SeriousBridge — основа для переноса механик других игр в Serious Sam Classic

SeriousBridge — ядро «мостов» между Serious Sam Classic (The First Encounter 1.05, The Second Encounter 1.07,
[Classics Patch](https://github.com/SamClassicPatch/SuperProject) 1.9.4) и другими играми. Каждая игра — отдельный порт
в своём репозитории; всё, что нужно больше чем одному порту, переезжает сюда, и с каждым новым портом ядро растёт.

| Репозиторий | Что |
|---|---|
| **SeriousBridge** (этот) | ядро для плагинов Sam (`core/sam`), протокол, физика, транспорт, проверочные стенды |
| [SemCraft 2](https://github.com/animeliodas/SemCraft2) | Minecraft 26.3 внутри Sam: гость работает рядом, его мир вклеивается в кадр Sam с глубиной; блоки, мобы, оружие, элитры, вода, поршни, лодки, интерфейс Minecraft, Стив от третьего лица |
| [SeriousPortal](https://github.com/animeliodas/SeriousPortal) | портальная пушка Portal: правила Portal переписаны в плагине, звуки — из вашей Portal |
| [SeriousHL](https://github.com/animeliodas/SeriousHL) | Half-Life 1: движение (банихоп, присед, бусты), оружие HL на стволах Сэма; сборка для спидрана с таймером и LiveSplit; установка в один клик |
| Doom (в планах) | классический Doom — [ROADMAP](docs/ROADMAP.md) |

> Сделано с помощью Claude Code (ИИ). Всё «работает» — с оракулами в реальной игре (скрипты печатают PASS/FAIL); что
> проверено и где — в документах и в журналах портов.

## Ядро для плагина Sam (`core/sam`)

Исходники компилируются в DLL каждого порта (своих DLL у ядра нет). Порт:

1. кладёт SeriousBridge рядом (`<папка>/seriousbridge`) или задаёт `$SERIOUSBRIDGE`;
2. в своём `.vcxproj` импортирует `$(SeriousBridgeDir)core\sam\SeriousBridge.props` (перед `Microsoft.Cpp.targets`);
3. в своём `StdH.h` (предкомпилированный заголовок) задаёт `SB_NAME` — имя для логов и папки `%TEMP%\<имя>`.

| Модуль | Что делает | Всегда / гость рядом |
|---|---|---|
| `Alloc` | `new/delete` плагина — на куче движка (иначе две CRT ломают кучу) | всегда |
| `Player` | локальный игрок, свойства Player.es (здоровье, броня, вид, бонусы TSE), поворот и заморозка вида | всегда |
| `Physics` | силы по ID (импульс, сдвиг, телепорт), режим движения против гравитации Sam, езда на транспорте гостя | всегда |
| `Movement` | движение игрока по правилам другой игры целиком (патч `CMovableEntity::PreMoving`: движение Сэма не мешает) | всегда |
| `Speedrun` | таймер забега (RTA, IGT по тикам игры), сплиты на концах уровней, LiveSplit через его TCP-сервер | всегда |
| `DevHook` | консольные команды из файла — тесты без мыши и клавиатуры игрока; сторож: зависшая игра пишет, где стоит | всегда |
| `Link`, `Json` | WebSocket-клиент к гостю на 127.0.0.1, разбор JSON | `SBridgeGuestBeside` |
| `LevelExport`, `Movers` | геометрия уровня (`.tri`) и подвижные браши (`.mov`) для гостя, их движение и триггеры | `SBridgeGuestBeside` |
| `Actors`, `Weapons` | монстры Sam для гостя (двойники, урон, отдача), выстрелы и снаряды Sam | `SBridgeGuestBeside` |
| `Blocks` | твёрдое у гостя → невидимые браши Sam (точная коллизия) | `SBridgeGuestBeside` |
| `Compositor`, `GlDraw` | кадр гостя из общей памяти → в кадр Sam с глубиной | `SBridgeGuestBeside` |

## Документы

| Файл | О чём |
|---|---|
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | части моста, владение, каналы, ядро и порты |
| [docs/PROTOCOL.md](docs/PROTOCOL.md) | сообщения связи (WebSocket JSON), кадр в общей памяти, файлы геометрии |
| [docs/PHYSICS.md](docs/PHYSICS.md) | физика игрока и монстров Sam под механиками гостя |
| [docs/TRANSPORT.md](docs/TRANSPORT.md) | транспорт гостя (лодки Minecraft) с игроком Sam |
| [docs/ROADMAP.md](docs/ROADMAP.md) | что дальше: Doom, ядро на стороне гостя, обратные мосты |
| [tools/echo_guest.py](tools/echo_guest.py) | простейший «гость»: принимает плагин Sam и показывает, что тот шлёт |

## Правила

- Только одиночная игра или свои серверы; никакого вмешательства в онлайн-игры с античитом.
- Никаких файлов игр в репозиториях: всё из игр берётся с установки игрока.
- Каждое «работает» — с оракулом в реальной игре.

## Лицензия

GPL-2.0, как Serious Engine 1 и Classics Patch, на которых стоят плагины Sam.
