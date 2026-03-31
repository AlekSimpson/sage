// Tests an array of structs where the s  ✓ structs_nine
  ✓ structs_fourteen
  ✓ arrays_eleventruct was returned by a factory function.
// Combines struct constructor functions with struct array element assignment.
//
// Expected stdout:
//   puti(items[0].id, 1) -> prints 10
//   puti(items[1].id, 1) -> prints 20
// good

Item :: struct {
    id: i64
}

make_item :: (id: i64) -> Item {
    it: Item
    it.id = id
    ret it
}

items: Item[2]
items[0] = make_item(10)
items[1] = make_item(20)

puti(items[0].id, 1)
puti(items[1].id, 1)
