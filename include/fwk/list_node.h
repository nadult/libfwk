// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/sys_base.h"

namespace fwk {

// Simple functions for lists of elements which are identified with integer values.
// User has to provide an accessor, which for given number returns reference to ListNode.

struct ListNode {
	bool empty() const { return next == -1 && prev == -1; }
	int next = -1, prev = -1;
};

struct List {
	bool empty() const { return head == -1; }
	int head = -1, tail = -1;
};

// TODO: add functions to remove head / tail

// Inserts new element at the front of the list.
// Assumes that node is disconnected (may be uninitialized).
template <class Accessor> void listInsert(Accessor &&accessor, List &list, int idx) {
	ListNode &node = accessor(idx);

	node.prev = -1;
	node.next = list.head;
	if(list.head == -1)
		list.tail = idx;
	else
		accessor(list.head).prev = idx;
	list.head = idx;
}

// Adds all elements from source at the beginning of target. Source is cleared.
template <class Accessor> void listMerge(Accessor &&accessor, List &target, List &source) {
	if(target.head == -1) {
		swap(source, target);
		return;
	}

	if(source.head == -1)
		return;

	ListNode &right = accessor(target.head);
	ListNode &left = accessor(source.tail);

	left.next = target.head;
	right.prev = source.tail;
	target.head = source.head;
	source.head = source.tail = -1;
}

// Assumes that node is on this list.
template <class Accessor> void listRemove(Accessor &&accessor, List &list, int idx) {
	ListNode &node = accessor(idx);
	int prev = node.prev, next = node.next;

	if(prev == -1) {
		list.head = next;
	} else {
		accessor(node.prev).next = next;
		node.prev = -1;
	}

	if(next == -1) {
		list.tail = prev;
	} else {
		accessor(next).prev = prev;
		node.next = -1;
	}
}
}
