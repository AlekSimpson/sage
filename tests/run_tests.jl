
using JSON3, StructTypes

const COMPILER = get(ENV, "COMPILER", "./build/bin/sage")
const TIMEOUT  = parse(Float64, get(ENV, "TEST_TIMEOUT", "10.0"))
const UPDATE   = "--update" in ARGS
const CREATE   = "--create" in ARGS

const SIGNAL_NAMES = Dict(
    4  => "SIGILL (illegal instruction)",
    6  => "SIGABRT (abort)",
    7  => "SIGBUS (bus error)",
    8  => "SIGFPE (floating point exception)",
    9  => "SIGKILL (killed)",
    11 => "SIGSEGV (segmentation fault)",
    15 => "SIGTERM (terminated)",
)

const DISABLED_TESTS = ["functions_seven.sage"]

struct CompilerResult
    exit_code::Int
    signal::Int
    stdout::String
    stderr::String
    timed_out::Bool
end

struct TestResult
    name::String
    passed::Bool
    detail::String
    compiler_stdout::String
    compiler_stderr::String
end

struct CompilerTest
    index::Int
    name::String
    source::String
    category::String
    expected_exit_code::Int
    expected_bytecode::String
    expected_stdout::String
end

function run_compiler(source_path::String)
    out = Pipe()
    err = Pipe()
    proc = run(pipeline(`$COMPILER --emit-bytecode $source_path`, stdout=out, stderr=err), wait=false)
    close(out.in)
    close(err.in)

    timed_out = Ref(false)
    timer = Timer(TIMEOUT) do _
        timed_out[] = true
        kill(proc)
        sleep(2.0)
        process_running(proc) && kill(proc, Base.SIGKILL)
    end

    stdout_str = @async read(out, String)
    stderr_str = @async read(err, String)
    wait(proc)
    close(timer)

    return CompilerResult(
        proc.exitcode,
        proc.termsignal,
        fetch(stdout_str),
        fetch(stderr_str),
        timed_out[],
    )
end

function compute_diff(expected::String, actual::String)
    exp_file = tempname()
    act_file = tempname()
    write(exp_file, expected)
    write(act_file, actual)
    diff_buf = IOBuffer()
    proc = run(pipeline(`delta --paging=never --hunk-header-style=omit $exp_file $act_file`, stdout=diff_buf, stderr=devnull), wait=false)
    wait(proc)
    rm(exp_file, force=true)
    rm(act_file, force=true)
    return String(take!(diff_buf))
end

function signal_name(sig::Int)
    return get(SIGNAL_NAMES, sig, "signal $sig")
end

function test_did_crash(result::CompilerResult)
    if result.timed_out
        return (true, "compiler timed out after $(TIMEOUT)s (possible infinite loop)")
    end
    if result.signal != 0
        return (true, "compiler crashed: $(signal_name(result.signal))")
    end
    return (false, "")
end

function run_test(test::CompilerTest)
    write("./tests/test_file.sage", test.source)
    result = run_compiler("./tests/test_file.sage")
    bytecode_output = read(".sage/bytecode/s.asm", String)

    result_tuple = test_did_crash(result)
    did_crash = result_tuple[0]
    if did_crash
        crashout_message = result_tuple[1]
        return TestResult(test.name, false, crashout_message, result.stdout, result.stderr)
    end

    if result.exit_code != test.expected_exit_code
        detail = "exit code: expected $(test.expected_exit_code), got $(result.exit_code)"
        return TestResult(test.name, false, detail, result.stdout, result.stderr)
    end

    if UPDATE
        # update json file expected bytecode with
    end

    if test.expected_bytecode == actual_output && test.expected_stdout == result.stdout
        return TestResult(test.name, true, "", result.stdout, result.stderr)
    end

    diff = compute_diff(test.expected_bytecode, actual_output)
    return TestResult(test.name, false, diff, result.stdout, result.stderr)
end

function load_tests()
    StructTypes.StructType(::Type{CompilerTest}) = StructTypes.Struct()
    json_data = read("./tests/tests.json", String)
    tests = JSON3.read(json_data, Vector{CompilerTest})
    sort!(tests, by=first)
    return tests
end

function print_output_block(label::String, content::String)
    isempty(strip(content)) && return
    printstyled("    $label:\n", color=:cyan)
    for line in split(rstrip(content), '\n')
        println("      ", line)
    end
end

function report_results(results::Vector{TestResult})
    failed = filter(r -> !r.passed, results)
    passed_results = filter(r -> r.passed, results)

    for r in passed_results
        printstyled("  ✓ ", color=:green, bold=true)
        println(r.name)
    end

    for test in DISABLED_TESTS
        printstyled("  ~ ", color=:yellow, bold=true)
        println(test)
    end

    if !isempty(passed_results) && !isempty(failed)
        println()
    end

    for r in failed
        printstyled("  ✗ ", color=:red, bold=true)
        println(r.name)
        printstyled("    reason: ", color=:yellow)
        println(r.detail)
        print_output_block("stdout", r.compiler_stdout)
        print_output_block("stderr", r.compiler_stderr)
        println()
    end

    println(repeat('-', 60))
    summary_color = isempty(failed) ? :green : :red
    total_test_length = length(results) + length(DISABLED_TESTS)
    printstyled("$(length(passed_results))/$(total_test_length) passed", color=summary_color, bold=true)
    if !isempty(failed)
        printstyled(", $(length(failed)) failed", color=:red, bold=true)
    end
    if !isempty(DISABLED_TESTS)
        print("  |  ")
        printstyled("$(length(DISABLED_TESTS)) disabled.", color=:yellow, bold=true)
    end
    println()

    exit(isempty(failed) ? 0 : 1)
end

function main()
    tests = load_tests()
    active_tests = filter(test -> !(test.name in DISABLED_TESTS), tests)

    if isempty(tests)
        printstyled("no tests found.\n", color=:yellow)
        exit(0)
    end
    results = [run_test(test) for test in active_tests]
    report_results(results)
end

main()
