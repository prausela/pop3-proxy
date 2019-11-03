#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>

#include "parser_utils.h"
#include "pop3_multi.h"
#include "mime_chars.h"
#include "mime_msg.h"
#include "content_type_parser.h"
#include "stripmime.h"
static char * filter_msg = "text/plain";


/*
 * imprime información de debuging sobre un evento.
 *
 * @param p        prefijo (8 caracteres)
 * @param namefnc  obtiene el nombre de un tipo de evento
 * @param e        evento que se quiere imprimir
 */
static void
debug(const char *p,
      const char * (*namefnc)(unsigned),
      const struct parser_event* e) {
    // DEBUG: imprime
    if (e->n == 0) {
        fprintf(stderr, "%-8s: %-14s\n", p, namefnc(e->type));
    } else {
        for (int j = 0; j < e->n; j++) {
            const char* name = (j == 0) ? namefnc(e->type)
                                        : "";
            if (e->data[j] <= ' ') {
                fprintf(stderr, "%-8s: %-14s 0x%02X\n", p, name, e->data[j]);
            } else {
                fprintf(stderr, "%-8s: %-14s %c\n", p, name, e->data[j]);
            }
        }
    }
}

/* mantiene el estado durante el parseo */
struct ctx {
    /* delimitador respuesta multi-línea POP3 */
    struct parser*          multi;
    /* delimitador mensaje "tipo-rfc 822" */
    struct parser*          msg;
    struct parser*          content_type;
    /* detector de field-name "Content-Type" */
    struct parser*          ctype_header;
    struct parser*          ctype_value;

    /* ¿hemos detectado si el field-name que estamos procesando refiere
     * a Content-Type?. Utilizando dentro msg para los field-name.
     */
    bool                    *msg_content_type_field_detected;
    bool                    *media_type_filter_detected;
    bool                    *msg_content_filter;
    bool                    *media_filter_apply;
    uint8_t                 *process_modification_mail;

    struct type_list*      media_types;
    struct subtype_list*   media_subtypes;
};

static bool T = true;
static bool F = false;
void 
printctx(struct ctx *ctx);
/* Detecta si un header-field-name equivale a Content-Type.
 * Deja el valor en `ctx->msg_content_type_field_detected'. Tres valores
 * posibles: NULL (no tenemos información suficiente todavia, por ejemplo
 * viene diciendo Conten), true si matchea, false si no matchea.
 */
static void
content_type_header(struct ctx *ctx, const uint8_t c) {
    const struct parser_event* e = parser_feed(ctx->ctype_header, c);
    do {
        debug("2.typehr", parser_utils_strcmpi_event, e);
        switch(e->type) {
            case STRING_CMP_EQ:
                ctx->msg_content_type_field_detected = &T;
                break;
            case STRING_CMP_NEQ:
                ctx->msg_content_type_field_detected = &F;
                break;
        }
        
        e = e->next;
    } while (e != NULL);
}
static void
content_type_value(struct ctx *ctx, const uint8_t c) {
    const struct parser_event* e = parser_feed(ctx->ctype_value, c);
    do {
        debug("3.typehr", parser_utils_strcmpi_event, e);
        switch(e->type) {
            case STRING_CMP_EQ:
                printf("Hola");
                break;
            case STRING_CMP_NEQ:
                printf("Chau");
                break;
        }
        e = e->next;
    } while (e != NULL);
}
/*
Los tipos a filtrar seran guardaos en una lista de nodos. 
Cada nodo tiene el tipo, si es asterisoc y una lista de subtipos y un next al siguiente tipo.
En el contexto vamos a guardr la lista de tipos y un puntero al nodo del subtipo que sera definido por el tipo
Despues de leer cada tipo y subtipo genero un parser que me tome el tipo. por ejemplo un parser para "text", otro para "image" y lo mismo con los subitpos
Estaria bueno mantener en cada nodo el estado si matcheo o no.
Guardar el parser de cada tipo/subtipo en el nodo correspondiente.
Una vez que encontre el tipo, guardar la lista de subtipos en el context del que el estado es equal's.
Recorrer la lista de subtipos para guardar el subtipo verdadero apra despues hacer el parser event.
*/
static void
content_type_msg(struct ctx *ctx, const uint8_t c) {
    const struct parser_event* e = parser_feed(ctx->content_type,c);

    do {
        switch (e->type){
            case MIME_TYPE_TYPE:
            case MIME_TYPE_TYPE_END:
            case MIME_TYPE_SUBTYPE:
            case MIME_PARAMETER_START:
            case MIME_PARAMETER:
            case MIME_BOUNDARY_END:
            case MIME_DELIMITER_START:
            case MIME_DELIMITER:
            case MIME_DELIMITER_END:
            case MIME_TYPE_UNEXPECTED:
                break;
            default:
                break;
            (ctx->process_modification_mail)[0]=e->data[0];
            (ctx->process_modification_mail)++;
        }
    } while (e != NULL);
    
}

/**
 * Procesa un mensaje `tipo-rfc822'.
 * Si reconoce un al field-header-name Content-Type lo interpreta.
 *
 */
