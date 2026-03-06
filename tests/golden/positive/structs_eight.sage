
Vector :: struct {
    zero: int
    one: int
}

Matrix :: struct {
    row_zero: Vector*
    row_one: Vector*
}

HyperDimMatrix :: struct {
    zero: Matrix
    one: Matrix
}

hyp: HyperDimMatrix
mat: Matrix

vec1: Vector
vec1.zero = 0
vec1.one = 1

vec2: Vector
vec2.zero = 2
vec2.one = 3

mat.row_zero = ^vec1
mat.row_one = ^vec2

hyp.zero = mat
hyp.one = mat


puti(mat.row_one.one, 1)

puti(hyp.zero.row_one.one, 1)
puti(hyp.one.row_one.one, 1)




