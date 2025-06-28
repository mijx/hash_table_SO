Aquí el hash se cambió para que se generara en base al ID, mes y año. Para probar más adelante si eso disminuye las colisiones en el hash.

También se corrigió el hecho de que el backend dejara de correr abruptamente cuando los criterios de busqueda formaban un hash que no se encontraba en la tabla (no se encontraba en header.dat), el problema era:
1. Que se llamaba a return sin devolver ninguna respuesta al frontend, a través de la tubería, que hiciera que dejara de estar esperando por el resultado de la búsqueda que nunca se iba a realizar.
2. Luego de quitar el return me había equivocado porque nunca se abría el indexer.dat si saltaba al final del codigo cuando la busqueda no tenía hash, pero siempre trataba de cerrarlo al final del codigo (provocando un error double free, por cerrar un archivo que nunca había sido abierto).


Como este código utiliza el mes y año para hacer la búsqueda, tenía sentido validar que ambos fueran ingresados, pero eso no lo hice.


Pero sí agregué una linea que dice que si fue ingresado un año, este debe validar que esté entre el rango de años posibles segun el dataset.