## Versión 1.2.0 - desplazamiento reactivo por eje

En Destinos de audio hay un control Desplazamiento X y otro Desplazamiento Y, cada uno con casilla independiente y deslizador de -200 a +200. El centro es 0. X positivo mueve las copias a la derecha y negativo a la izquierda; Y positivo hacia abajo y negativo hacia arriba. La cantidad se multiplica por el nivel de audio suavizado y la profundidad de reacción.

Los controles nuevos empiezan desactivados y en cero. Los ajustes guardados con el antiguo Desplazamiento X/Y conservan el resultado: su activación y cantidad se copian a los dos ejes. Guardar/recuperar mi ajuste incluye ambos controles. No se añaden controles X1/X2 o Y1/Y2. La interfaz sigue los cinco idiomas de OBS y conserva el pie a plugin from DjEmiMas con enlace a Discord.

Los manuales 1.1.0 describen el control combinado de la versión anterior; este apartado documenta su sustitución en 1.2.0.

## Versión 1.1.0 - cinco idiomas

Interfaz del plugin: español, inglés, alemán, francés e italiano, según el idioma de OBS. Los idiomas no incluidos usan español. Instalador: los mismos cinco idiomas, según Windows, con inglés como alternativa. El manual PDF actualizado está en español y usa un personaje ficticio; los efectos son renders de la DLL real.

# NEXUS Reactive Echo

## Versión final 1.0.0

Publicación del comportamiento aprobado en 0.3.2, sin cambios de renderizado.

Modos disponibles: **Actual (aprobado)** y **Zoom**. Grid, Radial y Spiral se han retirado del selector y del renderizado. Los ajustes antiguos de esos modos vuelven a Actual; Zoom conserva su identificador y configuración.

Zoom escala el historial alrededor de Centro X/Y, sin rotación. Los desplazamientos X/Y permiten desplazar las copias; el primer plano mantiene su tamaño. Centro X/Y va de 0 a 1. El control de separación radial se ha retirado.

Los cinco destinos combinables de audio se mantienen: intensidad LUT, escala, desplazamiento X/Y, alpha del feedback y cambio de color. Empiezan desactivados y usan la fuente, profundidad y suavizado existentes. La persistencia se limita a 0–0.98. El silencio retira las siluetas tras la caída configurada.

Las casillas Activar LUT, Activar LUT base y primer plano y Activar LUT reactiva permiten desactivar cada LUT sin borrar su ruta. Las escenas anteriores mantienen las LUT activadas por defecto. Las LUT comerciales no se distribuyen.

## Versión 0.2.0

- Corregido el arrastre de Posición del color intermedio: el panel ya no se reconstruye con cada paso. Verificado en OBS portátil: 0.50 a 0.89 y 0.89 a 0.11 en sendos arrastres. La muestra de paleta se refresca al cambiar otro control o reabrir el panel.
- Ocho paletas nuevas: Synthwave, Océano, Ácido, Lava, Caramelo, Aurora, Monocromo y Oro y petróleo; doce incluidas más Personalizado.
- Nuevo preset **Reactive 2 (referencia)**: aplica recursión de color, persistencia 0.8, zoom 1.14 y opacidad 0.3843. Conserva la selección de audio. Seleccionar la fuente de audio OBS que se quiera usar.
- En Cadena de referencia, la LUT base opcional también afecta al primer plano; la LUT reactiva solo afecta a las copias. Para la cadena observada, seleccionar Gothic LUT #23 y Thermograph _ 10 respectivamente. No se distribuyen esas LUT comerciales. Alternativamente elegir Degradado sin archivos externos.
- El color reactivo usa magnitud de audio suavizada; el silencio retira las copias tras la caída configurada. El primer plano conserva la LUT base si se eligió. Intensidad cero conserva el bypass exacto.
- Los modos anteriores siguen disponibles. La semejanza visual exacta aún requiere revisión del usuario; las pruebas funcionales no la acreditan.

