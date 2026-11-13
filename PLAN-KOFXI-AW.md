# Plan: KOF XI Atomiswave en NotDC AW

Estado: propuesto. Fecha: 2026-11-13.
Objetivo: ejecutar KOF XI Atomiswave en PS3 con el nucleo de NotDC, usando `fs_extracted` como fuente de datos y
sin tocar la instalacion ni los datos de NotDC.

## 0. Decisiones de base

- Se parte de NotDC-PS3: JIT SH4 en el PPE, PVR2 por RSX, AICA/ARM7, MMU y soporte big-endian ya resueltos.
- No se porta DreamRecomp a PS3.
- El juego arranca directamente en `0x8C010000`; no se emula la BIOS Atomiswave.
- La fuente principal de datos es `fs_extracted` (cartucho virtual). El set `.dec` empaquetado queda como alternativa.
- `fs_extracted` reduce RAM de host y facilita el modding. No reduce la CPU de emulacion: el SH-4 se sigue ejecutando.

## 1. Identidad aislada de la build de prueba (hecho)

| Dato | Valor |
|---|---|
| ELF / PKG | `build/notdc_aw_ps3.elf` / `build/notdc_aw_ps3.pkg`; full: `build/notdc_aw_ps3_full.pkg` |
| Nombre XMB | `NotDC AW` |
| Title ID | `NOTDCAW01` |
| Content ID | `UP0001-NOTDCAW01_00-0000000000000000` |
| Version XMB | `00.01` |
| Datos | `/dev_hdd0/data/NotDC-AW/` |
| Config | `/dev_hdd0/data/NotDC-AW/NotDC-AW.cfg` |
| BIOS / flash DC | `/dev_hdd0/data/NotDC-AW/data/dc_boot.bin`, `dc_flash.bin` |
| Logs | `/dev_hdd0/data/NotDC-AW/NotDC-AW.log`, `NotDC-AW_prev.log` |
| Cheats | `/dev_hdd0/data/NotDC-AW/cheats/` |
| Config por juego | `/dev_hdd0/data/NotDC-AW/config/` |
| Capturas | `/dev_hdd0/data/NotDC-AW/screenshots/` |
| VMU | `/dev_hdd0/data/NotDC-AW/vmu/` |
| Sonda JIT | `/dev_hdd0/data/NotDC-AW/execprobe.log` |
| Reload de Close Game | `/dev_hdd0/game/NOTDCAW01/USRDIR/RELOAD.SELF` |

La build original (`NOTDC0000`, `/dev_hdd0/data/NotDC/`) no se toca. Para probar tambien un disco Dreamcast hay que
copiar el BIOS y el flash a la carpeta nueva; no se leen de la instalacion original.

## 2. Arquitectura

```text
ax3201p01.fmem1 (8 MB) ──> RAM 0x0C00FF00, PC 0x8C010000
fs_extracted/ + _manifest.txt ──> AwRom ──> AwCart (0x5F7000)
                                             │
                                      G1 DMA (0x5F7400)
                                             │
                                             ▼
                                    RAM guest 0x0C000000
```

- **AwRom**: lee `fmem1`, el manifest y los archivos sueltos; cache LRU pequena; sin imagen de 232 MB en RAM.
- **AwCart**: registros EPR/MPR; sintetiza la tabla de records de 64 bytes; `MPR_FILE` se traduce al archivo suelto.
- **G1 DMA**: reutiliza el motor de GD-DMA de NotDC con origen `AwCart`; 27 ciclos/byte, bloques de 32 bytes, IRQ al final.
- **Flash AMD**: 128 KB en `0x00000000`, espejo en `0xA0000000`; comandos AMD y persistencia en `aw_flash.bin`.
- **Placa**: `0x600280` monedas/entradas; el resto del rango devuelve lo que espera el juego.
- **Mando AW**: dispositivo maple con funcion `0x01000000`; traduccion de botones DC -> AW.
- **Cheats**: escrituras de host por vblank, igual que el runtime de referencia.

## 3. Fases

### F0. Renombrado y aislamiento (hecho)

- `Makefile`: `TARGET`, `TITLE`, `TITLE_ID`, `CONTENT_ID`, `APP_VER`.
- Rutas de datos, config, logs, cheats, capturas, VMU y sonda.
- `PS3_SELF_PATH` con el Title ID nuevo.
- README, AGENTS y CI actualizados.

Criterio: `sh build.sh pkg` produce `build/notdc_aw_ps3.pkg`; instalar no modifica `NOTDC0000`.

Nota: en este checkout `build.sh` esta trackeado como `100644`, asi que `./build.sh` da `Permission denied`; usar
`sh build.sh ...` o darle permiso de ejecucion.

### F1. Esqueleto del modo Atomiswave

