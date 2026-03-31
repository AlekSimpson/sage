// Tests reading a field through a multi-hop pointer chain: node1 -> node2 -> value.
// Extends structs_six (linked list) by actually reading a field at the end of the chain.
//
// Expected stdout:
//   puti(node1.next.value, 3) -> prints 42
// good

Link :: struct {
    value: int
    next: Link*
}

node2: Link
node2.value = 42

node1: Link
node1.value = 1
node1.next = ^node2

puti(node1.next.value, 2)
