const std = @import("std");
const testing = std.testing;

pub fn add(a: i32, b: i32) i32 {
    return a + b;
}

pub fn general_add(comptime T: type, a: T, b: T) T {
    return a + b;
}

test "basic add functionality" {
    try testing.expect(add(3, 7) == 10);
}
