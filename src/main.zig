const std = @import("std");
const alloc = @import("allocator.zig");
const root = @import("root.zig");
const lexer = @import("lexer.zig");

pub fn main() !void {
    defer {
        const leaked = alloc.deinit();
        if (leaked) {
            std.debug.print("Memory leak detected!\n", .{});
        }
    }
}

test "simple test" {
    var list = std.ArrayList(i32).init(std.testing.allocator);
    defer list.deinit(); // try commenting this out and see if zig detects the memory leak!
    try list.append(42);
    try std.testing.expectEqual(@as(i32, 42), list.pop());
}
