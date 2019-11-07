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

list* list_empty(list *list){
  int elem=list->elementSize;
  list_destroy(list);
  return list_new(elem,NULL);
}

void list_append(list *list, uint8_t element)
{

  listNode *node = malloc(sizeof(listNode));
  node->data=element;

  node->next = NULL;

  if(list->logicalLength == 0) {
    list->head = list->tail = node;
  } else {
    list->tail->next = node;
    list->tail = node;
  }
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

char* list_return_string(list *list){
  int size=list_size(list);
  char* ret=malloc(list_size(list));
  int i=0;
  while(i<size){
    ret[i]=list_head(list,false);
    list_head(list,true);
    i++;
  }
  return ret;
}