## Estado

Fases 1, 2, 3 y 4 implementadas y validadas técnicamente con la DLL real, libobs/Direct3D 11 y la interfaz de OBS en modo portátil. La fase 5 está implementada y validada funcionalmente: interfaz nativa agrupada, selector LUT/degradado, paletas, seis presets y guardado personal. La fase 6 tiene una primera validación local de optimización, benchmarks GPU y cinco minutos continuos; sesiones largas y otras GPU siguen pendientes. La referencia prueba.mp4 ya se ha inspeccionado. La comparación en REFERENCE.md confirma diferencias de composición y siluetas que requieren corregir el renderizado antes de cerrar el diseño del efecto. La semejanza visual sigue pendiente.

1. Passthrough — validada.
2. Feedback y recursión GPU — validada.
3. Color y LUT .cube — validada.
4. Reactividad al audio de una fuente OBS — validada.
5. UI nativa y presets — validada funcionalmente.
6. Optimización y benchmarks — primera validación local completada; ver BENCHMARKS.md.

No iniciar una fase posterior hasta compilar y probar la actual. Los controles se organizan en Presets, Eco y siluetas, Color y Audio, sin pestañas duplicadas. Hay cuatro paletas de degradado, una personalizada y seis presets completos del efecto.

## Presets y ajuste personal

Elegir un preset y pulsar **Aplicar preset**: elegir en la lista solo selecciona qué se aplicará, no cambia los parámetros visuales. Seleccionar en la lista conserva el historial del eco. La lista no es un indicador del preset activo después de editar, deshacer o recuperar un ajuste.

| Preset | Resultado |
| --- | --- |
| Siluetas cálidas | Degradado amarillo, rojo y violeta detrás del original |
| Neón | Degradado cian y magenta con expansión mayor |
| Espiral suave | Siluetas con zoom, rotación y desplazamiento moderados |
| Eco suave (modo original) | Mezcla recursiva del modo anterior |
| Reposo (sin efecto) | Intensidad cero; el resto de ajustes se conserva |

Los presets no cambian la fuente de audio, la activación del detector, profundidad, sensibilidad, umbral ni tiempos de ataque/caída. Sí ajustan Expansión adicional con audio como parte de la geometría. Con reactividad activa y profundidad 1, un preset puede quedar invisible en silencio: elegir una fuente de audio válida o desactivar la reacción para revisarlo manualmente.

**Volver al ajuste anterior** permite alternar entre el estado anterior y el último aplicado o recuperado. **Guardar mi ajuste** guarda un único ajuste visual personal por instancia del filtro; reemplaza el guardado anterior. **Recuperar mi ajuste** restaura geometría, color, degradado y ruta/intensidad de LUT sin tocar el audio actual. El ajuste y el estado anterior se guardan con la escena de OBS; no se crean archivos externos de presets. Las LUT externas siguen necesitando su archivo original.

Los presets incluidos conservan la ruta de la LUT y los colores personales. Desactivan la contribución de LUT al aplicar su aspecto para evitar que una LUT anterior lo altere. Guardar el ajuste personal no reinicia el historial. No incluye biblioteca de varios presets personales ni importación/exportación en esta fase.

## Corrección visual: siluetas tras el chroma

La entrada ya debe tener el fondo eliminado. Colocar NEXUS después del chroma y activar **Siluetas detrás del original**. El modo anterior sigue siendo el predeterminado para conservar escenas guardadas.

Prueba inicial manual: Intensidad 1, Persistencia 0.94, Zoom 1.03, Rotación 0, desplazamientos 0, Cambio de tono por paso -8 grados. Estos valores son un punto de partida, no un preset validado contra la cámara del usuario. Desactivar temporalmente la reacción al audio permite apreciar el historial estable.