static void
mime_msg(struct ctx *ctx, const uint8_t c) {
    const struct parser_event* e = parser_feed(ctx->msg, c);
    do {
        debug("1.   msg", mime_msg_event, e);
        int aux;
        printf("Current letter parser feed: %c\n",e->data[0]);
        printctx(ctx);
        switch(e->type) {
            case MIME_MSG_NAME:
                if( ctx->msg_content_type_field_detected == 0
                || *ctx->msg_content_type_field_detected) {
                    for(int i = 0; i < e->n; i++) {
                        content_type_header(ctx, e->data[i]);
                        (ctx->process_modification_mail)[0]=e->data[0];
                        (ctx->process_modification_mail)++;
                    }
                }else{
                    (ctx->process_modification_mail)[0]=e->data[0];
                        (ctx->process_modification_mail)++;
                }
                
               
                break;
            case MIME_MSG_NAME_END:
                // lo dejamos listo para el próximo header
                parser_reset(ctx->ctype_header);
                parser_reset(ctx->ctype_value);
                (ctx->process_modification_mail)[0]=e->data[0];
                (ctx->process_modification_mail)++;
                break;
            case MIME_MSG_VALUE:
                if(ctx->msg_content_type_field_detected != 0
                && ctx->msg_content_type_field_detected == &T) {
                    for(int i = 0; i < e->n; i++) {
                        content_type_msg(ctx, e->data[i]);
                    }
                }
                (ctx->process_modification_mail)[0]=e->data[0];
                (ctx->process_modification_mail)++;
                break;
            case MIME_MSG_VALUE_END:
                // si parseabamos Content-Type ya terminamos
                ctx->msg_content_type_field_detected = 0;
                // (ctx->process_modification_mail)[0]=e->data[0];
                // (ctx->process_modification_mail)++;
                break;
            case MIME_MSG_BODY:
                printf("WUUUU ENTRE");
                break;

            case MIME_MSG_BODY_CR:
                break;

            case MIME_MSG_BODY_NEWLINE:
                break;
            default:
                (ctx->process_modification_mail)[0]=e->data[0];
                (ctx->process_modification_mail)++;
                break;
        }
        e = e->next;
    } while (e != NULL);
}

/* Delimita una respuesta multi-línea POP3. Se encarga del "byte-stuffing" */
static void
pop3_multi(struct ctx *ctx, const uint8_t c) {
    const struct parser_event* e = parser_feed(ctx->multi, c);
    do {
        debug("0. multi", pop3_multi_event,  e);
        switch (e->type) {
            case POP3_MULTI_BYTE:
                for(int i = 0; i < e->n; i++) {
                    mime_msg(ctx, e->data[i]);
                }
                break;
            case POP3_MULTI_WAIT:
                //(ctx->process_modification_mail)[0]=e->data[0];
                //(ctx->process_modification_mail)++;
                // nada para hacer mas que esperar
                break;
            case POP3_MULTI_FIN:
                // arrancamos de vuelta
                //en current point letter me quedo apuntado el final del body
                //ahi tengo que poner el mensaje de reemplazo

                parser_reset(ctx->msg);
                ctx->msg_content_type_field_detected = NULL;
                (ctx->process_modification_mail)[0]=0;

                break;
        }
        e = e->next;
        getchar();
    } while (e != NULL);
}

void 
printctx(struct ctx *ctx){
    if(ctx->msg_content_type_field_detected != 0){
        if(ctx->msg_content_type_field_detected == &T){
            printf("message CT detected TRUE\n");
        }
        else{
            printf("message CT detected FALSE\n");
        }
    }
    else{
        printf("message CT detected NULL\n");
    }          
    printf("data= %p\n",ctx->process_modification_mail);
        printf("current point letter: %c\n",ctx->process_modification_mail[0]);
}

int
main(const int argc, const char **argv) {
    int fd = STDIN_FILENO;
    if(argc > 1) {
        fd = open(argv[1], 0);
        if(fd == -1) {
            perror("opening file");
            return 1;
        }
    }

    const unsigned int* no_class = parser_no_classes();
    struct parser_definition media_header_def =
            parser_utils_strcmpi("content-type");
    
    struct parser_definition media_value_def =
            parser_utils_strcmpi("text/plain");

    struct type_list* media_types = malloc(sizeof(*media_types));
    struct subtype_list* media_subtypes = malloc(sizeof(*media_subtypes));
    
    media_types->type="text";
    media_types->is_wildcard=&F;
    media_types->next=NULL;
    //MAL: Hay que pasarle un parser utils strcmpi!!!!
    media_types->type_parser = parser_init(no_class, mime_type_parser());
    media_types->subtypes = media_subtypes;
    media_subtypes->next=NULL;
    media_subtypes->type="plain";
    //COMPLETAR

    struct ctx ctx = {
        .multi                      = parser_init(no_class, pop3_multi_parser()),
        .msg                        = parser_init(init_char_class(), mime_message_parser()),
        .content_type               = parser_init(no_class, mime_type_parser()),
        .ctype_header               = parser_init(no_class, &media_header_def),
        .ctype_value                = parser_init(no_class, &media_value_def), 
        .msg_content_filter         = &F, 
        .media_type_filter_detected = &F,
        .media_filter_apply         = &F,
    };

    uint8_t data[4096];
    int n;
    do {
        n = read(fd, data, sizeof(data));
        ctx.process_modification_mail=data;
        for(ssize_t i = 0; i < n ; i++) {
            pop3_multi(&ctx, data[i]);
        }
    } while(n > 0);
    int i=0;

    while(data[i]!=0){
        printf("%c",data[i++]);

    }

    parser_destroy(ctx.multi);
    parser_destroy(ctx.msg);
    parser_destroy(ctx.ctype_header);
    parser_destroy(ctx.ctype_value);
    parser_utils_strcmpi_destroy(&media_header_def);
    parser_utils_strcmpi_destroy(&media_value_def);
}