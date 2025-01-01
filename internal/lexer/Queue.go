package sage

type RegisterPair struct {
	Register         string
	Associated_value string
}

func NewRegisterPair(register string, value string) RegisterPair {
	return RegisterPair{
		Register:         register,
		Associated_value: value,
	}
}

type Queueable interface {
	string | int | byte | Token | RegisterPair
}

type Queue[Q Queueable] struct {
	array []Q
}

func NewEmptyQueue[Q Queueable]() *Queue[Q] {
	return &Queue[Q]{
		array: []Q{},
	}
}

func NewQueue[Q Queueable](arr []Q) *Queue[Q] {
	return &Queue[Q]{
		array: arr,
	}
}

func (q *Queue[Q]) push(value Q) {
	q.array = append(q.array, value)
}

func (q *Queue[Q]) Stack(value Q) {
	// Append the value to the beginning of the array like a stack
	q.array = append([]Q{value}, q.array...)
}

func (q *Queue[Q]) Pop() *Q {
	if len(q.array) == 0 {
		return nil
	} else if len(q.array) == 1 {
		retval := q.array[0]
		q.array = make([]Q, 0)
		return &retval
	}

	retval := q.array[0]
	q.array = q.array[1:]
	return &retval
}

func (q *Queue[Q]) empty() bool {
	return len(q.array) == 0
}