El modo nuevo mantiene un historial GPU de siluetas naranja/amarillo que cambia de tono al envejecer; el original se compone encima. El alpha puede extenderse fuera del recorte actual, siempre dentro del tamaño de la fuente. El primer plano opaco conserva sus píxeles; en bordes semitransparentes se ve la capa de estelas por debajo según la composición alpha normal.

Activar **Colorear estelas** y elegir **Tipo de color → LUT y ajustes de color** permite usar exposición, contraste, saturación, tono y la LUT 3D exclusivamente sobre las bandas. El primer plano se compone encima sin esa corrección. Es optativo y está desactivado por defecto para respetar escenas guardadas con controles de color antes inactivos. La corrección se aplica al dibujar el historial, no se guarda en la recursión. Intensidad mezcla tanto la corrección como la opacidad de las estelas; Intensidad de LUT ajusta su contribución. El selector y los controles se deshabilitan cuando esta opción está apagada. En el modo anterior el color sigue afectando a la imagen final. El audio controla su opacidad. Expansión adicional con audio añade entre 0 y 0.1 al zoom por paso, multiplicado por la envolvente suavizada y la profundidad de reacción; actúa solo con Siluetas y Reactividad al audio activas. Su valor inicial es 0 para conservar los ajustes anteriores. Con Profundidad 0 no hay expansión adicional. Por ejemplo: zoom 1.005 + expansión 0.025 × profundidad 1 × nivel 0.75 = zoom efectivo 1.02375. No escala la capa frontal. Intensidad o Persistencia cero producen bypass exacto. Las transformaciones siguen siendo por fotograma. La captura del original añade una tercera textura a las dos del historial; no se ha medido el coste.

Esta corrección tiene pruebas con la DLL real en Direct3D 11 y una entrada sintética con transparencia. También se ha probado en OBS portátil con una silueta sintética transparente, pulso de audio, mute/unmute y reapertura. No acredita todavía semejanza con prueba.mp4 ni validación con la cámara del usuario. La fase 5 está validada funcionalmente; las mediciones locales están en BENCHMARKS.md.

## Selector LUT o degradado

En Color, activar **Colorear estelas** y elegir **Tipo de color**:

- **LUT y ajustes de color**: selector .cube y corrección existente.
- **Degradado**: barra de vista previa y paletas Cálido, Neón, Fuego, Hielo o Personalizado.

Personalizado permite elegir tres colores mediante el selector nativo: inicio intenso, intermedio y final tenue. Posición del color intermedio ajusta la transición del efecto, entre 0.05 y 0.95. La rampa recorre RGB según la opacidad restante del historial; no representa posiciones de la cámara ni un tiempo exacto en segundos. La barra es una vista previa: los colores se editan con los botones y la posición con el deslizador.

LUT y degradado son alternativas, no se aplican simultáneamente. Cambiar entre ellos conserva la ruta de la LUT y los colores personales. El degradado se aplica al dibujar las estelas, sin alterar su alpha ni guardarlo en la recursión; el primer plano queda encima. En el modo original sin siluetas se mantienen los controles de LUT existentes y no se ofrece degradado.

La selección y los colores se guardan con el filtro en OBS. Las doce paletas no necesitan archivos externos. Compatibilidad: por defecto se mantiene el modo de color anterior. El detector de audio y su UUID no cambian al elegir paleta. La maqueta del chat anterior no se ha podido recuperar como imagen y no se declara coincidencia exacta con ella.

## Cadena con chroma de OBS

En los filtros de efecto de la cámara, colocar **Chroma primero y NEXUS Reactive Echo después**. El modo de siluetas necesita recibir el alpha recortado. La cadena se ha comprobado con el filtro chroma oficial de OBS y una imagen sintética sobre verde, incluidos bordes parcialmente transparentes; evidencia en tests/evidence/chroma-chain/.

El generador de esa prueba usa solo Python estándar. test.ps1 ejecuta chroma-chain-test con obs-filters.dll de la instalación OBS indicada. No requiere micrófono ni cámara física para probar la integración; el parecido con la cámara del usuario sigue pendiente.

