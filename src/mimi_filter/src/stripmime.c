#include "stripmime.h"
#include <string.h>

/*
 * imprime información de debuging sobre un evento.
 *
 * @param p        prefijo (8 caracteres)
 * @param namefnc  obtiene el nombre de un tipo de evento
 * @param e        evento que se quiere imprimir
 */
static void
debug(const char *p,
      const char *(*namefnc)(unsigned),
      const struct parser_event *e)
{
    // DEBUG: imprime
    if (e->n == 0)
    {
        fprintf(stderr, "%-8s: %-14s\n", p, namefnc(e->type));
    }
    else
    {
        for (int j = 0; j < e->n; j++)
        {
            const char *name = (j == 0) ? namefnc(e->type)
                                        : "";
            if (e->data[j] <= ' ')
            {
                fprintf(stderr, "%-8s: %-14s 0x%02X\n", p, name, e->data[j]);
            }
            else
            {
                fprintf(stderr, "%-8s: %-14s %c\n", p, name, e->data[j]);
            }
        }
    }
}

/* mantiene el estado durante el parseo */
struct ctx
{
    /* delimitador respuesta multi-línea POP3 */
    struct parser *multi;
    /* delimitador mensaje "tipo-rfc 822" */
    struct parser *msg;
    struct parser *content_type;
    /* detector de field-name "Content-Type" */
    struct parser *ctype_header;
    /* detector de field-name "Content-Transfer_Encoding"  */
    struct parser *ctransfer_encoding_header;
    struct parser *ctype_value;

    //parsea la palabra boundary en el header
    struct parser *boundary;

    struct parser *boundary_parser_detector;
    struct parser *charset_parser_detector;

    //lista que guarda una linea del body que puede ser bundary
    list *possible_boundary_string;

    //guarda un boundary name visto en el header para pushearlo al stack
    list *boundary_name;

    //stack que guarda el boundry name encontrado en el header
    stack *boundry_stack;

    /* ¿hemos detectado si el field-name que estamos procesando refiere
     * a Content-Type?. Utilizando dentro msg para los field-name.
     */
    bool *msg_content_type_field_detected;
    bool *media_type_filter_detected;
    bool *msg_content_filter;
    //borrar ese body?
    bool *media_filter_apply;
    //es multipart? (busco o no boundary)
    bool *multipart_section;
    bool *msg_content_transfer_encoding_field_detected;
    uint8_t *process_modification_mail;

    //se declaro un boundry en el header?
    bool *boundary_detected;

    //el ultimo caracter leido en el body es un -?
    bool *is_middle_dash;

    //los ultimos dos leidos son --? TRUE=estoy leyendo boundary name
    bool *double_middle_dash;

    //esa linea puede ser un boundry? Si no arranca con - la linea, lo pongo en false
    bool *possible_boundary;

    //encontre -- al final de un boundary
    bool *boundary_end;

    struct type_list *media_types;
    struct subtype_list *media_subtypes;
};

static bool T = true;
static bool F = false;
void printctx(struct ctx *ctx);
/* Detecta si un header-field-name equivale a Content-Type.
 * Deja el valor en `ctx->msg_content_type_field_detected'. Tres valores
 * posibles: NULL (no tenemos información suficiente todavia, por ejemplo
 * viene diciendo Conten), true si matchea, false si no matchea.
 */

static void
content_transfer_encoding_header(struct ctx *ctx, const uint8_t c)
{
    const struct parser_event *e = parser_feed(ctx->ctransfer_encoding_header, c);
    do
    {
        debug("6.typehr", parser_utils_strcmpi_event, e);
        switch (e->type)
        {
        case STRING_CMP_EQ:
            ctx->msg_content_transfer_encoding_field_detected = &T;
            fprintf(stderr, "UYYAAA");
            break;
        case STRING_CMP_NEQ:
            ctx->msg_content_transfer_encoding_field_detected = &F;
            break;
        }

        e = e->next;
    } while (e != NULL);
}

