

NamedScalar :: struct {
    name: string
    magnitude: int
}

value: NamedScalar
value.name = "some value"
value.magnitude = 39

len_value: int* = ^value.name.length


puti(@len_value, 2)

