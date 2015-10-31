/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: list.hpp,v 1.11 2015/05/21 18:52:40 thor Exp $
 **
 ** In this module: Generic list header
 **********************************************************************************/

#ifndef LIST_HPP
#define LIST_HPP
/// Includes
#include "types.hpp"
///

/// Forward declarations
template <class T> class List;
///

/// class Node
// This defines a generic node you may derive from
// The thing is tricky because a) we enforce that every
// object that inherits this inherits a node with its own
// type as the template type of this node structure. This
// adds the complication that the object might inherit more
// than one node at once so we must present the namespacing
// operator overall to ensure that we get the "right" node
// here. The right node is always the node of its own type.
template <class T> class Node {
  // The List may enter here
  friend class  List<T>;
  //
#ifndef HAS_PRIVATE_ACCESS
public:
#endif
  //
  // Linkage pointers aLONG the node.
  T             *Next,*Prev;
  //
  // Linkage to the list head. Unfortunately, we need it to remove
  // us in head/tail position savely
  List<T>       *Head;
  //
protected:
  // Don't delete this object without refering the base class
  ~Node(void)
  { }
public:
  //
  // Remove the node from the list
  void Remove(void) 
  {
    if (Next) {
      Next->Node<T>::Prev = Node<T>::Prev;
    } else {
      Head->List<T>::Tail = Node<T>::Prev;
    }
    if (Prev) {
      Prev->Node<T>::Next = Node<T>::Next;
    } else {
      Head->List<T>::Head = Node<T>::Next;
    }
#if CHECK_LEVEL > 0
    Next = Prev = NULL;
    Head        = NULL;
#endif
  }
  //
  // Return the next node from this list
  T *NextOf(void) const
  {
    return Next;
  }
  //
  // Return the previous node from this list
  T *PrevOf(void) const
  {
    return Prev;
  }
};
///

/// class List
// This defines a generic list header
template <class T> class List {
  // The Node may enter here
  friend class Node<T>;
  //
  // First node, Last Node.
  T *Head,*Tail;
  //
  //
public:
  List(void)
    : Head(NULL), Tail(NULL)
  { }
  //
  // Add a node to the top of a list
  void AddHead(T *node)
  {
    node->Node<T>::Next   = Head;
    node->Node<T>::Prev   = NULL;
    if (Head) {
      Head->Node<T>::Prev = node;
    } else {
      Tail                = node;
    }
    Head                  = node;
    node->Node<T>::Head   = this;
  }
  //
  // Add a node to the end of a list
  void AddTail(T *node)
  {
    node->Node<T>::Next   = NULL;
    node->Node<T>::Prev   = Tail;
    if (Tail) {
      Tail->Node<T>::Next = node;
    } else {
      Head                = node;
    }
    Tail                  = node;
    node->Node<T>::Head   = this;
  }
  //
  // Remove the first node from the list
  T *RemHead(void)
  {
    T *node;
    //
    if ((node = Head)) {
      // The node exists
      node->Node<T>::Remove();
      return node;
    }
    return NULL;
  }
  //
  // Remove the last node from the list
  T *RemTail(void)
  {
    T *node;
    //
    if ((node = Tail)) {
      // The node exists
      node->Node<T>::Remove();
      return node;
    }
    return NULL;
  }
  //
  // Insert this node after a given node.
  // Unfortunately, we cannot make this a member of the
  // node class as we would need a pointer to the object
  // the node is part of to insert it properly, which we don't.
  // Ok, the replacement is now here as a static list method.
  // The first argument is the node we want to insert, and
  // the second the node we want to insert behind.
  static void InsertAfter(T *that,T *node)
  {
    that->Node<T>::Head = node->Node<T>::Head;
    // Our next is the next of the node, and our prev is the node
    // we insert behind.
    that->Node<T>::Prev = node;
    if ((that->Node<T>::Next = node->Node<T>::Next)) {
      // The prev of our next is this node, and the next of node is this as well.
      that->Node<T>::Next->Node<T>::Prev = that;
    } else {
      // We are about to get the last node of the list
      that->Node<T>::Head->List<T>::Tail = that;
    }
    node->Node<T>::Next = that;
  }
  //
  // Insert this node in front of a given node. For all other wierdos,
  // see above.
  static void InsertBefore(T *that,T *node)
  {
    that->Node<T>::Head = node->Node<T>::Head;
    // Our next is the node we insert before, and the prev is the 
    // prev of the node we insert before.
    that->Node<T>::Next = node;
    if ((that->Node<T>::Prev = node->Node<T>::Prev)) {
      // The next of our prev is this node, and the prev of the node
      // we insert before is this node as well.
      that->Node<T>::Prev->Node<T>::Next = that;
    } else {
      // We are about to get the first node of the list
      that->Node<T>::Head->List<T>::Head = that;
    }
    node->Node<T>::Prev = that;
  }
  //
  // Check whether the list is empty
  bool IsEmpty(void) const
  {
    return (Head == NULL)?(true):(false);
  }
  //
  // Return the first node of the list
  T *First(void) const
  {
    return Head;
  }
  //
  // Return the last node of the list
  T *Last(void) const
  {
    return Tail;
  }
};
///

///
#endif