- Anadir extension `.aw` al navegador de la menu.
- Formato de descriptor `kofxi.aw`:

  ```ini
  fmem1=/dev_hdd0/data/NotDC-AW/aw/kofxi/ax3201p01.fmem1.dec
  fs=/dev_hdd0/data/NotDC-AW/aw/kofxi/fs_extracted
  records0=/dev_hdd0/data/NotDC-AW/aw/kofxi/records0.bin
  records1=/dev_hdd0/data/NotDC-AW/aw/kofxi/records1.bin
  records2=/dev_hdd0/data/NotDC-AW/aw/kofxi/records2.bin
  flash=/dev_hdd0/data/NotDC-AW/aw/kofxi/aw_flash.bin
  ```

  `records0/1/2` son opcionales. Son las copias de la tabla de records al inicio de mrom1, mrom4 y mrom7
  (68 KB cada una). Si faltan, se sintetizan desde `_manifest.txt` con los offsets correctos y nombres
  truncados a 16 bytes. Si existen, se conservan los nombres originales de 32 bytes.
  El descriptor debe estar dentro de `discs/` o de una subcarpeta visible en el navegador; por ejemplo
  `/dev_hdd0/data/NotDC-AW/discs/kofxi.aw` con rutas absolutas o `../aw/kofxi/...`.

  ```sh
  python3 - <<'PY'
  files = ['ax3201m01.mrom1.dec', 'ax3204m01.mrom4.dec', 'ax3207m01.mrom7.dec']
  for i, name in enumerate(files):
      d = open('decrypted/' + name, 'rb').read(0x10940)
      open(f'records{i}.bin', 'wb').write(d)
  PY
  ```

- Nuevo `ps3/ps3_aw.cpp/.h`: parsear el descriptor, reservar estado y marcar `g_aw_pending`.
- En `nullDC.cpp`: detectar `.aw`, bloquear el plugin de disco y llamar al loader.
- En `dc/dc.cpp`: cargar `fmem1` en RAM y fijar `PC=0x8C010000` despues del reset, como el camino `.elf`.
  Leer `gamePC` del header (`fmem1+0x64`) y `mpr_offset` (`fmem1+0x58`); validar que `gamePC` cae en RAM `0x8C...`.
- Saltar la comprobacion de BIOS solo en modo AW.
- Crear `PS3_DATA_ROOT "aw/"` en el arranque.

Criterio: el emulador arranca el payload y el log muestra `[aw] boot 0x8C010000`.

### F1b. Fallback: BIOS AW real (solo si F1 no basta)

- Cargar `awbios.zip` (`bios0.ic23`, 128 KB) en la flash de area 0 y dejar que la BIOS haga el boot.
- La BIOS necesita el cartucho, la flash AMD y la placa; F2/F3 ya los modelan.
- Es mas trabajo y no es el camino principal: el runtime de referencia demuestra que el payload arranca solo.
- No empezar por aqui; usarlo solo si el juego requiere inicializacion que la BIOS hace y el arranque directo no cubre.

### F2. Cartucho virtual desde `fs_extracted`

- `dc/aw/aw_rom.*`: leer `_manifest.txt`; tabla de records 0..N; mapa `offset -> archivo`; cache LRU.
- `dc/aw/aw_cart.*`: registros `0x5F7000..0x5F70FF`; modos EPR, MPR_RECORD y MPR_FILE; centinela de banco.
- Generar records 0 (header), 1 (`.`), 2 (`..`) y 3..N desde el manifest; respetar offsets y tamanos originales.
- `MPR_FILE`: resolver el offset absoluto y leer solo el rango pedido del archivo suelto.
- Fallback: si no hay `fs_extracted`, usar `decrypted/*.dec` empaquetado con el mismo `AwCart`.

Criterio: el juego lee la tabla de records y carga un archivo `.UNI` sin errores de rango.

**Estado 2026-11-13:** F1/F2 implementados y compilando:
`ps3/ps3_aw.cpp`, `dc/aw/aw_rom.cpp`, `aw_cart.cpp`, `aw_flash.cpp` y `aw_board.cpp`; menu `.aw`, salto a
`gamePC`, flash AMD, cartucho EPR/MPR, tablas de records 0/1/2 y DMA G1. Tambien hay mando AW maple
(`MDT_AwController`) y `misc/aw_prepare.py` para armar el kit. El navegador marca los `.aw` como `[AW]` y el
dispositivo del PKG como `ARCADE`. `pkg-full` arma el PKG de 236 MB con el set `.dec` empaquetado. Falta la
prueba en hardware; el coin/freeplay y los cheats van en F5.

**Bring-up HW 2026-11-13:** el primer PKG reconocio `.aw`, cargo el modo packed y llego a `PC=8C010000`, pero el
JIT leyo opcodes invalidos (`0x03A0`, `0x0900`). La causa era `copy_to_ram`: construia el valor little-endian y
ademas le aplicaba `bswap32`, dejando `fmem1` al reves. Corregido a leer la palabra host y hacer un solo
`bswap32`; verificado con las primeras 8 instrucciones (`D202 9302 2232 A003 ...`). Se anadieron cache packed de
64 KB, limite de DMA a RAM y `aw_trace.log` incondicional.

