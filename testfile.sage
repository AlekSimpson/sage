// Tests a three-level pointer indirection chain: A -> B -> C -> value.
// Extends structs_eight's hyper-dim matrix to verify that deeply nested
// pointer dereferences in field access chains compute correct addresses.
//
// Expected stdout:
//   puti(a.next.next.data, 2) -> prints 99
// good

Node :: struct {
    data: int
    next: Node*
}

c: Node
c.data = 99

b: Node
b.data = 2
b.next = ^c

a: Node
a.data = 1
a.next = ^b

puti(a.next.next.data, 2)
