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
	targetfile         string // apart of ir module header
	executable_name    string
	platform           Platform
	architecture       Architecture
	bitsize            int
	optimization_level OptLevel
}

func NewBuildSettings(filename string, executable_name string, platform Platform, arch Architecture, bitsize int) *BuildSettings {
	return &BuildSettings{
		targetfile:         filename,
		executable_name:    executable_name,
		platform:           platform,
		architecture:       arch,
		bitsize:            bitsize,
		optimization_level: NONE,
	}
}

func (bs *BuildSettings) set_optimization_level(level OptLevel) {
	bs.optimization_level = level
}