**Bring-up HW, segunda corrida 2026-11-13:** ya no hay crash ni opcodes invalidos. El juego corre a 60 fps pero se
queda en `PC=8C026A8E`, un poll de `SPG_STATUS` (`0xA05F810C`) esperando el bit `0x2000` (vsync). El JIT lo
trataba como idle; se desactivan `idlerule`/`idlehash` para AW. Ademas se aplica la tabla PVR2 de la BIOS AW
(case13: SPG/VO/FB) antes del payload, porque el arranque directo se saltaba esa inicializacion. Se anade
`[awpvr]` y `[awspg]` para diagnosticar el siguiente intento.

**Tercera corrida HW 2026-11-13:** el juego arranca, renderiza y llega a la pantalla de aviso/coin. El "coin error"
era `0x600280 = 0` (dos fichas activas para siempre). Ahora el registro arranca en `0xFF` y **L1/L2** inserta
moneda 1P (DC D, bit 11), como Flycast; SELECT sigue siendo el menu del emulador. La traza `[awdma]` confirma
el DMA del cartucho y la `[awspg]` el VSYNC.

**Cuarta corrida HW 2026-11-13:** el juego entra y renderiza; el unico "coin error" era el registro de monedas.
Se anaden: moneda en L1/L2, TEST/SERVICE en R3/L3, nombre maple "Atomiswave Controller", etiquetas AW en
CONTROLS (A/C/B/D/E/COIN), pagina **ARCADE** con Free Play / All Bosses / Same Chars / LDM Any, y carga de la
flash DC (`dc_flash_wb.bin`) en modo AW para region/idioma. La flash DC se construye USA/English/NTSC si no existe.

### F3. Dispositivos Atomiswave

- `dc/aw/aw_flash.*`: AMD 128 KB, sectores, autoselect, program/erase, polling y persistencia.
- `dc/aw/aw_board.*`: `0x600280` monedas 1P/2P y salida digital; timing de pulso de moneda.
- `dc/aw/aw_pad.*`: dispositivo maple AW, capacidades `0xFF663F00`, botones activos en bajo.
- En `dc/mem/sh4_area0.cpp`: en modo AW, `0x0..0x1FFFF` y `0xA0000000` van a la flash, no a `bios_b`.
- En `dc/mem/sb.cpp` o `gdromv3.cpp`: en modo AW, el DMA G1 toma `AwCart` como origen.
- En `dc/maple/maple_devs.cpp`: registrar el mando AW en los puertos 0/1.

Criterio: el juego pasa el arranque y llega a leer la flash sin bloquearse.

### F4. Arranque hasta titulo

- Ajustar PVR/TA solo si el juego lo pide; reutilizar el camino DC.
- Verificar el orden de arranque: aviso, SEGA, SNK, titulo.
- Anadir teclas/pad: moneda, Start, A/B/C/D/E.
- Logs `[aw]`, `[awcart]`, `[awflash]` y contadores de DMA/lecturas no mapeadas.

Criterio HW: llega al titulo y acepta moneda/Start.

### F5. Seleccion, pelea y cheats

- Mapear el mando AW: A=LP, B=LK, C=HP, D=HK, E=blowback; Start; moneda.
- Cheats por vblank, con opciones de menu y persistencia:

  | Opcion | Direccion / flag |
  |---|---|
  | Freeplay | ajuste de flash o `QUICK FREE` (`0x8C35AB3C` bit `0x04000000`) |
  | All bosses | `0x8C18F09A=0xFF`; `0x8C35AB40` bits `0x01..0x40` |
  | Same chars | `0x8C35AB40` bit `0x80` |
  | LDM any / leader free | `0x8C35AB3C` bit `0x20000000` (L CMD FREE) |

- `leader free` = **L CMD FREE**: permite que cualquier miembro del equipo use su LDM, no solo el lider elegido en
  Leader Select. La rutina `0x8C07C854` deja de exigir que el personaje sea el lider (`team+0x51`).

- Perfil `ARCADE DELUXE`: freeplay ON, all bosses ON, same chars ON, LDM any ON.
- Perfil `VANILLA`: todo OFF.
- No aplicar cheats en replays/netplay.

Criterio HW: seleccion de equipo, VS y una pelea completa; los flags se ven en el char select.

### F6. Audio y rendimiento

- Verificar AICA/ARM7; revisar escrituras de 32 bits a `KYONEX` si el audio queda mudo.
- Medir fps en pelea con el contador del menu y el log; si no llega a 60, probar:
  - cache mayor de `fs_extracted`;
  - precarga de los archivos de la escena;
  - ajustes del JIT/renderer ya existentes;
  - bajar resolucion o frameskip como ultimo recurso.
- No dar por bueno el rendimiento sin medida en HW.

Criterio HW: pelea jugable, audio presente y sin caidas sostenidas.

### F7. Aceptacion

- Arranque limpio, titulo, arcade, versus, training, seleccion, pelea, fin de ronda y continuar.
- Guardado/carga de flash y ajustes.
- Verificar que `/dev_hdd0/data/NotDC/` y `NOTDC0000` no cambian.
- Documentar resultados y limites.

### F8. Interfaz, controles y configuracion (siguiente bloque)

