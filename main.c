#include <stdio.h>
#include <stdlib.h>

// The number of objects at which we kick off the first GC
#define STACK_MAX 256
#define INIT_OBJ_NUM_MAX 8

/*
    The little language is dynamically typed
    It has two types of objects: ints and pairs
*/
typedef enum {
    OBJ_INT,
    OBJ_PAIR
} ObjectType;


typedef struct sObject {
    struct sObject* next;
    unsigned char marked;
    ObjectType type;

    union
    {
        /* OBJ_INT */
        int value;

        /* OBJ_PAIR */
        struct
        {
            struct sObject* head;
            struct sObject* tail;
        };
    };
    
} Object;


typedef struct {
    /* 1. keep track of the number of allocated objects to determine when to gc */
    // The total number of currently allocated objects.
    int numObjects;
    // The number of objects required to trigger a GC.
    int maxObjects;

    /* 2. Purpose: a linkedlist to record all allocated objects for determine what to free */
    Object* firstObject;

    /* 3 the VM stack to keep track of runtime variables */
    Object* stack[STACK_MAX];
    int stackSize;
} VM;

void markAll(VM* vm);
void sweep(VM* vm);

void assert(int condition, const char* message) {
  if (!condition) {
    printf("%s\n", message);
    exit(1);
  }
}

/*****/
VM* newVM() {
    VM* vm = malloc(sizeof(VM));
    vm->stackSize = 0;
    vm->firstObject = NULL;

    // Purpose: determine when to trigger gc
    vm->numObjects = 0;
    vm->maxObjects = INIT_OBJ_NUM_MAX;

    return vm;
}

void push(VM* vm, Object* value) {
    assert(vm->stackSize < STACK_MAX, "Stack overflow!");
    vm->stack[vm->stackSize++] = value;
}

Object* pop(VM* vm) {
    assert(vm->stackSize > 0, "Stack underflow!");
    return vm->stack[--vm->stackSize];
}

void mark(Object* object) {
    if (object->marked) return;

    object->marked = 1;

    if (object->type == OBJ_PAIR) {
        mark(object->head);
        mark(object->tail);
    }
}

/*
    From the beginning of the stack, and mark all the Object in stack as 1
*/
void markAll(VM* vm)
{
    for (int i = 0; i < vm->stackSize; i++) {
        mark(vm->stack[i]);
    }
}

// Sweepy sweep

void sweep(VM* vm) {
    // An address of the first item of an array containing addresses
    Object** object = &vm->firstObject;

    // the last item in the linkedlist is null
    while(*object) {
        if (!(*object)->marked) {
            Object* unreached = *object;

            // Move to next item in the linkedlist
            *object = unreached->next;
            free(unreached);
            vm->numObjects--;
        } else {
            /* This object was reached, so unmark it (for the next GC) */
            (*object)->marked = 0;
            object = &(*object)->next;
        }
    }
}

void gc(VM* vm) {
    int numObjects = vm->numObjects;

    // Everytime before gc start, all object should have mark set to 0
    markAll(vm);
    // After markAll, all Object lives in the stack should be marked to 1
    sweep(vm);
    // Based on the linkedlist, it free all Object not in the stack
    // For Object that exists in the stack, it set the mark to 0 for next gc

    /* 
        The multiplier there lets our heap grow as the number of living objects increases. 
        Likewise, it will shrink automatically if a bunch of objects end up being freed 
    */
    vm->maxObjects = vm->numObjects * 2;
}

Object* newObject(VM* vm, ObjectType type) {
    // When creating new Object, check if it already which the capacity that require gc
    if (vm->numObjects == vm->maxObjects) {
        printf("Auto gc is ran\n");
        gc(vm);
    }

    Object* object = malloc(sizeof(Object));
    object->type = type;
    object->marked = 0;

    /* 
        Insert it into the linkedlist of allocated objects 
        Add to the start of the linkedlist (the most beginning node is always the most recent added)
    */
    object->next = vm->firstObject;
    vm->firstObject = object;

    // Keeping track of how many allocated Object (for GC)
    vm->numObjects++;
    return object;
}

