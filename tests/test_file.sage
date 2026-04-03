// Integration: struct with a string field, where the string's .length is used
// inside a function that takes the field value as a parameter.
//
// Expected stdout:
//   puti(len, 2) -> prints 5  ("Alice")
//   puts(name, len)  -> prints Alice
// good

get_length :: (s: string) -> int {
    ret s.length
}

Person :: struct {
    name: string
    age: int
}

p: Person
p.name = "Alice"
p.age = 30

len: int = get_length(p.name)
puti(len, 1)
puts(p.name, len)