**Estado que motiva F8:** el juego corre y los cheats de RAM funcionan, pero el usuario reporta:
el navegador abre en `HDD/discs` vacio y hay que cambiar a `ARCADE` con Triangulo; los botones no responden;
`Free Play` no hace freeplay real; no aparece `Atomiswave Controller`; el test menu no se alcanza de forma clara.

#### F8.1 Arranque del navegador y lista de juegos

- Recordar el ultimo dispositivo y carpeta en `NotDC-AW.cfg` (`[ps3] LastDevice`, `LastFolder`), con fallback a HDD.
- Si existe `USRDIR/discs` (PKG completo), arrancar en **ARCADE** la primera vez; si no, en HDD.
- No mostrar una lista vacia como estado inicial: si HDD/discs esta vacio y ARCADE tiene juegos, entrar en ARCADE.
- Etiquetar cada entrada: `[AW]` para `.aw`, `[GDI]`, `[CDI]`, `[CHD]`, `[ELF]`; mostrar el titulo del `.aw`
  leyendo el `TITLE`/`name` del descriptor o el header del `fmem1`.
- Anadir una accion "Refresh" y conservar la posicion al volver de opciones.
- Criterio: al abrir NotDC AW se ve `[AW] kofxi` sin pulsar Triangulo.

#### F8.2 Controles arcade reales

- Separar el input AW del mapa Dreamcast: `ps3/ps3_aw_input.cpp` con estado directo
  `A/B/C/D/E/Start/Coin/Test/Service` y un mapa configurable DS3 -> AW.
- Pagina **ARCADE CONTROLS** en el menu, visible antes del boot y en el menu in-game, con etiquetas
  `A/C/B/D/E/START/COIN/TEST/SERVICE`; no reutilizar los nombres Dreamcast.
- Mantener el mando maple compatible: `name()` vuelve a `"Dreamcast Controller"` si el juego deja de
  detectarlo; la etiqueta Atomiswave se muestra en el menu del emulador, no se fuerza en el guest.
- Traza opt-in `[awpad] condition=%04X reads=%u` para distinguir "el juego no sondea" de "el boton no llega".
- Prueba dirigida en el button config/test menu del juego: pulsar cada boton y comprobar el bit AW.
- Criterio: A/B/C/D/E/Start/Coin/Test/Service responden en el juego y en el test menu.

#### F8.3 Freeplay real y persistencia

- `QUICK FREE` no es el ajuste real de freeplay. El camino correcto es el test menu:
  `COIN SETTINGS -> GAME MODE FREE PLAY`.
- Anadir a la pagina ARCADE un acceso **BOOT TEST MENU** que mantenga TEST durante los primeros N frames
  (por ejemplo 120), para no depender de que el usuario mantenga R3.
- Si el test menu no es alcanzable, localizar el flag real de freeplay en la flash DC o en RAM y escribir
  solo ese flag; no reutilizar `QUICK FREE` como sustituto.
- Persistencia: guardar `dc_flash_wb.bin` y `aw_flash.bin`; verificar tras reiniciar que FREE PLAY,
  LANGUAGE y REGION siguen.
- Criterio: arrancar sin ficha, elegir modo y entrar a pelea; reiniciar y comprobar que sigue en freeplay.

#### F8.4 Region e idioma

- La build actual crea la flash DC USA/English/NTSC si no hay `dc_flash.bin`.
- Entrar al test menu y poner `SYSTEM SETTINGS -> LANGUAGE`; guardar y reiniciar.
- Si el texto del juego sigue en japones, localizar el flag de idioma del ejecutable o de la flash AW
  (no asumir que el idioma del menu DC es el del juego) y anadirlo a la pagina ARCADE como `LANGUAGE`.
- Criterio: warning/titulo/menus en el idioma elegido y persistente.

#### F8.5 Test menu y ajustes permanentes

- R3 = TEST y L3 = SERVICE, pero ademas **BOOT TEST MENU** por menu para que no dependa del timing.
- Documentar que ajustes del test menu se guardan en `dc_flash_wb.bin`/`aw_flash.bin`.
- Anadir un diagnostico `[awcfg]` con el estado de freeplay/idioma/region leido de flash al arrancar.
- Criterio: el usuario puede entrar al test menu, cambiar freeplay/idioma, salir, reiniciar y conservarlo.

#### F8.6 Paginas del menu

- Mover **ARCADE** cerca de EMULATION (no al final) y marcar la fila con un icono/texto `[AW]`.
- En la pagina ARCADE: Free Play, All Bosses, Same Chars, LDM Any, Test Menu, Language, Region.
- En **CONTROLS** cambiar las etiquetas a AW cuando el juego seleccionado sea `.aw`; no esperar a que
  `aw_pending()` sea verdadero, porque eso ocurre despues del menu.
- Mantener los cheats actuales; anadir un perfil `ARCADE DELUXE` y `VANILLA` en una sola fila.
- Criterio: todas las opciones se entienden sin conocer la implementacion y persisten.

#### F8.7 Orden de trabajo

