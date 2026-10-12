# SeriousBridge protocol, version 1

Taken from the working Serious Sam ↔ Minecraft bridge ([SemCraft 2 `docs/CONTRACT.md`](https://github.com/animeliodas/SemCraft2/blob/main/docs/CONTRACT.md)),
split into the **core** every bridge with Sam as the host speaks, and **guest extensions** a guest may add.
Positions are Sam coordinates (metres; right-handed, y up, heading 0 looks down −z, positive heading turns left);
a guest maps them to its own world in one place.

## Transport
- WebSocket text frames, one JSON object each, on `127.0.0.1` (SemCraft 2: port 25610). The guest is the server; the
  Sam plugin connects and reconnects every second. Other local clients (tests, directors) may connect too: the guest
  handles their messages the same way and sends its events to every client.
- `"t"` is the message type. Unknown types and fields are ignored by both sides.

## Core: Sam → guest
| Message | When | Meaning |
|---|---|---|
| `{"t":"hello","v":1,"game":"TFE"\|"TSE","pid":N}` | each connection | host identity |
| `{"t":"cam","f":frame,"p":[x,y,z],"r":[heading,pitch,bank],"vfov":deg,"aspect":w/h,"w":px,"h":px,"near":m,"pl":[x,y,z]}` | every rendered frame | the camera Sam actually rendered, and the player's feet |
| `{"t":"level","name":"Levels\\X.wld","file":"...level_<hash>.tri","hash":"<hex>","tris":N}` | a world loaded (and on reconnect) | level geometry to build the guest's invisible shell from |
| `{"t":"state","hp":f,"alive":b,"inGame":b,"build":b}` | 5 Hz | the player's health and modes |
| `{"t":"reset","what":"all"\|"world"}` | a level entered | a new game (`all`) or the next level / a loaded save (`world`): the guest clears what the player built and spawned (and with `all` the player's things) |
| `{"t":"actors","l":[[id,x,feetY,z,height,width,health],...]}` | 10 Hz | Sam monsters near the player (the guest keeps stand-ins) |
| `{"t":"movers","l":[[i,x,y,z,m11..m33],...]}` | a moving brush moved (≤10 Hz, all every 5 s) | absolute = (x,y,z) + M · relative (`.mov`) |
| `{"t":"shot","o":[x,y,z],"d":[x,y,z],"dmg":f,"n":rays,"spread":deg,"range":m}` | Sam fired a hitscan weapon | the guest hurts what the rays hit first |
| `{"t":"proj","l":[[id,x,y,z,radius,dmg,break,flame],...]}` | every tick | Sam projectiles in flight |
| `{"t":"boom","p":[x,y,z],"r":m,"dmg":f,"break":b,"src":"rocket"\|...}` | a tracked projectile exploded | splash damage, broken blocks, fire |
| `{"t":"build","on":b}`, `{"t":"key","k":"attack"\|"use"\|"drop","down":b}`, `{"t":"slot","n":0..8}` | input | "build mode": the mouse and number keys act in the guest |
| `{"t":"screen","open":"inventory"\|"chat"\|""[,"text":"..."]}` | a guest screen asked for / closed | |
| `{"t":"cursor","x":0..1,"y":0..1}`, `{"t":"click","b":0\|1\|2,"down":b,"shift":b}`, `{"t":"char","c":codepoint}`, `{"t":"keydown","sc":usbScancode,"kc":keycode,"mods":m}`, `{"t":"scrollgui","d":±1}` | a guest screen is open | Sam's mouse and keyboard go to it |
| `{"t":"chat","m":"..."}` | Sam's chat line | the guest's chat; "/..." is a guest command |

## Core: guest → Sam
| Message | Meaning |
|---|---|
| `{"t":"hello","v":1,"shm":"Local\\<name>","pid":N}` | where the frames are |
| `{"t":"hurt","d":samHp,"src":"kind","from":[x,y,z]}` | the guest's world hurt the player (Sam hp units) |
| `{"t":"push","v":[x,y,z]}` | knockback as Sam m/s |
| `{"t":"shove","d":[x,y,z],"by":"piston"}` | the guest moved the player this far (metres) |
| `{"t":"actorhit","id":N,"d":samHp,"boom":b,"from":"kind"}` | the guest hit a Sam monster's stand-in |
| `{"t":"actorpush","id":N,"d":[x,y,z]}` | the guest moved a Sam monster's stand-in |
| `{"t":"blocks","set":[x,y,z,...],"clear":[...]}` | solid guest cells appeared / went (minimum corners): Sam makes them solid |
| `{"t":"fluid","water":h,"lava":h,"eyes":b,"flow":[x,y,z]}` | guest fluid at the player's feet: Sam's player swims by the guest's rules |
| `{"t":"trigger","mover":i}` | the guest's logic (redstone) triggers Sam's moving brush i |
| `{"t":"projhit","id":N}` | a Sam projectile hit something in the guest and is gone |
| `{"t":"screen","open":false}` / `{"t":"screen","open":"Name"}` | the guest closed its screen / opened one itself |
| `{"t":"level_ready","hash":"<hex>","blocks":N}`, `{"t":"log","m":"..."}` | progress, diagnostics |

## Guest extensions (SemCraft 2 / Minecraft)
- `{"t":"gear","elytra":b,"hand":b,"fireworks":n}` → Sam's player can glide by Minecraft's elytra formulas;
  `{"t":"firework"}` ← Sam lit one (the guest uses it up and shows it).
- `{"t":"spawn",...}`, `{"t":"pillar",...}`, `{"t":"cmd","c":"..."}`, `{"t":"status"}`, `{"t":"moversdebug",...}`:
  dev and test hooks.

A new guest adds its own types the same way: owner-side state goes out as facts, the other side answers with
requests (hit, move, trigger), never by editing its copy.

## Frames (shared memory, little-endian)
```
header (4096 bytes)
  0 int magic   4 int version 1   8 int header bytes   12 int slot count
  16 long slot stride   24 int max width   28 int max height
  32 long publish counter   40 int latest slot (-1 none)   44 int guest pid
slot descriptor i at 256 + 128*i
  +0 long seq (odd while written)  +8 long guest frame  +16 long host frame ("f" of the cam used)
  +24 int width  +28 int height  +32 float near  +36 float far  +40 float vfov (deg)
  +44 int flags: 1 depth in [0,1], 2 rows bottom-up, 4 reversed Z
  +48 float cam x,y,z  +60 float heading, pitch, bank (as received)  +72 long capture ns  +80 long publish ns
  +88 float aspect
slot data at 4096 + i*stride: world RGBA8 premultiplied, world depth float32, overlay RGBA8 premultiplied (HUD, screens)
```
Sam is 32-bit: keep the mapping under ~100 MB (SemCraft 2: 3 slots, 1920×1200 max).

## Geometry files
`.tri`: `"SCT1"`, int version 1, int N, int flags, float bbox[6], then N × {float v0[3], v1[3], v2[3], normal[3];
int flags} (1 = from a moving brush, 2 = passable). `.mov`: `"SCM1"`, int version 1, int M, int reserved, then M ×
{int T; T × float v0[3], v1[3], v2[3]} in the brush's own coordinates.