/*****/
void pushInt(VM* vm, int intValue) {
    Object* object = newObject(vm, OBJ_INT);
    object->value = intValue;
    push(vm, object);
}

/*****/
Object* pushPair(VM* vm) {
    Object* object = newObject(vm, OBJ_PAIR);
    object->tail = pop(vm);
    object->head = pop(vm);

    push(vm, object);
    return object;
}



void freeVM(VM *vm) {
  vm->stackSize = 0;
  gc(vm);
  free(vm);
}


// TESTS

void test1() {
    printf("Test 1: Objects on the stack are preserved.\n");
    VM* vm = newVM();
    pushInt(vm, 1);
    pushInt(vm, 2);

    gc(vm);
    assert(vm->numObjects == 2, "Should have preserved objects.");
    freeVM(vm);
}

void test2() {
    printf("Test 2: Unreached objects are collected.\n");
    VM* vm = newVM();
    pushInt(vm, 1);
    pushInt(vm, 2);
    pop(vm);
    pop(vm);

    gc(vm);
    assert(vm->numObjects == 0, "Should have collected objects.");
    freeVM(vm);
}

void test3() {
    printf("Test 3: Reach nested objects.\n");
    VM* vm = newVM();
    pushInt(vm, 1);
    pushInt(vm, 2);
    pushPair(vm);
    pushInt(vm, 3);
    pushInt(vm, 4);
    pushPair(vm);
    pushPair(vm);

    assert(vm->stackSize == 1, "Should only have one object left");
    assert(vm->numObjects == 7, "Should have reached objects.");
    freeVM(vm);
}

void test4() {
    printf("Test 4: Handle cycles.\n");
    VM* vm = newVM();
    pushInt(vm, 1);
    pushInt(vm, 2);
    Object* a = pushPair(vm);
    pushInt(vm, 3);
    pushInt(vm, 4);
    Object* b = pushPair(vm);

    /* Set up a cycle, and also make 2 and 4 unreachable and collectible */
    a->tail = b;
    b->tail = a;

    assert(vm->stackSize == 2, "Stack size should be two");
    assert(vm->numObjects == 6, "6 objects should be allocated before gc");
    gc(vm);
    assert(vm->numObjects == 4, "two objects is allocated but not used");
    int count = 0;
    Object* head = vm->firstObject;
    while (head) {
        count++;
        head = head->next;
    }
    printf("Counts: %d\n", count);
    free(vm);
}

// Test auto gc
void test5() {
    printf("Test 5: VM should trigger gc by itself when reach a capacity.\n");
    VM* vm = newVM();
    pushInt(vm, 1); // +1
    pushInt(vm, 2); // +2
    Object* a = pushPair(vm); // +3
    pushInt(vm, 3); // +4
    pushInt(vm, 4); // +5
    Object* b = pushPair(vm); // +6

    /* Set up a cycle, and also make 2 and 4 unreachable and collectible */
    a->tail = b;
    b->tail = a;

    pushInt(vm, 5); // +7
    pushInt(vm, 6); // +8  
    Object* c = pushPair(vm); // +9 -> gc should be perform at this point


    printf("%d\n", vm->numObjects);
    assert(vm->stackSize == 3, "Stack size should be three");
    assert(vm->numObjects == 7, "gc should be run already");
    // gc(vm);
    // assert(vm->numObjects == 4, "two objects is allocated but not used");
    int count = 0;
    Object* head = vm->firstObject;
    while (head) {
        count++;
        head = head->next;
    }
    printf("Counts: %d\n", count);
    free(vm);
}

int main() {
    test1();
    test2();
    test3();
    test4();
    test5();

    printf("ok\n");
    return 0;
}