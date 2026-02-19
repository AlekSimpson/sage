


integer: int = 5
int_pointer: int* = ^integer

int_copy: int = @int_pointer
puti(int_copy, 1)