## Uso de la fase 2

El filtro comienza con Intensidad = 0 para conservar exactamente la imagen original. Para ver el eco, aumentar Intensidad. Persistencia controla la mezcla con el fotograma anterior; Zoom, Rotación y Desplazamiento transforman ese historial. Limpiar eco descarta el historial.

El procesamiento emplea dos texturas GPU alternas y la captura de entrada gestionada por OBS. No descarga imágenes a CPU. Cada fotograma se calcula una vez aunque varias vistas soliciten la misma fuente.

La mezcla conserva el alpha de la imagen actual. Los ecos quedan dentro de su silueta actual; no producen una estela opaca fuera de una fuente transparente. RGB usa alpha premultiplicado y los píxeles totalmente transparentes quedan a cero. Intensidad cero y Persistencia cero omiten el procesamiento con identidad exacta.

Se limpia el historial al cambiar tamaño, retirar el filtro, ocultarlo, desactivarlo durante un tick o reanudar tras más de 250 ms sin renderizar. Las transformaciones y la persistencia se expresan por fotograma procesado: su duración depende de los FPS. Esta etapa aplica el efecto a fuentes SDR; una fuente que declara otro espacio de color pasa sin efecto. HDR y normalización temporal no se han validado como efectos compatibles.

## Color y LUT de la fase 3

Exposición, contraste, saturación y tono se aplican después del eco, a RGB sin premultiplicar; al terminar se vuelve a premultiplicar con el alpha original. La corrección no se guarda en el historial, por lo que no se acumula en cada recursión. Son controles artísticos sobre RGB SDR; no constituyen una transformación calibrada de cámara ni gestión HDR. El orden es exposición, contraste con pivote 0.5, saturación, tono y LUT.

Los controles funcionan con Persistencia = 0 siempre que Intensidad sea mayor que cero. Intensidad es el control maestro del filtro y con cero conserva identidad exacta, también si hay una LUT seleccionada. Intensidad de LUT controla únicamente la mezcla de la LUT.

Formatos admitidos: LUT 3D .cube con LUT_3D_SIZE entero entre 2 y 65, datos en orden rojo más rápido, interpolación trilineal y DOMAIN_MIN/DOMAIN_MAX opcionales. Se aceptan TITLE entre comillas, comentarios, decimales con punto y notación científica, UTF-8 con o sin BOM, CRLF y rutas con acentos. La entrada fuera del dominio se limita al borde de la LUT. La salida se limita al rango SDR.

El lector exige exactamente N³ filas. Límite de archivo: 32 MiB; línea inferior a 1024 bytes; valores numéricos finitos entre -65504 y 65504; dominio con amplitud mínima 0.000001 por canal. LUT 1D, shapers y LUT_3D_INPUT_RANGE no se admiten en esta etapa y producen un mensaje de formato no compatible.

Una LUT inválida o ausente desactiva la LUT anterior, manteniendo el eco y los controles de color. El estado del formato se muestra bajo el selector. Recargar LUT relee el mismo archivo cuando su contenido ha cambiado. El archivo se lee al seleccionarlo o recargarlo; no se lee por fotograma. Cada instancia tiene su propia textura LUT.

OBS guarda la ruta de la LUT, no una copia del archivo: conservarlo en esa ubicación permite recuperarlo al reabrir la escena.

## Reactividad al audio de la fase 4

Activar Reactividad al audio y elegir una fuente de audio existente en OBS. El detector usa el pico más alto de sus canales después del volumen de la fuente, respeta mute y desactivación y modula la intensidad general del filtro. No modifica ni duplica el audio. Los filtros nuevos arrancan con modo reactivo activado. Los filtros existentes conservan su elección guardada; si estaban en manual, activa el modo reactivo.

