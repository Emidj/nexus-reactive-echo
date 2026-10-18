NEXUS Reactive Echo 1.2.0 beta — Windows x64
Un plugin independiente de DjEmiMas para OBS Studio 32.2.2 o posterior.

INSTALACIÓN
Cierra OBS. Abre el instalador, selecciona la carpeta de OBS y pulsa
Instalar / Actualizar. La instalación estándar requiere permisos de administrador.
Para OBS portátil, marca la opción y selecciona la carpeta con portable_mode.txt.

USO
En los filtros de la fuente añade NEXUS Reactive Echo después del chroma.
Aplica un preset para comenzar o activa Siluetas detrás del original.
En Color puedes elegir LUT .cube o degradado suave/con bandas.
En Audio selecciona una fuente OBS y activa Modo reactivo. El nivel controla
la intensidad y la expansión adicional; el silencio elimina las siluetas tras
la caída configurada. Magnitud de la reacción escala esa respuesta sin dejar
un efecto fijo. Los filtros nuevos arrancan reactivos; los antiguos conservan
su modo guardado. Desactivar Modo reactivo permite un efecto manual constante.
Intensidad cero conserva la imagen original. El efecto procesa SDR.

DESINSTALACIÓN
Usa Desinstalar en este instalador, el desinstalador de la carpeta del plugin
o Aplicaciones instaladas de Windows (instalación estándar).
Solo retira archivos propios. No modifica las escenas ni los ajustes de OBS.
Los archivos de documentación modificados se conservan; una DLL modificada
bloquea la desinstalación para evitar eliminar una versión ajena.

ALCANCE DE LA BETA
Validado localmente con OBS 32.2.2 y Direct3D11 en un equipo local de pruebas.
No constituye validación de otras GPU ni de emisiones de varias horas.
Las LUT externas se guardan por ruta y no se copian al instalar.

Licencia GPL-2.0-or-later. Véase COPYING.txt y el paquete de código fuente.

Los controles mantienen el historial al arrastrarlos. Limpiar eco lo vacía.

REFERENCIA REACTIVE 2
Aplica el preset Reactive 2 (referencia). Activa Modo reactivo y elige la
fuente de audio. El color se aplica a la copia antes de la recursión.
Para utilizar la cadena de LUT de tu escena, selecciona Tipo de color: LUT,
y asigna Gothic LUT #23.cube como LUT base y Thermograph _ 10.cube como LUT
reactiva. Las LUT externas no se redistribuyen ni se copian al instalar.
También puedes usar las 12 paletas de degradado o tus propios colores.
La LUT base afecta al primer plano, incluso cuando no hay siluetas por silencio.
Intensidad cero sigue conservando la imagen original sin procesar.
