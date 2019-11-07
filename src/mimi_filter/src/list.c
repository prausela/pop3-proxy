#include "list.h"
 
list* list_new(int elementSize, freeFunction freeFn)
{
    list *l = malloc(sizeof(list));
    l->logicalLength = 0;
    l->elementSize = elementSize;
    l->tail = NULL;
    l->freeFn = freeFn;
    l->head = NULL;
  return l;
}

void list_destroy(list *list)
{
  listNode *current;
  while(list->head != NULL) {
    current = list->head;
    list->head = current->next;

    
    free(current);
  }
}

void list_append(list *list, uint8_t element)
{

  listNode *node = malloc(sizeof(listNode));
  node->data=element;
            printf("murio\n\n");

  node->next = NULL;
          printf("murio2\n\n");
          printf("Data pointer: %c\n",element);

         
  if(list->logicalLength == 0) {
    list->head = list->tail = node;
  } else {
    list->tail->next = node;
    list->tail = node;
  }
            printf("murio4\n\n");

  list->logicalLength++;
}

uint8_t list_head(list *list, bool removeFromList)
{
  assert(list->head != NULL);
 
  listNode *node = list->head;
  
 
  if(removeFromList) {
    list->head = node->next;
    list->logicalLength--;
 
    free(node);
  }
      return node->data;

}

int list_size(list *list)
{
  return list->logicalLength;
}