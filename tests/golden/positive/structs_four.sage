

NamedVector2 :: struct {
    x: int
    y: int
    name: string
}

value: NamedVector2
value.x = 4
value.y = 2
value.name = "some vector"

puti(value.name.length, 2)
puts(value.name.bytes, value.name.length)


