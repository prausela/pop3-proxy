# Back to the AOL #

## Convenciones que uso para Programar ##
Yo suelo usar un par de convenciones cuando programo, las quiero poner aca asi estamos todos en la misma pagina. Si normalmente usan alguna otra convencion, avisenme para que no les haga la vida imposible con cambiar todos los nombres de funciones y etc.

- Structs tienen la forma ```struct mystruct``` y si se le aplica un typedef el alias es ```mystruct_t```.
- Los typedef de punteros a estructuras los escribo con CammelCase. Ejemplo: ```typedef mystruct_t *MyStruct```.
- Las funciones de ADTs las escribo con cammelCase.
- Variables y funciones en todo otro caso las escribo en snake_case.
- Aquellas variables que sean punteros, su nombre termina en \_ptr. Ejemplo: ```int * read_ptr```.

## Cosas Utiles ##

- La directiva ```inline``` hace que cuando se llame a la funcion, en vez de hacer un call a la misma, el compilador coloque el codigo donde iria la llamada (como si fuera una macro). Aumenta la velocidad de ejecucion pero tiene un costo sobre el peso del programa.

### Paginas Web ###

- http://www.networksorcery.com/enp/protocol/pop.htm