1. F8.1 navegador + etiquetas (bajo riesgo, sin tocar el guest).
2. F8.2 controles directos + traza; probar en button config.
3. F8.3 freeplay real + BOOT TEST MENU.
4. F8.4 idioma/region y persistencia.
5. F8.5/F8.6 pulido de paginas y perfiles.

Cada paso termina con build, prueba HW y log; no se acumulan cambios de interfaz y guest en una sola corrida.

### F9. Modo "solo Atomiswave", arranque directo y controles rapidos para 2 jugadores (reemplaza F8.1 parcial, F8.2 y F8.6)

Decisiones del usuario (2026-11-13): credito compartido (P1 y P2 suman al mismo contador); "Mostrar avanzado" se
queda en Settings; el mini menu START+SELECT se hace; los ajustes del test menu viven en la flash/nvmem AW.

**Datos del repo de estudio (`../kofxi_aw_modding`) que usa este plan**

- Botones canonicos (`docs/specs/botones.md`): boton fisico 1=`A`/LP, 2=`C`/HP, 3=`E`/BLOWBACK, 4=`B`/LK,
  5=`D`/HK; adaptador DC A->A, B->C, C->E, X->B, Y->D; moneda = DC D (bit 11), no va por maple.
  Etiquetas para la UI: `A  LP (puño leve)`, `B  LK (patada leve)`, `C  HP (puño fuerte)`,
  `D  HK (patada fuerte)`, `E  BLOWBACK`, `START`, `COIN`. El menu F1 del recomp ya usa ese formato
  (`LP (A)`, `BLOWBACK (E)`).
- Test menu (`docs/research/runtime/debug-flags-search-2026-11-10.md`): `GAME SETTINGS` (incluye `BUTTON
  SETTINGS`, `CONTINUE`, `DIFFICULTY`), `SYSTEM SETTINGS` (`LANGUAGE`), `COIN SETTINGS` (`GAME MODE FREE PLAY`).
  La flash guarda siete registros: `SXA_CREDIT_CONF`, `SXA_COIN_NUM`, `SXA_SYSTEM_SETTING`, `SXA_CREDIT`,
  `SXA_TOTAL_TIME`, `SXA_MVSPTIME`, `SXA_MVSPINFO`. El flag de FREE PLAY esta, casi seguro, en
  `SXA_CREDIT_CONF` y **no esta decodificado todavia** (`rematch-menu-observaciones-2026-11-13.md`, limites).
- `QUICK FREE` (`0x8C35AB3C` bit `0x04000000`) es un flag de debug, no el freeplay real. No usarlo como
  sustituto final.
- Referencias para decodificar: `recomp/defaults/kofxi_unlocked_console_flash.bin` y
  `artifacts/freeplay-windows/persistent/` (flash persistente que ya guarda SAVE&EXIT del test menu).
- La BIOS AW guarda un ID del cartucho en la flash (sector `0x18000`); si no coincide, borra la SRAM del
  juego (`docs/decomp/bios/flycast_save_reset.md`). Como NotDC AW salta la BIOS, hay que guardar nuestra propia
  `aw_flash.bin` y no tocar ese sector.

**Diagnostico del codigo actual**

- `ps3_menu_run()` ya selecciona ARCADE (`ps3_menu.cpp:1927`), pero si el PKG no tiene el kit o el
  dispositivo no esta marcado como `ARCADE`, entra en HDD/discs vacio; la lista vacia no dice que hacer.
- `g_pad_map` es un mapa unico DS3 -> DC para todos los puertos. El mapa se edita ciclando valores con
  LEFT/RIGHT en una lista de 15 filas.
- La moneda lee solo `kcode[0]`; el P2 no puede insertar ficha.
- El menu SELECT in-game es el de 17 paginas.

**F9.1 Pantalla de inicio de AW (arreglar lista vacia)**

- Si hay juegos AW (dispositivo `ARCADE`/PKG), la primera pantalla es **una lista de juegos AW**, no un
  navegador de archivos: `KOF XI` (titulo del `.aw`), centrada, sin cabeceras de dispositivo ni carpeta.
  La lista vacia y el cambio de dispositivo con Triangulo quedan solo como "Examinar archivos".
- Si no hay ningun `.aw`: mensaje grande y unico: "No hay juegos Atomiswave. Copia kofxi.aw a
  NotDC-AW/discs/ o instala el PKG completo." con X = Examinar archivos.
- Pie de pagina fijo en modo AW: `X Jugar   [] Botones   /\ Test menu   START Opciones   O Salir`.
  El cursor empieza ya sobre KOF XI; X lanza directamente.
- Acciones por juego en la pantalla de inicio (sin entrar a menus): **Botones** (F9.2), **Test menu** (F9.3).
- Guardar ultimo juego y dispositivo en la cfg para no recorrer pantallas.
- Criterio: abrir la app -> ver KOF XI y 3 acciones, sin pulsar Triangulo ni leer una lista vacia.

**F9.2 Quick remap de botones, 2 jugadores**

