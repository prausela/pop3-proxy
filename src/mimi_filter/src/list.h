
#ifndef __LIST_H
#define __LIST_H

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>


// a common function used to free malloc'd objects
typedef void (*freeFunction)(void *);

typedef bool (*listIterator)(void *);
 
typedef struct _listNode {
  uint8_t data;
  struct _listNode *next;
} listNode;
 
typedef struct {
  int logicalLength;
  int elementSize;
  listNode *head;
  listNode *tail;
  freeFunction freeFn;
} list;
 
list* list_new(int elementSize, freeFunction freeFn);
void list_destroy(list *list);
 
void list_prepend(list *list, void* element);
void list_append(list *list, uint8_t element);
int  list_size(list *list);
 
void list_for_each(list *list, listIterator iterator);
uint8_t list_head(list *list, bool removeFromList);
void list_tail(list *list, void *element);
list* list_empty(list *list);
char* list_return_string(list *list);
uint8_t list_peek(list *list);
char* list_ret_end_string(list *list1);

 
#endif