// Tests that global-scope code runs before main is called, and that main
// can read values set up at global scope.
//
// Expected stdout:
//   puts(msg, msg.length) -> prints "setup"
//   puts(msg2, msg2.length) -> prints "main"
// good but add this one last, it may not follow language semantics actually double check existing main tests

msg: string = "setup"
puts(msg, msg.length)

main :: () {
    msg2: string = "main"
    puts(msg2, msg2.length)
}

main()
