package sage

import (
	"log"
	// "fmt"
)

type SageError struct {
	title string
	message string
	reason string
	possible_solutions string
	documentation_links []string // TODO:
	// TODO: 
}

func NewSageError(title string, message string, reason string, solutions string) *SageError {
	return &SageError{
		title: title,
		message: message,
		reason: reason,
		possible_solutions: solutions,
	}
}

func (se *SageError) RaiseError() {
	log.Fatalf("%s:\n%s\n%s\n%s\n", se.title, se.message, se.reason, se.possible_solutions)
}
