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

## RFCs ##

- 2449
```
   Initially, the server host starts the POP3 service by listening on
   TCP port 110.  When a client host wishes to make use of the service,
   it establishes a TCP connection with the server host.  When the
   connection is established, the POP3 server sends a greeting.  The
   client and POP3 server then exchange commands and responses
   (respectively) until the connection is closed or aborted.
```
Primero se recibe un ```greeting```
```
Commands in the POP3 consist of a case-insensitive keyword, possibly
   followed by one or more arguments.  All commands are terminated by a
   CRLF pair.  Keywords and arguments consist of printable ASCII
   characters.  Keywords and arguments are each separated by a single
   SPACE character.  Keywords are three or four characters long. Each
   argument may be up to 40 characters long.
```
Command = keyword + ' ' + argument[0] + ' ' + argument[1]\r\n
**ASCII & case-insensitive**
```
Responses in the POP3 consist of a status indicator and a keyword
   possibly followed by additional information.  All responses are
   terminated by a CRLF pair.  Responses may be up to 512 characters
   long, including the terminating CRLF.  There are currently two status
   indicators: positive ("+OK") and negative ("-ERR").  Servers MUST
   send the "+OK" and "-ERR" in upper case.
```
status = "+OK" | "-ERR" \r\n
```
Responses to certain commands are multi-line.  In these cases, which
   are clearly indicated below, after sending the first line of the
   response and a CRLF, any additional lines are sent, each terminated
   by a CRLF pair.  When all lines of the response have been sent, a
   final line is sent, consisting of a termination octet (decimal code
   046, ".") and a CRLF pair.  If any line of the multi-line response
   begins with the termination octet, the line is "byte-stuffed" by
   pre-pending the termination octet to that line of the response.
   Hence a multi-line response is terminated with the five octets
   "CRLF.CRLF".  When examining a multi-line response, the client checks
   to see if the line begins with the termination octet.  If so and if
   octets other than CRLF follow, the first octet of the line (the
   termination octet) is stripped away.  If so and if CRLF immediately
   follows the termination character, then the response from the POP
   server is ended and the line containing ".CRLF" is not considered
   part of the multi-line response.