package sage

type Platform int

const (
	DARWIN Platform = iota
	LINUX
	WINDOWS
)

type Architecture int

const (
	ARM Architecture = iota
	X86
	X64
)

type OptLevel int

const (
	NONE OptLevel = iota
	ONE
	TWO
	THREE
)

type BuildSettings struct {
	Targetfile         string // apart of ir module header
	Executable_name    string
	Platform           Platform
	Architecture       Architecture
	Bitsize            int
	Optimization_level OptLevel
	Program_arguments  []string
	Argument_count	   int
}

func NewBuildSettings(filename string, executable_name string, platform Platform, arch Architecture, bitsize int) *BuildSettings {
	return &BuildSettings{
		Targetfile:         filename,
		Executable_name:    executable_name,
		Platform:           platform,
		Architecture:       arch,
		Bitsize:            bitsize,
		Optimization_level: NONE,
	}
}