- Nuevo `ps3/ps3_aw_input.cpp`: estado AW directo por jugador (`A B C D E START COIN TEST SERVICE` +
  direcciones). `maple_aw_controller` y `aw_board` leen de ahi, no de `g_pad_map`. Mapa por jugador
  `g_aw_map[2][...]` (valores de la maquina) con claves cfg `AwP1A`, `AwP2A`, ..., migrando una vez desde `g_pad_map`.
- Pantalla **BOTONES** accesible desde la pantalla de inicio y desde el menu pausa. Dos columnas P1/P2 (L1/R1 o
  izquierda/derecha cambian de jugador; si hay un solo pad se muestra P2 con aviso "sin mando").
  Filas principales, con su etiqueta:

  ```text
  A  LP  puño leve        [ CUADRADO ]
  B  LK  patada leve      [ X        ]
  C  HP  puño fuerte      [ TRIANGULO]
  D  HK  patada fuerte    [ CIRCULO  ]
  E  BLOWBACK             [ R1       ]
  START                   [ START    ]
  COIN                    [ L1       ]
  ----
  Direcciones y otros...  (X para abrir, flujo mas lento)
  ```

- **Quick remap**: X en una fila = "Pulsa el boton del mando..." y se asigna el siguiente boton DS3 detectado
  (O cancela). Un atajo "REMAPEAR TODO" recorre A,B,C,D,E,START,COIN en orden para el jugador activo.
  Si el boton ya se usaba en otra fila, se intercambian (sin botones duplicados por accidente).
- Direcciones, TEST/SERVICE y stick/D-pad swap: bajo la fila "Direcciones y otros...", mismo mecanismo pero no
  entra en el quick remap.
- Presets en una fila: `ARCADE 6 BOTONES` (defecto: Cuadrado=A, X=B, Triangulo=C, Circulo=D, R1=E),
  `FIGHT PAD` y `RESTABLECER`. Boton `Probar`: panel en vivo que ilumina los botones presionados por cada jugador.
- **Moneda compartida**: P1 COIN o P2 COIN suman un credito al mismo contador de `0x600280` (cada pulsacion = un
  pulso, igual que el codigo actual). No hay contador por jugador. `START` de cada jugador sigue siendo propio.
- Opcional en Settings: "Insertar moneda al pulsar START" (para quien no quiera usar el boton de moneda).
- Mantener `name()` "Dreamcast Controller" hacia el guest; las etiquetas AW viven solo en la UI.
- Traza opt-in `[awpad] p=%d cond=%04X`. Criterio: dos DS3 juegan; P1 y P2 remapean A-E, START y COIN en menos
  de 30 s cada uno, sin salir del juego.

**Estado F9.1/F9.2 (2026-11-13, sin prueba en HW):** pantalla de inicio AW y pantalla BOTONES hechas
(`ps3/ps3_aw_input.*` mapa por jugador `AwP1A`.. en la cfg, `ps3/ps3_aw_remap.*` pantalla, `ps3_menu_ui.h`).
Cuadrado en el inicio abre BOTONES; moneda compartida (P1 o P2); Players = 2 como minimo en modo AW; el stick
izquierdo tambien da direcciones. Pendiente de F9.2: abrir BOTONES desde el menu pausa (va con el mini menu de
F9.3), presets "FIGHT PAD" y "Insertar moneda al pulsar START". `.got`: 912 bytes libres.

**Estado F9.3 (2026-11-13, sin prueba en HW):** hecho el mini menu en juego (`ps3/ps3_aw_mini.*`: Continuar,
Botones, Test menu, Cheats, Opciones avanzadas, Cerrar juego; SELECT sigue cerrando el menu) y `R1 Test menu` en la
pantalla de inicio (TEST sostenido ~240 lecturas del pad al arrancar; en juego ~120 al cerrar el menu).
**Hallazgo clave:** el test menu del juego guarda en las flashes que NotDC ya persiste (`aw_flash.bin` y
`dc_flash_wb.bin`; `mem_SaveFlash`), asi que FREE PLAY *persiste sin decodificar el flag*. Pero al salir con
SAVE&EXIT el juego escribe `0x7611` en `SB_SFRES` pidiendo reinicio, y NotDC solo hacia un reset de perifericos con la
CPU corriendo (cuelgue). Ahora, en modo AW, ese reset guarda ambas flashes y vuelve al menu de juegos
(`aw_on_system_reset`); hay que pulsar X para volver a entrar. Pendiente: atajo START+SELECT 2 s, probar en HW que TEST
sostenido abre el test menu (si solo se lee en el titulo, ajustar el momento), y reinicio directo sin pasar por el menu.

**F9.3 Test menu y mini menu en juego**

- Pantalla de inicio: accion **Test menu** arranca KOF XI con TEST pulsado ~120 frames (con la placa viendo
  `0x600280`/bit TEST) hasta que el juego entre al test; despues se libera.
- En juego: SELECT abre **mini menu** de 6 filas (no las 17 paginas): `Continuar`, `Botones`, `Test menu`,
  `Cheats` (Free Play parche, All Bosses, Same Chars, LDM Any, perfiles), `Opciones avanzadas`, `Cerrar juego`.
  `Opciones avanzadas` abre el menu actual; ahi vive "Mostrar avanzado" para mostrar el resto de filas.