- Magnitud de la reacción: escala la respuesta al nivel de audio; 0 desactiva el efecto reactivo. Ningún valor mantiene una intensidad fija en silencio.
- Sensibilidad: ganancia del detector entre -24 y +24 dB.
- Umbral de silencio: nivel que produce reacción cero, entre -80 y -6 dB. Por defecto -48 dB.
- Ataque y Caída: constantes de tiempo exponenciales en milisegundos, no duraciones hasta alcanzar exactamente el destino. Por defecto 30 ms y 250 ms; aproximadamente un 63% del recorrido por constante de tiempo.

El pico en dB, sumada la sensibilidad, se normaliza entre el umbral y 0 dB y se limita a 0–1. La envolvente suavizada multiplica la intensidad manual según la profundidad. Con cualquier magnitud, silencio, mute o una fuente ausente llevan a identidad exacta tras la caída; no existe suelo manual. Los filtros nuevos activan el modo reactivo por defecto. En filtros antiguos se respeta audio_enabled guardado: activa «Modo reactivo (sin audio, sin efecto)» y selecciona la fuente OBS. Desactivar el modo reactivo permite un efecto manual constante.

Si dejan de llegar callbacks durante 250 ms, el detector considera silencio e inicia la caída. La selección se guarda por UUID de OBS, de modo que renombrar la fuente mantiene la conexión. Si una fuente guardada aún no existe durante la carga de la escena, se reintenta la conexión una vez por segundo. Una fuente eliminada no queda retenida por el filtro. Actualizar fuentes renueva el selector y muestra Fuente no disponible si falta la seleccionada.

Esta fase implementa reacción al nivel, sin análisis por bandas ni detección de tempo. La envolvente usa segundos del tick de vídeo; la persistencia y las transformaciones del eco siguen siendo por fotograma, como en la fase 2.
## Rendimiento medido

La composición de siluetas y primer plano usa una sola pasada de salida. En el equipo local de pruebas, la media de medianas del modo degradado pasó de 0.0625 a 0.0534 ms a 1080p y de 0.2611 a 0.2228 ms a 4K. Son mediciones del dibujo de fuente/filtro, sin cámara, audio, encoding ni emisión; no son tiempos totales de OBS ni requisitos universales. Otros modos variaron por carga y estado de la máquina.

La batería funcional completa pasa y la comparación entre versiones muestra diferencias máximas de 1/255 en RGB y alpha por eliminar cuantización intermedia. Bypass y primer plano opaco siguen superando sus comprobaciones exactas. Prueba continua de 300.48 s a 1080p: sin crecimiento de asignaciones, memoria privada estable tras el calentamiento. Consulte **BENCHMARKS.md** para método, CSV, recursos y límites. Sesiones de varias horas, otras GPU y la cámara real siguen pendientes.

## Compilar

Requisitos: Python 3, compilador oficial Zig 0.15.2, cabeceras oficiales OBS 32.2.2 y OBS instalado.

Por defecto se buscan herramientas en build-tools/zig-x86_64-windows-0.15.2/ y build-tools/obs-studio-32.2.2/. También se pueden indicar NEXUS_TOOLS_DIR, ZIG_EXE, OBS_SDK_DIR y OBS_INSTALL_DIR. El proyecto no enlaza código ni motores de otros plugins; las herramientas y las cabeceras son dependencias de compilación.

Ejecutar `python build.py` desde esta carpeta. La DLL incluye el shader; no requiere archivos .effect externos ni paquetes Python de terceros.

Salida: dist/nexus-reactive-echo/bin/64bit/nexus-reactive-echo.dll.

## Validación

Después de compilar, ejecutar `./test.ps1`. Su parámetro ObsRoot permite señalar otra instalación de OBS. Las pruebas no instalan el plugin ni modifican escenas de usuario.

