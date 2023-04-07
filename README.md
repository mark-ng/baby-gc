# Mark-and-sweep Garbage Collector

> Following the tutorial by Bob Nystrom to build a [Baby’s First Garbage Collector](https://journal.stuffwithstuff.com/2013/12/08/babys-first-garbage-collector/)

## Language implementation detail

- Minimal Stack-based (like the JVM) virtual machine
- Dynamically typed language (like CPython), objects require allocating space in the heap (malloc)
- There is only two types of objects: ints and pairs. A pair can be a pair of anything, two ints, an int and another pair, etc.
- To create a pair object, you need pop the top two objects from the stack
- The object is pushed and poped from the stack without worrying about memory leak when an object is popped out of the stack without properly free(). The garbage collector will do the job to reclaim all unused objects

## How garbage collector work?

- It divides to two steps
  - First, marked objects in the stack as used and recursively all the objects referenced by the in used objects.
  - Second, since there is no way to trace the unused objects from the stack. The method here is to maintain a linked list in the vm holding all allocated objects. When gc started, it will traverse the linkedlist to remove all objected which is not marked as used
- The gc will be triggered once the amount of initialize objects reach a certain amount. 

## dev

Compile and run

```bash
make && ./markandsweep
```

Clean up

```bash
make clean
```