- Atajo: START+SELECT 2 s abre el mini menu directo en `Test menu`. `Test menu` en juego fuerza TEST durante 1
  vblank cuando el juego esta en el titulo, o reinicia con TEST pulsado si no esta.
- **Freeplay real** (trabajo de investigacion previo, en el repo de estudio):
  1. Con el recomp/Flycast, entrar a `COIN SETTINGS -> GAME MODE FREE PLAY`, guardar con SAVE&EXIT y comparar
     la flash antes/despues (`xxd` o un script de diff) para localizar el campo de `SXA_CREDIT_CONF`.
  2. Documentarlo en `docs/research/` de `kofxi_aw_modding` con confianza y offset.
  3. En NotDC AW, la fila `Free Play` escribe ese campo en `aw_flash.bin` (con CRC si hace falta) al lanzar,
     o carga una flash base con freeplay activo. Mientras tanto la fila se llama `Free Play (parche)`.
- Criterio: entrar al test menu desde la pantalla de inicio, cambiar FREE PLAY/idioma/dificultad, salir con
  SAVE&EXIT, reiniciar y comprobar que persiste.

**F9.4 Menos perillas**

- En modo AW, el menu pausa muestra solo `Mini menu` (arriba). Settings tiene 5 paginas: `JUEGO`, `CONTROLES`,
  `PANTALLA`, `AUDIO`, `AVANZADO`; las 17 paginas existentes caen bajo `AVANZADO` y solo se ven con
  `Mostrar avanzado` ON (ya existe `g_advanced_preset`).
- Quitar GD-ROM, VMU/Maple, FLASH/CLOCK, NETWORK, DRAW COST y TIMING de la vista basica AW.
- Reemplazar textos Dreamcast ("Disc", "BIOS", "GD-ROM") por "Juego" / "Cartucho" en la UI AW.

**Estado F9.4 (2026-11-13, sin prueba en HW):** la fila `Free Play`/`QUICK FREE` se elimino (menu, cheat, clave cfg); el
freeplay se cambia solo desde el test menu del juego. En modo AW sin Advanced Mode se ocultan FLASH/CLOCK, MAPLE/VMU y
NETWORK; la pagina ARCADE (All Bosses, Same Chars, LDM Any) va justo despues de EMULATION y START en la pantalla de inicio
abre directo en ella; X en opciones lanza el juego sin pasar por la pagina CONTROLS de Dreamcast. `.got`: 872 B libres.
Pendiente: perfiles `ARCADE DELUXE`/`VANILLA` en una fila, atajo START+SELECT 2 s, textos "Disc"/"BIOS" restantes.

**Estado perfiles y logs (2026-11-13, sin prueba en HW):** fila `Profile` al inicio de la pagina ARCADE: `ARCADE DELUXE`
(All Bosses + Same Chars + LDM Any) / `VANILLA` / `CUSTOM` (mezcla, solo se muestra, no se elige). Los tres cheats y el perfil
ahora son *live* en el menu pausa (antes se tomaban como "proximo arranque"). Logs en `NotDC-AW.log`, por etiqueta:
`[awui]` (juegos hallados, seleccion, filas del mini menu, test menu), `[awmap]` (pads de P1/P2, cada boton asignado, guardado),
`[awpad]` (mapa cargado, TEST sostenido, cada cambio del word de cada jugador), `[awcheat]` (perfil y flags escritos),
`[aw]` (SB_SFRES). Si un boton no responde: `[awpad] P1 kcode` cambia = el boton llego al emulador; si no cambia, mirar `[awmap]`.
`.got`: 776 B libres.

**F9.5 Orden de trabajo**

1. F9.1 pantalla de inicio de AW (solo UI, sin guest).
2. F9.2 quick remap, moneda compartida y presets; probar con dos pads.
3. F9.3 mini menu y Test menu; en paralelo, investigar el flag de freeplay en el repo de estudio.
4. F9.3 freeplay real una vez localizado.
5. F9.4 recorte de paginas y textos.

Cada paso termina con build, prueba en HW y log; no se acumulan cambios.

**Riesgos:** TOC (1.440 B libres en `.got`; textos AW en un objeto aparte con `-mminimal-toc`); remapear
mal TEST/SERVICE puede dejar al usuario sin test menu (el mini menu siempre tiene `Test menu`, sin depender del
mapa); persistir el nombre de boton, no el numero; el decode de freeplay puede ser mas largo de lo previsto.

## 4. Archivos previstos

