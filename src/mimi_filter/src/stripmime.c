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

    //parsea la palabra boundary en el header
    struct parser*          boundary;

    //pusheo los boundaries encontrados
    struct stack*           boundary_stack;

    /* ¿hemos detectado si el field-name que estamos procesando refiere
     * a Content-Type?. Utilizando dentro msg para los field-name.
     */
    bool                    *msg_content_type_field_detected;
    bool                    *media_type_filter_detected;
    bool                    *msg_content_filter;
    bool                    *media_filter_apply;
    uint8_t                 *process_modification_mail;

    //se declaro un boundry en el header?
    bool                    *boundary_detected;

    //el ultimo caracter leido en el body es un -?
    bool                    *is_middle_dash;

    //los ultimos dos leidos son --?
    bool                    *double_middle_dash;

    //esa linea puede ser un boundry? Si no arranca con - la linea, lo pongo en false
    bool                    *possible_boundary;
    

    struct type_list*      media_types;
    struct subtype_list*   media_subtypes;

    //cambiar y hacer con linked list pero no me salia
    char                   possible_boundary_string[10];

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

/* Detecta un boundary */
static void boundary_analizer(struct ctx * ctx, const uint8_t c) {
    const struct parser_event * e = parser_feed(ctx->boundary, c);

    do {

        switch (e->type) {
            case STRING_CMP_EQ:
                ctx->boundary_detected = &T;
                break;
            case STRING_CMP_NEQ:
                ctx->boundary_detected = &F;
                break;
            default:
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

static void
content_subtype_match(struct ctx* ctx,const uint8_t c){
    struct subtype_list* subtype_list = ctx->media_subtypes;
    struct parser_event * current;
    if(subtype_list->is_wildcard != 0 && *subtype_list->is_wildcard){
        current             = malloc(sizeof(*current));
        current->type       = STRING_CMP_EQ;
        current->data[0]    = c;
        current->n          = 1;
    } else {
        subtype_list->event = parser_feed(subtype_list->type_parser,c);
        current             = subtype_list->event;

        while (subtype_list->next != NULL) {
            subtype_list        = subtype_list->next;
            subtype_list->event = parser_feed(subtype_list->type_parser,c);

            if(subtype_list->event->type == STRING_CMP_EQ){
                current = subtype_list->event;
            }
        }
    }
    if(current->type == STRING_CMP_EQ){
        //Match found!
        ctx->media_filter_apply = &T;
    }else{
        //Overwriting just in case
        ctx->media_filter_apply = &F;
    }
}

static void
content_type_match(struct ctx* ctx,const uint8_t c){
    struct type_list* type_list = ctx->media_types;
    if(type_list == NULL){
       //TODO: Error handling!!
       return;
    }
    
    /* Leave in current*/
    struct parser_event * current;
    type_list->event = parser_feed(type_list->type_parser,c);
    current = type_list->event;
    
    while(type_list->next != NULL){
        type_list=type_list->next;
        type_list->event = parser_feed(type_list->type_parser,c);
        if(type_list->event->type == STRING_CMP_EQ){
            current = type_list->event;
        }
    }
    if(current->type == STRING_CMP_EQ){
        //Match found!
        ctx->media_type_filter_detected = &T;
        ctx->media_subtypes = type_list->subtypes;
    }else{
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
content_type_msg(struct ctx* ctx, const uint8_t c) {
    const struct parser_event* e = parser_feed(ctx->content_type,c);
    debug("5.   ct_type", content_type_msg_event, e);
    fprintf(stderr,"Current letter parser feed: %c\n",e->data[0]);
    do {
        switch (e->type){
            case MIME_TYPE_TYPE:
                for(int i = 0; i < e->n; i++) {
                    content_type_match(ctx, e->data[i]);
                }
                break;
            case MIME_TYPE_TYPE_END:
                //Aca encontre el /
                if(!(*ctx->media_type_filter_detected)){
                    //Nothing to be done here! HELP
                }
                break;
            case MIME_TYPE_SUBTYPE:
                if(*ctx->media_type_filter_detected){
                    for(int i = 0;i < e->n;i++){
                        content_subtype_match(ctx, e->data[0]);
                    }
                }
                break;
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
mime_msg(struct ctx *ctx, const uint8_t c) {
    const struct parser_event* e = parser_feed(ctx->msg, c);
    //para no entrar a -- si estoy parada ahi
    int set=0;
    int i=0;

    do {
        debug("1.   msg", mime_msg_event, e);
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
                        content_type_value(ctx, e->data[i]);
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
            printf("Mime Body\n");
                if(ctx->boundary_detected!=0 && ctx->boundary_detected==&T){
                    printf("No deberia entrar\n");
                    if(e->data[0]=='-'){
                        //tengo que setear el possible boundry cuando arranca la linea
                        ctx->possible_boundary=&T;
                        if(ctx->possible_boundary!=0 && ctx->possible_boundary==&T){
                            if(ctx->is_middle_dash!=0 && ctx->is_middle_dash==&T){
                                //consegui dos guiones juntos, guardo todo lo que 
                                //sigue (hasta el new line) para compararlo con el ultimo del stack,
                                ctx->double_middle_dash=&T;
                            }
                        }
                    }
                    //se me va a poner el - en el possible string, sacarlo.
                    if(ctx->double_middle_dash!=0 && ctx->double_middle_dash==&T){
                        printf("No deberia entrar x2\n");

                        int i=0;
                        while(ctx->possible_boundary_string[i] != 0){
                            i++;
                        }
                        ctx->possible_boundary_string[i++]=e->data[0];
                        ctx->possible_boundary_string[i]=0;
                        i=0;
                        printf("String armandose:");
                        while(ctx->possible_boundary_string[i] != 0){
                            printf("%c",ctx->possible_boundary_string[i]);
                            i++;
                        }
                        printf("\n");
                    }
                
                }
                if(e->data[0]=='-'){
                    printf("No deberia entrar x3\n");
                    ctx->is_middle_dash=&T;
                }
                else
                {
                    ctx->is_middle_dash=&F;                
                }              
                break;

            case MIME_MSG_BODY_CR:
                break;

            case MIME_MSG_BODY_NEWLINE:
                if(ctx->double_middle_dash!=0 && ctx->double_middle_dash==&T){
                    printf("Boundry String: ");
                    while(ctx->possible_boundary_string[i] != 0){
                        printf("%c",ctx->possible_boundary_string[i]);
                        i++;
                    }
                    printf("\n");
                }
                i=0;
                while(ctx->possible_boundary_string[i] != 0){
                    (ctx->process_modification_mail)[0]=ctx->possible_boundary_string[i];
                    (ctx->process_modification_mail)++;
                    i++;
                }  
                i=0;
                while(ctx->possible_boundary_string[i] != 0){
                    ctx->possible_boundary_string[i++]=0;
                }
                
                ctx->is_middle_dash=&F;
                ctx->double_middle_dash=&F;
                ctx->possible_boundary=&F;
            
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
    // printf("STRING en pop3: ");
    // for(int i=0;i<10;i++){
    //     printf("%d",ctx->possible_boundry_string[i]);
    // }
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
                
                parser_reset(ctx->msg);
                parser_reset(ctx->content_type);
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
    
    struct parser_definition type1 =
            parser_utils_strcmpi("text");
    struct parser_definition subtype1 =
            parser_utils_strcmpi("plain");

    media_types->next=NULL;
    media_types->type_parser = parser_init(no_class, &type1);
    media_types->subtypes = media_subtypes;
    media_subtypes->next=NULL;
    media_subtypes->is_wildcard=&F;
    media_subtypes->type_parser = parser_init(no_class,&subtype1);



    struct ctx ctx = {
        .multi                      = parser_init(no_class, pop3_multi_parser()),
        .msg                        = parser_init(init_char_class(), mime_message_parser()),
        .content_type               = parser_init(init_char_class(), mime_type_parser()),
        .ctype_header               = parser_init(no_class, &media_header_def),
        .ctype_value                = parser_init(no_class, &media_value_def), 
        .msg_content_filter         = &F, 
        .media_type_filter_detected = &F,
        .media_filter_apply         = &F,
        .media_types                = media_types,
        .media_subtypes             = NULL,
        .boundary_detected           = &F,
        .possible_boundary_string    = {0,0,0,0,0,0,0,0,0,0},
        .boundary_detected           = &T,
        .double_middle_dash         = &F,
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

