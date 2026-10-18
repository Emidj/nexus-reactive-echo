# NEXUS Reactive Echo — mediciones locales de fase 6

Windows x64, OBS/libobs 32.2.2 y Direct3D 11 en un equipo local de pruebas. Son resultados de ese entorno con entrada sintética SDR, no requisitos mínimos ni una garantía en otras GPU.

## Método

El ejecutable benchmark.exe carga la DLL real y dibuja una fuente RGBA con transparencia. Usa timestamps GPU y un rango de frecuencia/disjoint de Direct3D 11 alrededor de obs_source_video_render. El destino se reutiliza; crear/limpiar el destino, esperar al tick, consultar los timestamps y cualquier lectura de píxeles quedan fuera del intervalo medido. No hay lectura de píxeles en el benchmark. Las consultas sincronizan CPU/GPU, por lo que es un microbenchmark serializado y no reproduce todo el solapamiento de una sesión normal de OBS.

Cada versión: 7 modos × 3 tamaños × 2 repeticiones. Por caso se descartan 30 fotogramas y se miden 120 al ritmo de ticks de vídeo de 60 Hz: 5040 muestras medidas por DLL. Las dos repeticiones de la versión anterior se ejecutaron antes de las dos de la optimizada. No se controlaron todas las cargas de escritorio, las frecuencias de GPU ni el estado térmico. No se ejecutaron otras baterías GPU nuestras durante estos intervalos; hubo compilación CPU durante parte de la medición anterior.

Las tablas muestran la media de las dos medianas por caso; no son un P95 conjunto. Los CSV conservan las medianas, P95 y tiempo de envío CPU de cada repetición. No hubo consultas inválidas en ninguna de las 10080 muestras medidas. El coste incluye el dibujo de la fuente y el filtro cuando está activo. No incluye cámara, chroma, análisis de audio, codificación, transmisión ni la interfaz de OBS.

## GPU de la versión optimizada, milisegundos por dibujo

| Modo | 1280×720 | 1920×1080 | 3840×2160 |
| --- | ---: | ---: | ---: |
| Fuente sin filtro | 0,0065 | 0,0115 | 0,0396 |
| Bypass (intensidad 0) | 0,0065 | 0,0130 | 0,0396 |
| Eco original | 0,0189 | 0,0409 | 0,1673 |
| Siluetas | 0,0272 | 0,0530 | 0,2228 |
| Siluetas con degradado | 0,0274 | 0,0534 | 0,2228 |
| Siluetas con LUT 65 | 0,0565 | 0,1133 | 0,4594 |
| Eco original con LUT 65 | 0,0420 | 0,0818 | 0,3450 |

## Cambio de composición

HECHO: el modo siluetas dibujaba las estelas y después el original en dos draws de salida. Ahora un shader compone ambos en un solo draw de salida. Captura e historial siguen separados; no se ha reducido la resolución ni eliminado el alpha.

| Modo | 1080p anterior → optimizado | Cambio observado | 4K anterior → optimizado | Cambio observado |
| --- | ---: | ---: | ---: | ---: |
| Siluetas | 0,0624 → 0,0530 | -15.0% | 0,2607 → 0,2228 | -14.6% |
| Siluetas con degradado | 0,0625 → 0,0534 | -14.5% | 0,2611 → 0,2228 | -14.7% |
| Siluetas con LUT 65 | 0,1185 → 0,1133 | -4.4% | 0,4819 → 0,4594 | -4.7% |

INFERENCIA: la menor cantidad de escritura y mezcla de salida contribuye a la reducción observada en siluetas y degradado. No se atribuye toda diferencia a la optimización: también variaron casos cuyo shader no cambió (el eco original a 1080p pasó de 0,0364 a 0,0409 ms). La diferencia con LUT es más pequeña y puede verse más afectada por la variabilidad. No se demuestra una mejora general del tiempo CPU ni del rendimiento completo de OBS.

## Compatibilidad de imagen

La batería completa anterior pasa con la DLL optimizada. Además, seis comparaciones directas contra la DLL de fase 5 cubren siluetas sin corrección, degradado y LUT, cada uno con intensidad 1 y 0,5, tras 16 pasos. Error máximo RGB: 1/255; error máximo alpha: 1/255. La eliminación de una cuantización intermedia produce pequeñas diferencias en la composición de bordes; no es una salida idéntica bit a bit. Las pruebas específicas de primer plano opaco y bypass exacto siguen pasando.

## Recursos

Las tres texturas propias inicializadas del modo siluetas suman teóricamente 12 bytes por píxel: 10,55 MiB a 720p, 23,73 MiB a 1080p y 94,92 MiB a 4K. La captura intermedia de OBS añade al menos otra textura RGBA8; una LUT 65 RGBA32F añade aproximadamente 4,19 MiB. Es una estimación de superficies, no VRAM medida: excluye shaders, buffers del driver, la fuente, el destino del benchmark y demás recursos de OBS.

El ejecutable registra el contador bnum_allocs y memoria privada del proceso durante la prueba continua. La memoria privada no equivale a VRAM. obs_add_data_path deja dos asignaciones globales del registro de rutas de libobs tras obs_shutdown; se miden por separado de las del plugin y se retira la ruta al terminar. El array del registro permanece hasta el fin del proceso. No se presenta ese contador global como una fuga del filtro.

## Prueba continua de cinco minutos

PASS: 18030 fotogramas de degradado a 1920×1080, ticks configurados a 60 Hz y 300,479 segundos de ejecución. Cero consultas GPU inválidas. El contador bnum_allocs permaneció en 1345 en todas las muestras de 30 segundos (delta final 0). Memoria privada inicial: 236,301 MiB; muestras posteriores entre 237,199 y 237,328 MiB, con último valor 237,297 MiB. No se observó crecimiento sostenido tras la subida inicial de aproximadamente 1 MiB.

Después de destruir fuente/filtro y cerrar libobs quedaron 2 asignaciones, exactamente las 2 medidas al registrar la ruta global de datos de libobs. No quedaron asignaciones adicionales respecto a ese registro. La prueba fue de modo estable, sin audio, cámara ni emisión; no demuestra estabilidad durante horas ni en todos los cambios de configuración. Los cambios de tamaño, audio, reactivación y destrucción se cubren por las pruebas funcionales separadas.

Revisión posterior en OBS portátil con chroma oficial y degradado guardado: efecto visible con la DLL optimizada. La sesión se deja abierta para revisión local; no se ha validado su cierre en esta ronda.

## Repetir las pruebas

Después de compilar, ejecutar desde la carpeta del proyecto:

```powershell
.\benchmark.ps1 -OutputPrefix build\mi-benchmark
.\benchmark.ps1 -OutputPrefix build\mi-sesion -SoakSeconds 300
```

Los scripts usan la instalación OBS indicada mediante -ObsRoot y permiten otra DLL con -DllPath. Los CSV de la prueba continua solo tienen cabecera: los datos de memoria y duración se guardan en el log. La versión anterior se conserva en work/phase5-baseline/ para repetir la comparación. Evidencia publicada en tests/evidence/phase6/.

## Límites pendientes

Sigue pendiente validar otras GPU, cámaras físicas, sesiones de varias horas y cargas de emisión reales. La semejanza exacta con prueba.mp4 y con la maqueta no se deduce de estos benchmarks. La duración y las transformaciones del historial siguen dependiendo de los FPS.