| Archivo | Funcion |
|---|---|
| `ps3/ps3_aw.cpp/.h` | Descriptor, boot AW, glue de menu |
| `dc/aw/aw_rom.cpp/.h` | `fmem1`, manifest, archivos sueltos, cache |
| `dc/aw/aw_cart.cpp/.h` | Registros EPR/MPR, records, DMA source |
| `dc/aw/aw_flash.cpp/.h` | Flash AMD y persistencia |
| `dc/aw/aw_board.cpp/.h` | Monedas y placa |
| `dc/aw/aw_pad.cpp/.h` | Mando maple AW |
| `ps3/ps3_menu.cpp` | Pantalla/entrada AW |
| `ps3/ps3_cheats.cpp` | Cheats KOF XI |
| `dc/mem/sh4_area0.cpp` | Mapa de flash AW |
| `dc/mem/sb.cpp`, `dc/gdrom/gdromv3.cpp` | Origen del DMA G1 |
| `dc/maple/maple_devs.cpp` | Registro del mando AW |
| `Makefile` | `SRC_DIRS`, `-mminimal-toc` para objetos frios |

## 5. Riesgos

- **Rendimiento**: el SH-4 emulado y el PVR2 son el limite real; el loader no lo cambia. Es la incognita principal.
- **TOC**: al 2026-11-13 quedan 1.440 bytes en `.got`. Todo archivo nuevo debe ir con `-mminimal-toc` y sin cadenas nuevas en objetos calientes.
- **Flash AMD**: si el juego espera el fin de borrado/programacion, un modelo incompleto lo deja colgado.
- **DMA G1**: encajar el origen cartucho sin romper el camino GD-ROM de Dreamcast.
- **Archivos sueltos**: muchos archivos pequenos y busquedas en HDD; hace falta cache y handles LRU.
- **Cheats**: `ALL BOSSES` y `SAME CHARS` son flags de debug; probar arcade y versus. No usarlos en replays.
- **ROM**: el `kofxi.zip` local tiene una `fmem1` modificada; usar el `fmem1` original (`6374d8cf...`).

## 6. Prueba HW inicial

Hay dos formas:

**A. PKG todo en uno (recomendado para FTP).** `pkg-full` mete el set `.dec` empaquetado y el descriptor
`discs/kofxi.aw` dentro de `USRDIR`. El menu lo muestra como dispositivo `PKG`; no hay que copiar los 232 MB.

```sh
python3 misc/aw_prepare.py --mode packed --src ../kofxi_aw_modding --out misc/aw_pkg
sh build.sh pkg-full
```

Salida: `build/notdc_aw_ps3_full.pkg` (~236 MB). El flash y el cfg siguen en `/dev_hdd0/data/NotDC-AW/`.

Nota: el `pkg` de ps3dev aborta con la carpeta `fs_extracted` (1.057 archivos); con 8 `.dec` grandes funciona.
Por eso el PKG completo usa `mode=packed`. El kit suelto sigue disponible para FTP/modding.

**B. FTP con `fs_extracted`.** Armar el kit de datos sueltos y copiarlo:

```sh
python3 misc/aw_prepare.py --src ../kofxi_aw_modding --out /tmp/notdc_aw_kit
```

Luego copiar `/tmp/notdc_aw_kit/aw` y `/tmp/notdc_aw_kit/discs` a `/dev_hdd0/data/NotDC-AW/`.

1. Instalar `notdc_aw_ps3_full.pkg` (o `notdc_aw_ps3.pkg` para el kit suelto).
2. Si tambien pruebas un disco Dreamcast, copiar `dc_boot.bin` y `dc_flash.bin` a `/dev_hdd0/data/NotDC-AW/data/`.
3. Para el kit suelto, copiar el set AW a `/dev_hdd0/data/NotDC-AW/aw/kofxi/`:
   - `ax3201p01.fmem1.dec` (original);
   - `fs_extracted/` con `_manifest.txt`;
   - `records0.bin`, `records1.bin`, `records2.bin` (68 KB cada uno, opcionales);
   - `aw_flash.bin` (o dejar que se cree).
4. Para el kit suelto, crear `kofxi.aw` en `/dev_hdd0/data/NotDC-AW/discs/` con las rutas absolutas.
5. Arrancar NotDC AW y comprobar que crea `/dev_hdd0/data/NotDC-AW/`.
6. En el PKG completo, entrar al dispositivo `PKG` y elegir `kofxi.aw`; en el kit suelto, `HDD/discs/kofxi.aw`.
   Buscar `[aw] mode packed` o `[aw] mode fs` y `[aw] boot`.
7. Llegar al titulo; moneda/Start; seleccionar equipo; pelea.
8. Activar `ARCADE DELUXE`; comprobar ALL BOSSES, SAME CHARS y LDM ANY.
9. Enviar `NotDC-AW.log` y `NotDC-AW_prev.log`.

## 7. Estimacion

- F0: hecho.
- F1-F2: 3-5 dias de codigo y compilacion.
- F3-F4: 1-2 semanas, con iteracion en HW.
- F5: 2-4 dias.
- F6-F7: 1-2 semanas.

Total orientativo: 3-6 semanas. La mayor parte del tiempo sera depuracion en consola, no escritura de codigo.

## 8. Preguntas abiertas

- Flag exacto de `COIN SETTINGS -> GAME MODE FREE PLAY`; mientras tanto, inyectar credito o usar `QUICK FREE`.
- Mantener el fallback al ROM empaquetado o solo `fs_extracted`.
- Confirmar que `NOTDCAW01` no choca con otra app instalada.
