const COMPILER = get(ENV, "COMPILER", "./build/bin/sage")
const TEST_DIR = get(ENV, "TEST_DIR", "tests/golden")
const TIMEOUT  = parse(Float64, get(ENV, "TEST_TIMEOUT", "10.0"))
const UPDATE   = "--update" in ARGS

const SIGNAL_NAMES = Dict(
    4  => "SIGILL (illegal instruction)",
    6  => "SIGABRT (abort)",
    7  => "SIGBUS (bus error)",
    8  => "SIGFPE (floating point exception)",
    9  => "SIGKILL (killed)",
    11 => "SIGSEGV (segmentation fault)",
    15 => "SIGTERM (terminated)",
)

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

    CompilerResult(
        proc.exitcode,
        proc.termsignal,
        fetch(stdout_str),
        fetch(stderr_str),
        timed_out[],
    )
end

function expected_exit_code(category::String)
    category == "positive" && return 0
    category == "negative" && return 1
    return 0
end

function relevant_output(result::CompilerResult, category::String)
    if category == "negative"
        return result.stderr
    elseif category == "warnings"
        return result.stderr * result.stdout
    else
        return read(".sage/bytecode/s.asm", String)
        # return result.stdout
    end
end

function compute_diff(expected::String, actual::String)
    exp_file = tempname()
    act_file = tempname()
    write(exp_file, expected)
    write(act_file, actual)
    diff_buf = IOBuffer()
    proc = run(pipeline(`diff --unified $exp_file $act_file`, stdout=diff_buf, stderr=devnull), wait=false)
    wait(proc)
    rm(exp_file, force=true)
    rm(act_file, force=true)
    String(take!(diff_buf))
end

function signal_name(sig::Int)
    get(SIGNAL_NAMES, sig, "signal $sig")
end

function check_crash(result::CompilerResult)
    if result.timed_out
        return "compiler timed out after $(TIMEOUT)s (possible infinite loop)"
    end
    if result.signal != 0
        return "compiler crashed: $(signal_name(result.signal))"
    end
    nothing
end

function run_test(source::String, category::String)
    base_path = dirname(source) * "/expected"
    target_filename = split(basename(source), ".")[1]
    expected_file = base_path * "/" * target_filename

    name = relpath(source, TEST_DIR)

    result = run_compiler(source)

    crash = check_crash(result)
    if !isnothing(crash)
        return TestResult(name, false, crash, result.stdout, result.stderr)
    end

    if result.exit_code != expected_exit_code(category)
        detail = "exit code: expected $(expected_exit_code(category)), got $(result.exit_code)"
        return TestResult(name, false, detail, result.stdout, result.stderr)
    end

    actual = relevant_output(result, category)

    if UPDATE
        write(expected_file, actual)
        return TestResult(name, true, "", result.stdout, result.stderr)
    end

    if !isfile(expected_file)
        return TestResult(name, false, "missing expected file: $expected_file", result.stdout, result.stderr)
    end

    expected = read(expected_file, String)
    if expected == actual
        return TestResult(name, true, "", result.stdout, result.stderr)
    end

    diff = compute_diff(expected, actual)
    TestResult(name, false, diff, result.stdout, result.stderr)
end

function discover_tests()
    tests = Tuple{String,String}[]
    for category in readdir(TEST_DIR)
        cat_dir = joinpath(TEST_DIR, category)
        if !isdir(cat_dir)
            continue
        end

        for f in readdir(cat_dir)
            if isdir(joinpath(cat_dir, f))
                continue
            end

            push!(tests, (joinpath(cat_dir, f), category))
        end
    end
    sort!(tests, by=first)
    tests
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
    printstyled("$(length(passed_results))/$(length(results)) passed", color=summary_color, bold=true)
    if !isempty(failed)
        printstyled(", $(length(failed)) failed", color=:red, bold=true)
    end
    println()

    exit(isempty(failed) ? 0 : 1)
end

function main()
    tests = discover_tests()
    if isempty(tests)
        printstyled("no tests found in $TEST_DIR\n", color=:yellow)
        exit(0)
    end
    results = [run_test(src, cat) for (src, cat) in tests]
    report_results(results)
end

main()
