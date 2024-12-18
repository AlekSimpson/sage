package sage

type Queueable interface {
	string | int | byte | Token
}

type Queue[Q Queueable] struct {
	array      []Q
	null_value Q
}

func NewEmptyQueue[Q Queueable](nullval Q) *Queue[Q] {
	return &Queue[Q]{
		array:      []Q{},
		null_value: nullval,
	}
}

func NewQueue[Q Queueable](arr []Q, nullvalue Q) *Queue[Q] {
	return &Queue[Q]{
		array:      arr,
		null_value: nullvalue,
	}
}

func (q *Queue[Q]) push(value Q) {
	q.array = append(q.array, value)
}

func (q *Queue[Q]) stack(value Q) {
	// Append the value to the beginning of the array like a stack
	q.array = append([]Q{value}, q.array...)
}

func (q *Queue[Q]) pop() Q {
	if len(q.array) == 0 {
		return q.null_value
	} else if len(q.array) == 1 {
		retval := q.array[0]
		q.array = make([]Q, 0)
		return retval
	}

	retval := q.array[0]
	q.array = q.array[1:]
	return retval
}

func (q *Queue[Q]) empty() bool {
	return len(q.array) == 0
}
