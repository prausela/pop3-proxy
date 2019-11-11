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
        fprintf(stderr, "%-14s\n", namefnc(e->type));
    }
    else
    {
        for (int j = 0; j < e->n; j++)
        {
            const char *name = (j == 0) ? namefnc(e->type)
                                        : "";
            if (e->data[j] <= ' ')
            {
                fprintf(stderr, "0x%02X", e->data[j]);
            }
            else
            {
                fprintf(stderr, "%c",e->data[j]);
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
    /* delimitador para atributos del content-type y sus tipos  */
    struct parser *content_type;
    /* detector de field-name "Content-Type" */
    struct parser *ctype_header;
    /* detector de field-name "Content-Transfer_Encoding"  */
    struct parser *ctransfer_encoding_header;
    //Not used atm...
    struct parser *ctype_value;
    /* detector de message type en header*/
    struct parser *message_type;

    /* detector de rfc822*/
    struct parser *rfc822;

    /* delimitador para atributo boundary */
    struct parser *boundary_parser_detector;
  

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
    bool *replace_content_type;
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

    //detecta un message en el header
    bool *message_detected;
    //detecta un rfc822 en el header
    bool *rfc822_detected;

    struct type_list *media_types;
    struct subtype_list *media_subtypes;
};

static bool T = true;
static bool F = false;
void printctx(struct ctx *ctx);

static void
insert_text(struct ctx *ctx, char * text){
    int i = 0;
    while(text[i] != 0){
        ctx->process_modification_mail[0] = text[i];
        (ctx->process_modification_mail)++;
        i++;
    }
}

static void
reset_parser_types_and_subtypes(struct ctx *ctx){
    ctx->media_subtypes = NULL;
    struct subtype_list *media_subtypes;
    //getchar();
    struct type_list *media_types = ctx->media_types;
    while(media_types!=NULL){
        parser_reset(media_types->type_parser);
        media_subtypes   = media_types->subtypes;
        while(media_subtypes != NULL){
            if(*media_subtypes->is_wildcard){
                media_subtypes = NULL;
            }else{
                parser_reset(media_subtypes->type_parser);
                media_subtypes = media_subtypes->next;
            }
            
        }
        media_types = media_types->next;
    }
    //getchar();
}

void print_filter_msg(struct ctx*ctx){
    char *filter=getenv("FILTER_MSG");
    if(filter==NULL){
        filter="MENSAJE DE REEMPLAZO";
    }

    int i=0;
    while(filter[i]!=0){
        ctx->process_modification_mail[0]=filter[i++];
        (ctx->process_modification_mail)++;
    }
    ctx->process_modification_mail[0]='\r';
    (ctx->process_modification_mail)++;
    ctx->process_modification_mail[0]='\n';
    (ctx->process_modification_mail)++;
}
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
        //debug("6.typehr", parser_utils_strcmpi_event, e);
        switch (e->type)
        {
        case STRING_CMP_EQ:
            ctx->msg_content_transfer_encoding_field_detected = &T;
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
        //debug("2.typehr", parser_utils_strcmpi_event, e);
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

//detecta message en el header
static void
message_header(struct ctx *ctx, const uint8_t c) {
    const struct parser_event *e = parser_feed(ctx->message_type, c);
    do
    {//debug("7.typemessage", parser_utils_strcmpi_event, e);
        switch (e->type)
        {
        case STRING_CMP_EQ:
            ctx->message_detected = &T;
            break;
        case STRING_CMP_NEQ:
            ctx->message_detected= &F;
            break;
        }

        e = e->next;
    } while (e != NULL);
}

//detecta rfc822 en el header
static void
rfc822_header(struct ctx *ctx, const uint8_t c)
{
    const struct parser_event *e = parser_feed(ctx->rfc822, c);
    do
    {
        //debug("8.type rfc822", parser_utils_strcmpi_event, e);
        switch (e->type)
        {
        case STRING_CMP_EQ:
            ctx->rfc822_detected = &T;
            break;
        case STRING_CMP_NEQ:
            ctx->rfc822_detected= &F;
            break;
        }

        e = e->next;
    } while (e != NULL);
}

static void
boundary_charset_match(struct ctx *ctx, const uint8_t c)
{
    const struct parser_event *boundary_parser_evt = parser_feed(ctx->boundary_parser_detector, c);
    //Imprimir resultados
    if (boundary_parser_evt->type == STRING_CMP_EQ)
    {
        ctx->multipart_section = &T;
        ctx->boundary_detected = &T;
    }
    
    else
    {
        //Todavia no se...
        ctx->multipart_section = NULL;
    }
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
        ctx->replace_content_type = &T;
        //Go back and replace whatever to text/plain
        //                    replace_to_plain_text(ctx,c);
    }
    else if (current->type == STRING_CMP_NEQ)
    {
        ctx->media_filter_apply = &F;
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
    while (type_list != NULL)
    {

        if(type_list->type_parser==NULL){

        }
        current = parser_feed(type_list->type_parser, c);
               
        if (current->type == STRING_CMP_EQ)
        {   
            ctx->media_type_filter_detected = &T;
            ctx->media_subtypes = type_list->subtypes;
            return;
        }else if (current->type == STRING_CMP_NEQ){
           
        }
        type_list = type_list->next;
    }
    ctx->media_type_filter_detected = &F;
    
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
    //debug("5.   ct_type", content_type_msg_event, e);
    do
    {
        switch (e->type)
        {
        case MIME_TYPE_TYPE:
            for (int i = 0; i < e->n; i++)
            {
                content_type_match(ctx, e->data[i]);
                message_header(ctx,e->data[i]);
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
                if(ctx->media_filter_apply != 0 && *ctx->media_filter_apply){
                    /*while(ctx->process_modification_mail[0] != ' '){
                        (ctx->process_modification_mail)--;
                    }
                    insert_text(ctx, "text/plain");*/
                }
            }
            if(ctx->message_detected!=0 && ctx->message_detected==&T){
                rfc822_header(ctx,e->data[0]);

            }

            break;
        case MIME_PARAMETER_START:
            //Should i do smth here?
            if (*ctx->media_filter_apply)
            {
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
            if (ctx->boundary_detected != 0 && ctx->boundary_detected == &T)
            {

               

                stack_push(ctx->boundry_stack, list_return_string(ctx->boundary_name));
                ctx->boundary_name = list_empty(ctx->boundary_name);
                if (ctx->multipart_section != NULL && *ctx->media_filter_apply && !*ctx->multipart_section)
                {
                }
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
        //debug("1.   msg", mime_msg_event, e);
        //printf("Current letter parser feed: %c\n", e->data[0]);
        //printctx(ctx);
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
            parser_reset(ctx->message_type);
            parser_reset(ctx->rfc822);
            (ctx->process_modification_mail)[0] = e->data[0];
            (ctx->process_modification_mail)++;
            if(ctx->msg_content_transfer_encoding_field_detected != 0
            && *ctx->msg_content_transfer_encoding_field_detected && *ctx->media_filter_apply){
                insert_text(ctx," 8BIT");
            }
            break;
        case MIME_MSG_VALUE:
            if (ctx->msg_content_type_field_detected != 0 && ctx->msg_content_type_field_detected == &T && !*ctx->media_filter_apply)
            {
                for (int i = 0; i < e->n; i++)
                {
                    content_type_msg(ctx, e->data[i]);
                }
            }
            if(ctx->msg_content_transfer_encoding_field_detected != 0
            && *ctx->msg_content_transfer_encoding_field_detected && *ctx->media_filter_apply){

            }else{
                if(*ctx->replace_content_type){
                    while(ctx->process_modification_mail[0] != ' '){
                        (ctx->process_modification_mail)--;
                    }
                    insert_text(ctx, " text/plain");
                    ctx->replace_content_type = &F;
                }else{
                    (ctx->process_modification_mail)[0] = e->data[0];
                    (ctx->process_modification_mail)++;
                }
            }
            
            break;
        case MIME_MSG_VALUE_END:
            // si parseabamos Content-Type ya terminamos
            ctx->msg_content_type_field_detected = 0;
            ctx->msg_content_transfer_encoding_field_detected = 0;
            ctx->boundary_detected=&F;
            if(ctx->rfc822_detected!=0 && ctx->rfc822_detected==&T){
                parser_set_state(ctx->msg,MIME_MSG_NAME);
                //ctx->rfc822_detected=&F;
            }
            break;
        case MIME_MSG_BODY_START:
        if (*ctx->rfc822_detected)
            {
                ctx->message_detected = &F;
                ctx->rfc822_detected = &F;
                parser_reset(ctx->content_type);
            }else{

                (ctx->process_modification_mail)[0] = e->data[0];
                (ctx->process_modification_mail)++;
                (ctx->process_modification_mail)[0] = e->data[1];
                (ctx->process_modification_mail)++;
                if (*ctx->media_filter_apply)
                {
                    print_filter_msg(ctx);
                    (ctx->process_modification_mail)[0] = '\r';
                    (ctx->process_modification_mail)++;
                    (ctx->process_modification_mail)[0] = '\n';
                    (ctx->process_modification_mail)++;
                }
            }

            break;
        case MIME_MSG_BODY:
                if (!*ctx->media_filter_apply)
                {
                    (ctx->process_modification_mail)[0] = e->data[0];
                    (ctx->process_modification_mail)++;
                }

                if (e->data[0] == '-')
                {
                    
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
                    //si encuentro un -- al final, prendo bool que encontre el boundary name end
                    if (e->data[0] == '-' && list_size(ctx->possible_boundary_string) != 0)
                    {
                        if (list_peek(ctx->possible_boundary_string) == '-')
                        {
                            //encontre dos -- al final
                            ctx->boundary_end = &T;
                        }
                    }
                    //si leo un - y es el primero, no lo pongo en el string
                    if (!(e->data[0] == '-' && list_size(ctx->possible_boundary_string) == 0))
                    {
                        list_append(ctx->possible_boundary_string, e->data[0]);
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

            if (ctx->double_middle_dash != 0 && ctx->double_middle_dash == &T)
            {
               
                if ((strcmp(list_return_string(ctx->possible_boundary_string), stack_peek(ctx->boundry_stack)) == 0) && ctx->boundary_end != &T)
                {
                    //el possible boundry string coincidio con el del stack
                    //lo agrego al body
                    if (*ctx->media_filter_apply)
                    {
                        (ctx->process_modification_mail)[0] = '-';
                        (ctx->process_modification_mail)++;

                        (ctx->process_modification_mail)[0] = '-';
                        (ctx->process_modification_mail)++;
                        while (list_size(ctx->possible_boundary_string) >= 1)
                        {

                            (ctx->process_modification_mail)[0] = list_head(ctx->possible_boundary_string, false);
                            list_head(ctx->possible_boundary_string, true);
                            (ctx->process_modification_mail)++;
                        }
                        (ctx->process_modification_mail)[0] = '\n';
                    (ctx->process_modification_mail)++;
                    }
                    ctx->possible_boundary_string = list_empty(ctx->possible_boundary_string);
                    parser_set_state(ctx->msg, MIME_MSG_NAME);
                    ctx->media_filter_apply=&F;
                     parser_reset(ctx->boundary_parser_detector);
                    ctx->boundary_detected=&F;
                    
                    reset_parser_types_and_subtypes(ctx);
                    ctx->msg_content_transfer_encoding_field_detected = NULL;
                    parser_reset(ctx->content_type);
                }
            }
            if (ctx->boundary_end != 0 && ctx->boundary_end == &T)
            {
               
                if (strcmp(list_ret_end_string(ctx->possible_boundary_string), stack_peek(ctx->boundry_stack)) == 0)
                {
                    //lo escribo en el body
                    if (*ctx->media_filter_apply)
                    {
                        (ctx->process_modification_mail)[0] = '-';
                        (ctx->process_modification_mail)++;

                        (ctx->process_modification_mail)[0] = '-';
                        (ctx->process_modification_mail)++;

                        while (list_size(ctx->possible_boundary_string) >= 1)
                        {

                            (ctx->process_modification_mail)[0] = list_head(ctx->possible_boundary_string, false);
                            list_head(ctx->possible_boundary_string, true);
                            (ctx->process_modification_mail)++;
                        }
                    }
                    (ctx->process_modification_mail)[0] = '\n';
                    (ctx->process_modification_mail)++;
                    stack_pop(ctx->boundry_stack);
                    ctx->possible_boundary_string = list_empty(ctx->possible_boundary_string);
                    ctx->media_filter_apply=&F;
                }
            }

            ctx->is_middle_dash = &F;
            ctx->double_middle_dash = &F;
            ctx->possible_boundary = &F;
            ctx->boundary_end=&F;

            break;
        default:
            (ctx->process_modification_mail)[0] = e->data[0];
            (ctx->process_modification_mail)++;
            break;
        }
        e = e->next;
    } while (e != NULL);
    //getchar();
}

/* Delimita una respuesta multi-línea POP3. Se encarga del "byte-stuffing" */
static void
pop3_multi(struct ctx *ctx, const uint8_t c)
{
    const struct parser_event *e = parser_feed(ctx->multi, c);
    do
    {
        //debug("", pop3_multi_event, e);
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
        //getchar();
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
    if (ctx->media_filter_apply != 0 && ctx->media_filter_apply == &T)
    {
        printf("media filter: TRUE\n");
    }
    else
    {
        printf("media filter: FALSE\n");
    }
    if (ctx->double_middle_dash != 0 && ctx->double_middle_dash == &T)
    {
        printf("Double middle dash: TRUE\n");
    }
    else
    {
        printf("Double middle dash: FALSE\n");
    }
    if (ctx->is_middle_dash != 0 && ctx->is_middle_dash == &T)
    {
        printf("middle dash: TRUE\n");
    }
    else
    {
        printf("middle dash: FALSE\n");
    }
    if (ctx->boundary_detected != 0 && ctx->boundary_detected == &T)
    {
        printf("boundary detected: TRUE\n");
    }
    else
    {
        printf("boundary detected: FALSE\n");
    }
    if (ctx->boundary_end != 0 && ctx->boundary_end == &T)
    {
        printf("boundary end: TRUE\n");
    }
    else
    {
        printf("boundary end: FALSE\n");
    }
    if (stack_size(ctx->boundry_stack) != 0)
    {
        printf("Boundary stack peek: %s\n", stack_peek(ctx->boundry_stack));
    }
    if (ctx->rfc822_detected != 0 && ctx->rfc822_detected== &T)
    {
        printf("rfc : TRUE\n");
    }
    else
    {
        printf("rfc: FALSE\n");
    }
}

static void
add_media_subtype(const char * subtype, struct subtype_list *media_subtypes){
    if(strcmp(subtype,"*") != 0){
        struct parser_definition * def      = malloc(sizeof(*def));
        struct parser_definition aux        = parser_utils_strcmpi(subtype);
        memcpy(def, &aux, sizeof(aux));
        media_subtypes->type_parser            = parser_init(parser_no_classes(), def);
        media_subtypes->is_wildcard         = &F;
        media_subtypes->subtype             = malloc(sizeof(subtype) + 1);
        media_subtypes->next=NULL;
        strcpy(media_subtypes->subtype,subtype);
    }else{
        media_subtypes->is_wildcard         = &T;
        media_subtypes->type_parser         = NULL;
        media_subtypes->subtype             = NULL;
        media_subtypes->next=NULL;
    }
}

static void
add_media_type(const char * type, const char * subtype, struct type_list *media_types){
    struct type_list *media_type_prev=NULL;
    if(media_types->type == NULL){    //Empty list
        struct parser_definition * def = malloc(sizeof(*def));
        struct parser_definition aux = parser_utils_strcmpi(type);
        memcpy(def, &aux, sizeof(aux));
        media_types->type_parser            = parser_init(parser_no_classes(), def);
        
        struct subtype_list *media_subtypes = malloc(sizeof(*media_subtypes));
        media_types->next                   = NULL;
        media_subtypes->next                = NULL;
        media_types->event                  = malloc(sizeof(*media_subtypes->event));
        media_types->subtypes               = media_subtypes;
        media_types->event                  = malloc(sizeof(*media_types->event));
        media_types->type                   = malloc(sizeof(type) + 1);
        strcpy(media_types->type,type);
        add_media_subtype(subtype, media_subtypes);
    }else{

        while(media_types != NULL){
            if(strcmp(media_types->type,type) == 0){
                //Busco en subtipos
                while(media_types->subtypes->next!=NULL){
                media_types->subtypes=media_types->subtypes->next;
            }
                if(!*media_types->subtypes->is_wildcard){
                    struct subtype_list *media_subtypes = malloc(sizeof(*media_subtypes));
                    media_subtypes->next                = NULL;
                    media_types->subtypes->next               = media_subtypes;
                    //strcpy(media_types->type,type);
                    add_media_subtype(subtype,media_subtypes);
                }
                return;
            }
            media_type_prev=media_types;
            media_types = media_types->next;
        }
            //Agrego al final
            struct type_list *media_types_extra = malloc(sizeof(*media_types_extra));
            struct subtype_list *media_subtypes = malloc(sizeof(*media_subtypes));

            struct parser_definition * def      = malloc(sizeof(*def));
            struct parser_definition aux        = parser_utils_strcmpi(type);
            memcpy(def, &aux, sizeof(aux));
            media_types_extra->type_parser      = parser_init(parser_no_classes(), def);

            media_type_prev->next               = media_types_extra;
            media_types_extra->next             = NULL;
            media_types_extra->event                  = malloc(sizeof(*media_types->event));
            media_types_extra->subtypes         = media_subtypes;
            media_types_extra->type             = malloc(sizeof(type) + 1);
            strcpy(media_types_extra->type,type);
            add_media_subtype(subtype,media_subtypes);
        }
    }

static void
free_media_filter_list(struct type_list *media_types){
    struct type_list    *media_types_aux    = media_types;
    struct subtype_list *media_subtypes_aux;
    while(media_types_aux != NULL){
        free(media_types_aux->type);
        free(media_types_aux->event);
        parser_destroy(media_types_aux->type_parser);
        media_subtypes_aux = media_types_aux->subtypes;
        while (media_subtypes_aux != NULL) {
            free(media_subtypes_aux->event);
            free(media_subtypes_aux->is_wildcard);
            if(media_subtypes_aux->type_parser != NULL)
                parser_destroy(media_subtypes_aux->type_parser);
            struct subtype_list *aux = media_subtypes_aux;
            media_subtypes_aux = media_subtypes_aux->next;
            free(aux);
        }
        free(media_subtypes_aux);
        struct type_list *aux2 = media_types_aux;
        media_types_aux = media_types_aux->next;
        free(aux2);
    }
    free(media_types_aux);
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
        parser_utils_strcmpi("text/plain");

    struct parser_definition message_type_def = parser_utils_strcmpi("message");
    struct parser_definition rfc822_type_def = parser_utils_strcmpi("rfc822");

    struct type_list *media_types = malloc(sizeof(*media_types));
	struct type_list *media_types_aux=media_types;
    struct subtype_list *subtype_list_aux=media_types->subtypes;
    

    char * flm = getenv("FILTER_MEDIAS");
    if (media_types == NULL) {
        return -1;
    }
	if (flm == NULL) {
		printf("No filter medias, please add one\n");
        flm = "";
        //TODO write in pipe the same message!!
        return 1;
	}
	char * medias = malloc(strlen(flm) + 1);
	if (medias == NULL) {
		free(media_types);
		return -1;
	}
	strcpy(medias, flm);
	const char * comma   = ",";
	const char * slash   = "/";
	char * context       = "comma";
	char * context_b     = "slash";
	char * current;
	char * mime;
	/*tomar primer media*/
	current = strtok_r(medias, comma, &context);
	while (current != NULL) {
		char * aux = malloc(strlen(current) + 1);
		if (aux == NULL) {
			free(aux);
			return -1;
		}
		strcpy(aux, current);
		/* getting type */
		mime = strtok_r(aux, slash, &context_b);
		if (mime == NULL) {
            free(aux);
			return -1;
		}
		char * type = malloc(strlen(mime) + 1);
		if (type == NULL) {
            free(type);
			return -1;
		}
		strcpy(type, mime);
		/*getting subtype*/
		mime = strtok_r(NULL, slash, &context_b);
		if (mime == NULL) {
			return -1;
		}
		char * subtype = malloc(strlen(mime) + 1);
		if (subtype == NULL) {
            free(subtype);
			return -1;
		}
		strcpy(subtype, mime);
		add_media_type(type, subtype, media_types);
        media_types=media_types_aux;
		free(aux);
		current = strtok_r(NULL, comma, &context);
        
	}
        //media_types->subtypes=subtype_list_aux;
        
        // while(media_types!=NULL){
        //     printf("Media types: %s\n",media_types->type);
        //     while(media_types->subtypes!=NULL){
        //         printf("subtipe %s\n",media_types->subtypes->subtype);
        //         media_types->subtypes=media_types->subtypes->next;
        //     }
        //     media_types=media_types->next;
        // }
    free(medias);
    


    struct parser_definition boundary_parser = parser_utils_strcmpi("boundary");
    struct parser_definition charset_parser = parser_utils_strcmpi("charset");

    struct ctx ctx = {
        .multi = parser_init(no_class, pop3_multi_parser()),
        .msg = parser_init(init_char_class(), mime_message_parser()),
        .content_type = parser_init(init_char_class(), mime_type_parser()),
        .ctype_header = parser_init(no_class, &media_header_def),
        .ctype_value = parser_init(no_class, &media_value_def),
        .message_type = parser_init(no_class, &message_type_def),
        .ctransfer_encoding_header = parser_init(no_class, &content_transfer_encoding_header_def),
        .rfc822 = parser_init(no_class, &rfc822_type_def),
        .msg_content_filter = &F,
        .media_type_filter_detected = &F,
        .media_filter_apply = &F,
        .replace_content_type = &F,
        .media_types = media_types_aux,
        .media_subtypes = NULL,
        .multipart_section = NULL,
        .boundary_detected = &F,
        .possible_boundary_string = list_new(sizeof(uint8_t), NULL),
        .double_middle_dash = &F,
        .boundary_parser_detector = parser_init(no_class, &boundary_parser),
        .msg_content_transfer_encoding_field_detected = NULL,
        .boundry_stack = stack_new(NULL),
        .boundary_name = list_new(sizeof(uint8_t), NULL),
        .boundary_end = &F,
        .message_detected=&F,
        .rfc822_detected=&F,
    };
    uint8_t data[9999], transformed[9999];
    memset(transformed, '\0', sizeof(transformed));
    int n;
    do
    {
        n = read(fd, data, sizeof(data)); //Read from pipe instead of file!
        ctx.process_modification_mail = transformed;
        ssize_t i;
        for (i = 0; i < n; i++)
        {
            pop3_multi(&ctx, data[i]);
        }
        printf("%s",transformed);
        
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
    parser_destroy(ctx.ctransfer_encoding_header);
    parser_utils_strcmpi_destroy(&content_transfer_encoding_header_def);
    parser_destroy(ctx.boundary_parser_detector);
    parser_utils_strcmpi_destroy(&boundary_parser);
    //To be removed
    parser_utils_strcmpi_destroy(&charset_parser);
    //free_media_filter_list(ctx.media_types);
    
}