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
