# Proyecto

Trabajo Especial de la materia Protocolos de Comunicación. El objetivo del trabajo es implementar un servidor proxy para el protocolo POP3.


## Para Comenzar

Dentro de la carpeta del proyecto se pobran ver la carpeta src, que contiene los archivos del trabajo. Tambien contiene la carpeta de los tests y los services, entre otras.

### 

### Instalacion

Nos posicionamos dentro de la carpeta del trabajo practico e ingresamos a la carpeta src

```
cd src
```

Realizamos el make clean
```
make clean
```
Luego el make all
```
make all
```
Para probar el proxy, este tiene la opcion -h, que despliega un menu de ayuda
```
./proxy -h
```


## Tests

Se realizaron diferentes tests para comprobar el comportamiento del pop3 parser, del pop3 command parser, el singleline parser, el multiline parser y las transformaciones. Para correrlos hay que estar posicionado en la carpeta src.

Para correr el singleline parser test
```
./pop3_singleline_response_parser 
```
Para correr el multiline parser test
```
./pop3_multiline_response_parser 
```
Para correr el pop3 parser test
```
./pop3__parser 
```
Para correr el pop3 command test
```
./pop3_command_parser 
```
### 


## Referencias
Stack.c: 

## Authors

- **Paula Oseroff**- Legajo: 58415
- **Manuel Luque**- Legajo: 57386
- **Gaston Lifschitz**- Legajo: 58225
- **Micaela Banfi**- Legajo: 57293



