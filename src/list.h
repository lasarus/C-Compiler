#ifndef LIST_H
#define LIST_H

#include <stdlib.h>

/*
  Example usage:
  LIST(type_list, struct type *);
  int f(void) {
  struct type_list *tl = NULL;
  type_list_add(&tl, type_simple(T_INT32));
  return tl ? tl->n : 0;
  }
*/

#define LIST_SIZE(LIST) ((LIST) ? (LIST)->n : 0)
#define LIST_INDEX(LIST, IDX) ((LIST)->list[IDX])

#include <string.h>

#define LIST_FREE_EQ(NAME, T, FREE, EQ)									\
	struct NAME {														\
		int n, capacity;												\
		T list[];														\
	};																	\
	inline static struct NAME *NAME##_dup(struct NAME *list) {			\
		if (list == NULL) return NULL;									\
		size_t sz = sizeof(struct NAME) + list->capacity * sizeof (T);	\
		struct NAME *ret = malloc(sz);									\
		memcpy(ret, list, sz);											\
		return ret;														\
	}																	\
	inline static void NAME##_add(struct NAME **listq, T t) {			\
		int cap = *listq ? (*listq)->capacity : 0;						\
		int n = *listq ? (*listq)->n : 0;									\
		if (cap == 0 || n >= cap) {										\
			cap = cap * 4;												\
			if (!cap)													\
				cap = 2;												\
			size_t sz = sizeof(struct NAME) + cap * sizeof t;			\
			*listq = realloc(*listq, sz);									\
		}																\
		(*listq)->capacity = cap;										\
		(*listq)->n = n;													\
		(*listq)->list[(*listq)->n++] = t;								\
	}																	\
	inline static void NAME##_push_front(struct NAME **list, T t) {		\
		NAME##_add(list, t);											\
		for (int i = LIST_SIZE(*list) - 1; i >= 1; i--) {				\
			(*list)->list[i] = (*list)->list[i - 1];					\
		}																\
		(*list)->list[0] = t;											\
	}																	\
	inline static void NAME##_remove(struct NAME **list, int idx) {		\
		FREE(&(*list)->list[idx]);										\
		if (idx != (*list)->n - 1)										\
			(*list)->list[idx] = (*list)->list[(*list)->n - 1];			\
		(*list)->n--;													\
	}																	\
	inline static int NAME##_is_empty(struct NAME *list) {				\
		return list == NULL || list->n == 0;							\
	}																	\
	inline static void NAME##_free(struct NAME *list) {					\
		free(list);				/*TODO: free all elements as well.*/	\
	}																	\
	inline static T *NAME##_top(struct NAME *list) {					\
		return &list->list[list->n - 1];								\
	}																	\
	inline static void NAME##_pop(struct NAME **list) {					\
		NAME##_remove(list, (*list)->n - 1);							\
	}																	\
	inline static int NAME##_index_of(struct NAME *list, T t) {			\
		(void)list, (void)t;											\
		for (int i = 0; i < LIST_SIZE(list); i++) {						\
			if (EQ(list->list[i], t)) return i;							\
		}																\
		return -1;														\
	}																	\
	static inline void NAME##_insert (struct NAME **list, T t) {		\
		int index = NAME##_index_of(*list, t);							\
		if(index == -1)													\
			NAME##_add(list, t);										\
	}																	\
	static inline struct NAME *NAME##_union (struct NAME *a, struct NAME *b) { \
		struct NAME *ret = NULL;										\
		for(int i = 0; i < LIST_SIZE(a); i++)							\
			NAME##_insert(&ret, LIST_INDEX(a, i));						\
		for(int i = 0; i < LIST_SIZE(b); i++)							\
			NAME##_insert(&ret, LIST_INDEX(b, i));						\
		return ret;														\
	}																	\
	static inline struct NAME *NAME##_intersection (struct NAME *a, struct NAME *b) { \
		struct NAME *ret = NULL;										\
		for(int i = 0; i < LIST_SIZE(a); i++)							\
			if (NAME##_index_of(b, LIST_INDEX(a, i)) != -1)				\
				NAME##_insert(&ret, LIST_INDEX(a, i));					\
		return ret;														\
	}																	\
	struct NAME

#define NULL_FREE(T)
#define NULL_EQ(T1, T2) 1
#define LIST(NAME, T) LIST_FREE_EQ(NAME, T, NULL_FREE, NULL_EQ)

#define FOREACH_NODE2(X, LIST)											\
	if (LIST)															\
		for (_Bool _b = 1; _b;)											\
			for (node_idx X; _b; _b = 0)								\
				for (int _i = 0; X = LIST->list[_i], _i < LIST->n; _i++)

#define FOREACH_USE(IT, NODE)					\
	FOREACH_NODE2(IT, get_node(NODE)->uses)

#endif