static void
content_type_header(struct ctx *ctx, const uint8_t c)
{
    const struct parser_event *e = parser_feed(ctx->ctype_header, c);
    do
    {
        debug("2.typehr", parser_utils_strcmpi_event, e);
        switch (e->type)
        {
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
content_type_value(struct ctx *ctx, const uint8_t c)
{
    const struct parser_event *e = parser_feed(ctx->ctype_value, c);
    do
    {
        debug("3.typehr", parser_utils_strcmpi_event, e);
        switch (e->type)
        {
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

static void
boundary_charset_match(struct ctx *ctx, const uint8_t c)
{
    const struct parser_event *boundary_parser_evt = parser_feed(ctx->boundary_parser_detector, c);
    const struct parser_event *charset_parser_evt = parser_feed(ctx->charset_parser_detector, c);
    //Imprimir resultados
    if (boundary_parser_evt->type == STRING_CMP_EQ)
    {
        fprintf(stderr, "LA CONCHA DE LA LORAAAA");
        ctx->multipart_section = &T;
    }
    else if (charset_parser_evt->type == STRING_CMP_EQ)
    {
        fprintf(stderr, "LA CONCHA DE TU MADREEE");
        ctx->multipart_section = &F;
    }
    else
    {
        //Todavia no se...
        ctx->multipart_section = NULL;
    }
}

static void
replace_to_plain_text(struct ctx *ctx, const uint8_t c)
{
    fprintf(stderr, "before %c and %c", c, ctx->process_modification_mail[0]);
    while (ctx->process_modification_mail[0] != ' ' && ctx->process_modification_mail[0] != ':')
    {
        (ctx->process_modification_mail)--;
    }
    (ctx->process_modification_mail)++;
    ctx->process_modification_mail[0] = 't';
    (ctx->process_modification_mail)++;
    ctx->process_modification_mail[0] = 'e';
    (ctx->process_modification_mail)++;
    ctx->process_modification_mail[0] = 'x';
    (ctx->process_modification_mail)++;
    ctx->process_modification_mail[0] = 't';
    (ctx->process_modification_mail)++;
    ctx->process_modification_mail[0] = '/';
    (ctx->process_modification_mail)++;
    ctx->process_modification_mail[0] = 'p';
    (ctx->process_modification_mail)++;
    ctx->process_modification_mail[0] = 'l';
    (ctx->process_modification_mail)++;
    ctx->process_modification_mail[0] = 'a';
    (ctx->process_modification_mail)++;
    ctx->process_modification_mail[0] = 'i';
    (ctx->process_modification_mail)++;
    ctx->process_modification_mail[0] = 'n';
    (ctx->process_modification_mail)++;
    fprintf(stderr, "LALAAAA %c y %c\n", c, ctx->process_modification_mail[0]);
}

static void
content_subtype_match(struct ctx *ctx, const uint8_t c)
{
    struct subtype_list *subtype_list = ctx->media_subtypes;
    struct parser_event *current;
    if (subtype_list->is_wildcard != 0 && *subtype_list->is_wildcard)
    {
        current = malloc(sizeof(*current));
        current->type = STRING_CMP_EQ;
        current->data[0] = c;
        current->n = 1;
    }
    else
    {
        subtype_list->event = parser_feed(subtype_list->type_parser, c);
        current = subtype_list->event;

        while (subtype_list->next != NULL)
        {
            subtype_list = subtype_list->next;
            subtype_list->event = parser_feed(subtype_list->type_parser, c);

            if (subtype_list->event->type == STRING_CMP_EQ)
            {
                current = subtype_list->event;
            }
        }
    }
    if (current->type == STRING_CMP_EQ)
    {
        //Match found!
        ctx->media_filter_apply = &T;
        fprintf(stderr, "Encontre un match!\n");
        //Go back and replace whatever to text/plain
        //                    replace_to_plain_text(ctx,c);
    }
    else
    {
        //Overwriting just in case
        ctx->media_filter_apply = &F;
    }
}

static void
content_type_match(struct ctx *ctx, const uint8_t c)
{
    struct type_list *type_list = ctx->media_types;
    if (type_list == NULL)
    {
        //TODO: Error handling!!
        return;
    }

    /* Leave in current*/
    struct parser_event *current;
    type_list->event = parser_feed(type_list->type_parser, c);
    current = type_list->event;

    while (type_list->next != NULL)
    {
        type_list = type_list->next;
        type_list->event = parser_feed(type_list->type_parser, c);
        if (type_list->event->type == STRING_CMP_EQ)
        {
            current = type_list->event;
        }
    }
    if (current->type == STRING_CMP_EQ)
    {
        //Match found!
        fprintf(stderr, "WUUUU");
        ctx->media_type_filter_detected = &T;
        ctx->media_subtypes = type_list->subtypes;
    }
    else
    {
        //Overwriting just in case
        ctx->media_type_filter_detected = &F;
    }
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
content_type_msg(struct ctx *ctx, const uint8_t c)
{
    const struct parser_event *e = parser_feed(ctx->content_type, c);
    debug("5.   ct_type", content_type_msg_event, e);
    fprintf(stderr, "Current letter parser feed: %c\n", e->data[0]);
    do
    {
        switch (e->type)
        {
        case MIME_TYPE_TYPE:
            for (int i = 0; i < e->n; i++)
            {
                content_type_match(ctx, e->data[i]);
            }
            break;
        case MIME_TYPE_TYPE_END:
            //Aca encontre el /
            if (!(*ctx->media_type_filter_detected))
            {
                //Nothing to be done here! HELP
            }
            break;
        case MIME_TYPE_SUBTYPE:
            if (*ctx->media_type_filter_detected)
            {
                for (int i = 0; i < e->n; i++)
                {
                    content_subtype_match(ctx, e->data[0]);
                }
            }
            break;
        case MIME_PARAMETER_START:
            //Should i do smth here?
            if (*ctx->media_filter_apply)
            {
                //fprintf(stderr,"char in buffer= %c",(ctx->process_modification_mail)[0]);
            }
            break;
        case MIME_PARAMETER:
            //Look for Boundary/charset
            for (int i = 0; i < e->n; i++)
            {
                boundary_charset_match(ctx, e->data[0]);
            }
        case MIME_BOUNDARY_END:
            //Not much to do in here!
            break;
        case MIME_DELIMITER_START:
            break;
        case MIME_DELIMITER:
            if (ctx->multipart_section != NULL)
            {
                if (*ctx->multipart_section)
                {
                    //encontre un boundary, lo pusheo al stack!
                    list_append(ctx->boundary_name, e->data[0]);
                }
                else
                {
                    //Estoy en una seccion normal!
                    if (*ctx->media_filter_apply)
                    {
                        //TODO: Cambiar el charset a utf-8
                    }
                    else
                    {
                        //NADA (transparente)
                    }
                }
            }
            break;
        case MIME_DELIMITER_END:
            //printf("list return string %s\n",list_return_string(ctx->boundary_name));
            stack_push(ctx->boundry_stack, list_return_string(ctx->boundary_name));
            printf("STACK PEEK: %s\n", stack_peek(ctx->boundry_stack));
            ctx->boundary_name=list_empty(ctx->boundary_name);
            if (ctx->multipart_section != NULL && *ctx->media_filter_apply && !*ctx->multipart_section)
            {
            }
            break;
        case MIME_TYPE_UNEXPECTED:
            break;
        default:
            break;
            if (e->type == MIME_DELIMITER_END && ctx->multipart_section != NULL && *ctx->media_filter_apply && !*ctx->multipart_section)
            {
            }
            else
            {
            }
            if (!*ctx->media_filter_apply)
            {
                (ctx->process_modification_mail)[0] = e->data[0];
                (ctx->process_modification_mail)++;
            }
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

/**
 * Procesa un mensaje `tipo-rfc822'.
 * Si reconoce un al field-header-name Content-Type lo interpreta.
 *
 */
static void
mime_msg(struct ctx *ctx, const uint8_t c)
{
    const struct parser_event *e = parser_feed(ctx->msg, c);
    
    do
    {
        debug("1.   msg", mime_msg_event, e);
        printf("Current letter parser feed: %c\n", e->data[0]);
        printctx(ctx);
        switch (e->type)
        {
        case MIME_MSG_NAME:
            if (ctx->msg_content_type_field_detected == 0 || *ctx->msg_content_type_field_detected)
            {
                for (int i = 0; i < e->n; i++)
                {
                    content_type_header(ctx, e->data[i]);
                }
            }
            if (ctx->msg_content_transfer_encoding_field_detected == 0 || *ctx->msg_content_transfer_encoding_field_detected)
            {
                for (int i = 0; i < e->n; i++)
                {
                    content_transfer_encoding_header(ctx, e->data[i]);
                }
            }
            (ctx->process_modification_mail)[0] = e->data[0];
            (ctx->process_modification_mail)++;
            break;
        case MIME_MSG_NAME_END:
            // lo dejamos listo para el próximo header
            parser_reset(ctx->ctype_header);
            parser_reset(ctx->ctype_value);
            parser_reset(ctx->ctransfer_encoding_header);
            (ctx->process_modification_mail)[0] = e->data[0];
            (ctx->process_modification_mail)++;
            break;
        case MIME_MSG_VALUE:
            if (ctx->msg_content_type_field_detected != 0 && ctx->msg_content_type_field_detected == &T && !*ctx->media_filter_apply)
            {
                for (int i = 0; i < e->n; i++)
                {
                    content_type_msg(ctx, e->data[i]);
                }
            }
            (ctx->process_modification_mail)[0] = e->data[0];
            (ctx->process_modification_mail)++;
            break;
        case MIME_MSG_VALUE_END:
            // si parseabamos Content-Type ya terminamos
            ctx->msg_content_type_field_detected = 0;
            ctx->msg_content_transfer_encoding_field_detected = 0;
            // (ctx->process_modification_mail)[0]=e->data[0];
            // (ctx->process_modification_mail)++;
            break;
        case MIME_MSG_BODY:
            if (!*ctx->media_filter_apply)
            {
                (ctx->process_modification_mail)[0] = e->data[0];
                (ctx->process_modification_mail)++;
            }
            else
            {
                if (ctx->boundary_detected != 0 && ctx->boundary_detected == &T)
                {
                    if (e->data[0] == '-')
                    {
                        printf("LEO GUION\n");
                        //tengo que setear el possible boundry cuando arranca la linea
                        ctx->possible_boundary = &T;
                        if (ctx->possible_boundary != 0 && ctx->possible_boundary == &T)
                        {
                            if (ctx->is_middle_dash != 0 && ctx->is_middle_dash == &T)
                            {
                                //consegui dos guiones juntos, guardo todo lo que
                                //sigue (hasta el new line) para compararlo con el ultimo del stack,
                                ctx->double_middle_dash = &T;
                            }
                        }
                    }
                    //estoy leyendo un boundary name?
                    if (ctx->double_middle_dash != 0 && ctx->double_middle_dash == &T)
                    {
                        //si leo un - y es el primero, no lo pongo en el string
                        if (!(e->data[0] == '-' && list_size(ctx->possible_boundary_string) == 0))
                        {
                            list_append(ctx->possible_boundary_string, e->data[0]);
                        }
                        //si encuentro un -- al final, prendo bool que encontre el boundary name end
                        if (e->data[0] == '-' && list_size(ctx->possible_boundary_string) != 0)
                        {
                            (ctx->process_modification_mail)--;
                            if (ctx->process_modification_mail[0] == '-')
                            {
                                //encontre dos -- al final
                                ctx->boundary_end = &T;
                            }
                            (ctx->process_modification_mail)++;
                        }
                    }
                }

                if (e->data[0] == '-')
                {
                    ctx->is_middle_dash = &T;
                }
                else
                {
                    ctx->is_middle_dash = &F;
                }
            }
            break;

        case MIME_MSG_BODY_CR:
            if (!*ctx->media_filter_apply)
            {
                (ctx->process_modification_mail)[0] = e->data[0];
                (ctx->process_modification_mail)++;
            }
            break;

        case MIME_MSG_BODY_NEWLINE:
            if (!*ctx->media_filter_apply)
            {
                (ctx->process_modification_mail)[0] = e->data[0];
                (ctx->process_modification_mail)++;
            }
            else
            {
                if (ctx->double_middle_dash != 0 && ctx->double_middle_dash == &T)
                {   printf("possible_boundary_string: %s\n",list_return_string(ctx->possible_boundary_string));
                    printf("stack peek: %s\n",stack_peek(ctx->boundry_stack));


                    if (strcmp(list_return_string(ctx->possible_boundary_string), stack_peek(ctx->boundry_stack)) == 0)
                    {
                        printf("ENTROOO \n");
                        //el possible boundry string coincidio con el del stack
                        //lo agrego al body
                        while (list_size(ctx->possible_boundary_string) >= 1)
                        {

                            (ctx->process_modification_mail)[0] = list_head(ctx->possible_boundary_string, false);
                            list_head(ctx->possible_boundary_string, true);
                            (ctx->process_modification_mail)++;
                        }
                        ctx->possible_boundary_string = list_empty(ctx->possible_boundary_string);
                    }
                }
                if (ctx->boundary_end != 0 && ctx->boundary_end == &T)
                {
                    if (strcmp(list_return_string(ctx->possible_boundary_string), stack_peek(ctx->boundry_stack)) == 0)
                    {
                        printf("encontro el fin del boundary\n");
                        stack_pop(ctx->boundry_stack);
                        ctx->possible_boundary_string = list_empty(ctx->possible_boundary_string);
                    }
                }

                ctx->is_middle_dash = &F;
                ctx->double_middle_dash = &F;
                ctx->possible_boundary = &F;
            }
            break;
        default:
            (ctx->process_modification_mail)[0] = e->data[0];
            (ctx->process_modification_mail)++;
            break;
        }
        e = e->next;
    } while (e != NULL);
}

/* Delimita una respuesta multi-línea POP3. Se encarga del "byte-stuffing" */
static void
pop3_multi(struct ctx *ctx, const uint8_t c)
{
    const struct parser_event *e = parser_feed(ctx->multi, c);
    do
    {
        debug("0. multi", pop3_multi_event, e);
        switch (e->type)
        {
        case POP3_MULTI_BYTE:
            for (int i = 0; i < e->n; i++)
            {
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

            parser_reset(ctx->msg);
            parser_reset(ctx->content_type);
            ctx->msg_content_type_field_detected = NULL;
            ctx->msg_content_transfer_encoding_field_detected = NULL;
            (ctx->process_modification_mail)[0] = 0;

            break;
        }
        e = e->next;
        getchar();
    } while (e != NULL);
}

void printctx(struct ctx *ctx)
{
    if (ctx->msg_content_type_field_detected != 0)
    {
        if (ctx->msg_content_type_field_detected == &T)
        {
            printf("message CT detected TRUE\n");
        }
        else
        {
            printf("message CT detected FALSE\n");
        }
    }
    else
    {
        printf("message CT detected NULL\n");
    }
    printf("data= %p\n", ctx->process_modification_mail);
    printf("current point letter: %c\n", ctx->process_modification_mail[0]);
    if(ctx->media_filter_apply!=0 && ctx->media_filter_apply==&T){
        printf("media filter: TRUE\n");
    }
    else{
        printf("media filter: FALSE\n");
    }
    if(ctx->double_middle_dash!=0 && ctx->double_middle_dash==&T){
        printf("Double middle dash: TRUE\n");
    }
    else{
        printf("Double middle dash: FALSE\n");
    }
    if(ctx->is_middle_dash!=0 && ctx->is_middle_dash==&T){
        printf("middle dash: TRUE\n");
    }
    else{
        printf("middle dash: FALSE\n");
    }
    if(ctx->boundary_detected!=0 && ctx->boundary_detected==&T){
        printf("boundary detected: TRUE\n");
    }
    else{
        printf("boundary detected: FALSE\n");
    }
}

int main(const int argc, const char **argv)
{
    int fd = STDIN_FILENO;
    if (argc > 1)
    {
        fd = open(argv[1], 0);
        if (fd == -1)
        {
            perror("opening file");
            return 1;
        }
    }

    const unsigned int *no_class = parser_no_classes();
    struct parser_definition media_header_def =
        parser_utils_strcmpi("content-type");
    struct parser_definition content_transfer_encoding_header_def =
        parser_utils_strcmpi("content-transfer-encoding");
    struct parser_definition media_value_def =
        parser_utils_strcmpi("image/png");

    struct type_list *media_types = malloc(sizeof(*media_types));
    struct subtype_list *media_subtypes = malloc(sizeof(*media_subtypes));

    struct parser_definition type1 = parser_utils_strcmpi("image");
    struct parser_definition subtype1 = parser_utils_strcmpi("png");
    struct parser_definition boundary_parser = parser_utils_strcmpi("boundary");
    struct parser_definition charset_parser = parser_utils_strcmpi("charset");

    media_types->next = NULL;
    media_types->type_parser = parser_init(no_class, &type1);
    media_types->subtypes = media_subtypes;
    media_subtypes->next = NULL;
    media_subtypes->is_wildcard = &F;
    media_subtypes->type_parser = parser_init(no_class, &subtype1);

    struct ctx ctx = {
        .multi = parser_init(no_class, pop3_multi_parser()),
        .msg = parser_init(init_char_class(), mime_message_parser()),
        .content_type = parser_init(init_char_class(), mime_type_parser()),
        .ctype_header = parser_init(no_class, &media_header_def),
        .ctype_value = parser_init(no_class, &media_value_def),
        .ctransfer_encoding_header = parser_init(no_class, &content_transfer_encoding_header_def),
        .msg_content_filter = &F,
        .media_type_filter_detected = &F,
        .media_filter_apply = &F,
        .media_types = media_types,
        .media_subtypes = NULL,
        .multipart_section = NULL,
        .boundary_detected = &F,
        .possible_boundary_string = list_new(sizeof(uint8_t), NULL),
        .double_middle_dash = &F,
        .boundary_parser_detector = parser_init(no_class, &boundary_parser),
        .charset_parser_detector = parser_init(no_class, &charset_parser),
        .msg_content_transfer_encoding_field_detected = NULL,
        .boundry_stack = stack_new(NULL),
        .boundary_name = list_new(sizeof(uint8_t), NULL),
        .boundary_end = &F,
    };
    uint8_t data[4096], transformed[4096];
    memset(transformed, '\0', sizeof(transformed));
    int n;
    do
    {
        n = read(fd, data, sizeof(data)); //Read from pipe instead of file!
        ctx.process_modification_mail = transformed;
        for (ssize_t i = 0; i < n; i++)
        {
            pop3_multi(&ctx, data[i]);
        }
        int i = 0;
        while (transformed[i] != 0)
        {
            printf("%c", transformed[i++]);
        }
        memset(transformed, '\0', sizeof(transformed));
        //write to pipe!!
        //empty buffer
    } while (n > 0);

    parser_destroy(ctx.multi);
    parser_destroy(ctx.msg);
    parser_destroy(ctx.ctype_header);
    parser_destroy(ctx.ctype_value);
    parser_utils_strcmpi_destroy(&media_header_def);
    parser_utils_strcmpi_destroy(&media_value_def);
}