- passthrough-test: 12 comparaciones RGBA exactas y comprobaciones de dimensiones, en tres tamaños y cuatro estados.
- feedback-test: 25 comprobaciones de recursión, decaimiento, traslación, zoom, rotación, alpha, limpieza, retorno sin efecto, caché de varias vistas y cambio de tamaño.
- cube-test: 32 comprobaciones del lector, incluidos archivos inválidos, tamaños 2/17/65, dominio y configuración regional española.
- color-test: 34 comprobaciones con la DLL real, LUT 3D en GPU, interpolación, orden de canales, controles, alpha, errores, recarga, varias instancias y ausencia de acumulación de color.
- audio-test: 31 comprobaciones con audio sintético enviado a fuentes OBS y renderizado de la DLL real: respuesta de nivel, canales, volumen, mute, silencio, cambios de fuente, UUID, reconexión, suavizado y destrucción con audio concurrente.

La prueba visual se realizó en work/obs-qa con un patrón RGBA y una animación sintética. Se comprobó registro del filtro, imagen sin cambios en fase 1, eco visible en fase 2, controles, intensidad cero, limpieza y guardar/reabrir escena. En fase 3 se comprobó el mensaje de LUT incompleta, selección mediante el diálogo nativo de una ruta con acentos, cambio visible de color con LUT, saturación cero en blanco y negro y persistencia tras reiniciar OBS. En fase 4 se comprobó una fuente multimedia WAV de prueba: activación, fuente seleccionada, respuesta al pulso, retorno a la imagen original con mute, recuperación y guardar/reabrir. Las sesiones completas terminaron con cero fugas reportadas por OBS.

Evidencias conservadas en tests/evidence/. Los resultados corresponden a esta máquina y no son benchmarks ni garantizan el mismo comportamiento en otras GPU. Véase STATUS.md para el alcance exacto.

Licencia: GPL-2.0-or-later; texto incluido en COPYING.txt.



# Bandas de color

En Color → Degradado, «Transición de color» permite elegir degradado suave o 3, 4, 6, 8 y 12 bandas. El valor predeterminado sigue siendo suave. Las bandas usan los mismos colores y posición intermedia del degradado; cambian la distribución del color según la opacidad del historial, sin cuantizar el alpha ni modificar el primer plano. La vista previa y el ajuste personal incluyen esta selección. Los presets incluidos conservan su aspecto suave.

Comparación reproducible sobre una grabación: `./camera-study.ps1 -InputVideo 'ruta/al/video.mkv' -Bands` después de compilar. Produce una muestra reducida de dos segundos sin audio, con chroma oficial OBS, degradado suave, 3 bandas y 6 bandas. Los parámetros de chroma de esta herramienta están calibrados provisionalmente para FAKE3.mkv; no son universales.

## Instalador Windows

`python package.py` genera NEXUS Reactive Echo 0.1.2 beta: instalador EXE, ZIP binario, ZIP de código fuente y SHA256SUMS.txt en dist/. Ejecutar después de compilar y validar con test.ps1. El instalador incluye la DLL probada; la imagen lateral y el icono son los archivos NEXUS elegidos por el usuario.

Instalación estándar en ProgramData/obs-studio/plugins/nexus-reactive-echo (requiere administrador), o instalación portátil en la carpeta OBS seleccionada con portable_mode.txt. Comprueba OBS x64 32.2.2 o posterior, hashes del paquete y rutas, bloquea cambios con OBS abierto y conserva escenas y ajustes. Incluye desinstalador propio y entrada de desinstalación de Windows para modo estándar. Archivos propios modificados reciben protección; los datos ajenos no se eliminan.

Validación aislada: `python tests/installer-test.py`. Solo crea carpetas nuevas dentro de build/, no instala en el OBS habitual. El control de proceso abierto se prueba con un proceso obs64.exe controlado; el registro de Windows y la elevación real de administrador del modo estándar siguen pendientes de validación en un entorno de instalación real.


Los cambios de controles conservan el historial para evitar cortes durante el arrastre. Limpiar eco, cambiar entre modo original/siluetas, bypass, cambios de tamaño y reanudación tras una pausa pueden reiniciarlo.
