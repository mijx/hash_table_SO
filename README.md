# Sistema de Búsqueda de Préstamos de la Biblioteca Pública de Seattle

Este documento describe el sistema de búsqueda de préstamos de la Biblioteca Pública de Seattle, incluyendo la descripción del dataset utilizado, los campos clave, los criterios de búsqueda implementados, la justificación de estos criterios y ejemplos de uso del programa.

## 1. Descripción de la Base de Datos

Este conjunto de datos contiene un registro de todos los préstamos físicos de ítems realizados en la Biblioteca Pública de Seattle, desde abril de 2005 hasta septiembre de 2017. No incluye renovaciones. Su descarga gratuita está disponible al público en: https://www.kaggle.com/datasets/seattle-public-library/seattle-library-checkout-records?select=Checkouts_By_Title_Data_Lens_2012.csv.

## 2. Campos del Dataset Seleccionado

A continuación, se presenta una descripción detallada de los campos del dataset proporcionado, incluyendo los rangos de entrada válidos para cada uno.

* **BibNumber** 
    * Descripción: Identificador único del registro bibliográfico (título o ítem en el catálogo).
    * Tipo: Numérico.
    * Ejemplo: 2700635.

* **ItemBarcode** 
    * Descripción: Código de barras único asignado a cada ejemplar físico.
    * Tipo: Cadena numérica.
    * Ejemplo: 0010072332389.

* **ItemType** 
    * Descripción: Tipo de ítem prestado (libro para adultos, DVD, audiolibro, etc.).
    * Tipo: cadena de 3 a 5 letras.
    * Ejemplo: acbk.

* **Collection** 
    * Descripción: Sección o colección de la biblioteca a la que pertenece el ítem (CA9-Geneal. Bibliographies, CS 8 - NW Arts, etc.).
    * Tipo: Código de texto, generalmente de 4 a 6 caracteres.
    * Ejemplo: namys.

* **CallNumber** 
    * Descripción: Número de clasificación del ítem, usado para localizarlo físicamente en la biblioteca.
    * Tipo: Cadena alfanumérica.
    * Ejemplo: 133.32424 M7813T 2010.

* **CheckoutDateTime** 
    * Descripción: Fecha y hora exactas en que el ítem fue prestado.
    * Tipo: Fecha y hora en formato MM/DD/YYYY hh:mm:ss AM/PM.
    * Ejemplo: 04/04/2011 10:17:00 AM.

## 3. Criterios de Búsqueda Implementados

Los criterios de búsqueda implementados son los siguientes:

* ID del recurso: BibNumber.
* Año de búsqueda(opcional).
* Número de mes (opcional).

### Justificación de los Criterios 

Se permiten campos opcionales, año y mes, para permitir flexibilidad en el uso de la interfaz, ya que habilita al sistema para atender tanto a necesidades de consulta precisas como a exploraciones más amplias. Los dos tipos de consultas son:

* **Consultas Específicas**: permite obtener los registros de un año y mes en particular, permitiendo al sistema reducir el espacio de búsqueda, mejorando significativamente el rendimiento y la velocidad de las consultas para ítems específicos dentro de ese periodo.
* **Búsquedas Globales (a través del tiempo)**: los usuarios pueden optar por especificar solamente el ID para realizar búsquedas de un ítem a lo largo de toda la línea de tiempo del dataset. Útil para encontrar un ítem sin importar su año de registro.

Los criterios de búsqueda realacionados a la fecha de checkout es un campo primarios, debido a su importancia para filtrar y segmentar la información. Esta característica se vuelve fundamental dado que el dataset abarca más de una década de datos.

El ID de **BibNumber** se designa como otro campo de búsqueda primario. Su función es actuar como un identificador único para cada ejemplar dentro del dataset. El uso de este campo garantiza gran presiciónrecisión absoluta, dado que al buscar por ID se elimina cualquier ambigüedad, asegurando que el resultado sea exactamente el ejemplar deseado.

### Rangos de Valores Válidos para las Entradas 

* **ID del recurso**: 1, 2 o 3, para seleccionar entre las opciones respectivas “Buscar por ID y año”, “Buscar ID en todos los años” o “Salir”.
* **Año de búsqueda**: desde 2005 hasta 2017.
* **ID del recurso físico**: número entero largo.

## 4. Ejemplos de Uso del Programa

### 4.1. Pasos de Ejecución 

Para compilar cada componente de tu programa, usa los siguientes comandos:

```bash
gcc constructor.c -o constructor
gcc frontend.c -o frontend `pkg-config --cflags --libs gtk+-3.0`
gcc backend.c -o backend
```

Una vez compilado, el primer paso es generar el archivo índice de los hashes de todos los archivos CSV. Para hacer esto, ejecuta:
```bash
./constructor
```
Después de generar el índice, necesitas crear las tuberías de comunicación:
```bash
mkfifo /tmp/frontend_input /tmp/frontend_output 2>/dev/null || true
```
Ahora, en una terminal, ejecuta el backend:
```bash
./backend
```

Finalmente, en otra terminal, ejecuta el frontend:
```bash
./frontend
```
### 4.2. Ejemplos específicos de búsquedas
#### Ingresando ID, año y fecha
<img src="demo/tres_parametros.png" alt="Ejemplo 1" style="width:80%;">

#### Ingresando ID y año
<img src="demo/dos_parametros.png" alt="Ejemplo 2" style="width:80%;">