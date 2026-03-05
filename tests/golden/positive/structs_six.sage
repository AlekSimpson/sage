
null: int = 0

ListNode :: struct {
    data: string
    next: ListNode*
}

node3: ListNode
node3.data = "node3 data"

node2: ListNode
node2.data = "node2 data"
node2.next = ^node3

node1: ListNode
node1.data = "node1 data"
node1.next = ^node2


