// Tests writing to a struct through a pointer and then reading the value
// back directly from the original struct variable.
// Verifies pointer store propagates to the underlying memory.
//
// Expected stdout:
//   puti(node.value, 2) -> prints 99
// good

Node :: struct {
    value: int
}

node: Node
node.value = 0

ptr: Node* = ^node
ptr.value = 99

puti(node.value, 2)
